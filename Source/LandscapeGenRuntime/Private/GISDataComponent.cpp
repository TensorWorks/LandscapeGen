#include "GISDataComponent.h"
#include "Landscape.h"

// Sets default values for this component's properties
UGISDataComponent::UGISDataComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UGISDataComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UGISDataComponent::SetGeoTransforms(GDALDatasetRef& DatasetRef)
{
	GeoTransform.AddZeroed(6);
	FMemory::Memcpy(GeoTransform.GetData(), GDALHelpers::GetGeoTransform(DatasetRef).Get(), 6 * sizeof(double));
	
	InvGeoTransform.AddZeroed(6);
	FMemory::Memcpy(InvGeoTransform.GetData(), GDALHelpers::GetInvertedGeoTransform(DatasetRef).Get(), 6 * sizeof(double));
}

FVector UGISDataComponent::GetWorldSpaceLocation(FVector2D GPSCoordinate)
{
	// Get Transform from lat long 
	FString WGS84_WKT = GDALHelpers::WktFromEPSG(4326);
	OGRCoordinateTransformationRef CoordTransform = GDALHelpers::CreateCoordinateTransform(WGS84_WKT, WKT);
	
	UE_LOG(LogTemp, Log, TEXT("coord %s"), *GPSCoordinate.ToString());
	
	UE_LOG(LogTemp, Log, TEXT("WKT:\n %s"), *WKT);	
	
	// LatLong to Projection
	FVector Projected;
	
	// Prior to GDAL 3.0, coordinate transformations expect WGS84 coordinates to be in (lon,lat) format instead of (lat,lon)
	// (See: https://gdal.org/tutorials/osr_api_tut.html#crs-and-axis-order)
	#if GDAL_VERSION_NUM < GDAL_COMPUTE_VERSION(3,0,0)
		GPSCoordinate = FVector2D(GPSCoordinate.Y, GPSCoordinate.X);
	#endif
	
	check(GDALHelpers::TransformCoordinate(CoordTransform, FVector(GPSCoordinate, 0), Projected))
	
	// Projection to pixel space
	FVector2D PixelLoc = GDALHelpers::ApplyGeoTransform(InvGeoTransform.GetData(), FVector2D(Projected));
	
	// Get Height at pixel from ALandscape
	ALandscape* parent = (ALandscape*)GetAttachmentRootActor();
	FVector out(PixelLoc.X, PixelLoc.Y, 0);
	
	FVector origin;
	FVector bounds;
	parent->GetActorBounds(true, origin, bounds);
	
	// Convert to normalized local space
	out /= FVector(NumPixelsX, NumPixelsY, 1.0);
	
	// Scale to Worldspace
	out *= 2.0 * bounds;
	
	// Offset by actor transform
	out += parent->GetActorLocation();
	
	UE_LOG(LogTemp, Log, TEXT("World Space %s, pixel %s, norm %s, parent_loc: %s, origin %s, bounds: %s"), *out.ToString(), *PixelLoc.ToString(), *(PixelLoc / FVector2D(NumPixelsX, NumPixelsY)).ToString(), *parent->GetActorLocation().ToString(), *origin.ToString(), *bounds.ToString());
	
	out.Z = parent->GetHeightAtLocation(out).Get(0);
	
	return out;
}

FVector2D UGISDataComponent::GetGPSLocation(const FVector& WorldSpaceCoordinate)
{
	ALandscape* parent = (ALandscape*)GetAttachmentRootActor();
	FVector origin;
	FVector bounds;
	
	parent->GetActorBounds(true, origin, bounds);
	
	// Get local position then normalize to pixel space
	FVector2D PixelCoordinate = FVector2D(parent->GetTransform().InverseTransformPosition(WorldSpaceCoordinate));
	
	// Normalize local position
	PixelCoordinate /= FVector2D(bounds * 2.0f);
	
	// Scale to pixel space
	PixelCoordinate *= FVector2D(NumPixelsX, NumPixelsY);

	// Convert PixelSpace to projection
	FVector2D Projection = GDALHelpers::ApplyGeoTransform(GeoTransform.GetData(), PixelCoordinate);
	
	// Get Transform to lat long 
	FString WGS84_WKT = GDALHelpers::WktFromEPSG(4326);
	OGRCoordinateTransformationRef CoordTransform = GDALHelpers::CreateCoordinateTransform(WKT, WGS84_WKT);
	
	FVector Out;
	check(GDALHelpers::TransformCoordinate(CoordTransform, FVector(Projection, 0), Out))
	
	// Prior to GDAL 3.0, coordinate transformations expect WGS84 coordinates to be in (lon,lat) format instead of (lat,lon)
	// (See: https://gdal.org/tutorials/osr_api_tut.html#crs-and-axis-order)
	#if GDAL_VERSION_NUM < GDAL_COMPUTE_VERSION(3,0,0)
		Out = FVector(Out.Y, Out.X, Out.Z);
	#endif
	
	return FVector2D(Out);
}
