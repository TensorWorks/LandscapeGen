#pragma once

#include "CoreMinimal.h"
#include "Math/Vector2D.h"
#include "GISData.generated.h"


UENUM(BlueprintType)
enum ECornerCoordinateType
{
	// Corner coordinates are WGS84 (lat,lon) values
	LatLon     UMETA(DisplayName = "LatLon"),
	
	// Corner coordinates are in the projected coordinate system of the raster data
	Projected  UMETA(DisplayName = "Projected"),
};


USTRUCT(BlueprintType)
struct LANDSCAPEGENEDITOR_API FGISData
{
	GENERATED_BODY()
	
	// The buffer of raw heightmap values (in metres) and the heightmap raster dimensions
	UPROPERTY(BlueprintReadWrite)
	TArray<float> HeightBuffer;
	uint32 HeightBufferX, HeightBufferY;
	
	// The buffer of colour values and the colour raster dimensions
	UPROPERTY(BlueprintReadWrite)
	TArray<uint8> ColorBuffer;
	uint32 ColorBufferX, ColorBufferY;
	
	UPROPERTY(BlueprintReadWrite)
	TEnumAsByte<EPixelFormat> PixelFormat = EPixelFormat::PF_B8G8R8A8;
	
	// The Well-Known Text (WKT) representation of the projected coordinate system used by the raster data
	UPROPERTY(BlueprintReadWrite)
	FString ProjectionWKT;
	
	// Specifies whether the corner coordinates are WGS84 (lat,lon) or projected coordinates
	UPROPERTY(BlueprintReadWrite)
	TEnumAsByte<ECornerCoordinateType> CornerType = ECornerCoordinateType::LatLon;
	
	// The coordinate of the upper-left corner of the raster data
	UPROPERTY(BlueprintReadWrite)
	FVector2D UpperLeft;
	
	// The coordinate of the lower-right corner of the raster data
	UPROPERTY(BlueprintReadWrite)
	FVector2D LowerRight;
};
