#include "LandscapeGenerationBPFL.h"
#include "Landscape.h"
#include "LandscapeEdit.h"

#include "GDALHelpers.h"
#include "GISDataComponent.h"
#include "LandscapeConstraints.h"

#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"

#include "Factories/TextureFactory.h"

#include "Factories/MaterialFactoryNew.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionLandscapeLayerCoords.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"

#include <vector>

ALandscape* ULandscapeGenerationBPFL::GenerateLandscapeFromGISData(
	const UObject* WorldContext, const FString& LandscapeName, const FGISData& GISData, const FVector& Scale3D)
{
	// Check that we can allocate GISData in one texture
	if (
		GISData.HeightBufferX > LandscapeConstraints::MaxRasterSizeX() ||
		GISData.HeightBufferY > LandscapeConstraints::MaxRasterSizeY() ||
		GISData.ColorBufferX > LandscapeConstraints::MaxRasterSizeX() ||
		GISData.ColorBufferY > LandscapeConstraints::MaxRasterSizeY()
	) {
		UE_LOG(LogTemp, Log, TEXT("Textures too large to allocate in single landscape"));
		return nullptr;
	}
	
	auto ColorBuffer = GISData.ColorBuffer;
	
	// Textures need to be in BGRA format, so reorder the raster channels if the input data is in another format
	if (GISData.PixelFormat != EPixelFormat::PF_B8G8R8A8)
	{
		// Determine the correct mapping from source channel order to destination channel order
		// (Note that these are 1-indexed to remain consistent with the way GDAL 1-indexes raster bands)
		std::vector<uint32> ChannelMapping;
		switch (GISData.PixelFormat)
		{
		case EPixelFormat::PF_R8G8B8A8:
			ChannelMapping = {3,2,1,4};
			break;
		default:
			UE_LOG(LogTemp, Log, TEXT("Unsupported pixel format for colour data"));
			return nullptr;
		}
		
		mergetiff::RasterData<uint8> dataWrapper(ColorBuffer.GetData(), 4, GISData.ColorBufferY, GISData.ColorBufferX, true);
		GDALDatasetRef datasetWrapper = mergetiff::DatasetManagement::wrapRasterData(dataWrapper);
		TArray<uint8> remapped;
		mergetiff::RasterData<uint8> remappedRD = GDALHelpers::AllocateAndWrap<uint8>(remapped, 4, GISData.ColorBufferY, GISData.ColorBufferX, 255);
		
		if (mergetiff::RasterIO::readDataset(datasetWrapper, remappedRD, ChannelMapping) == false)
		{
			UE_LOG(LogTemp, Log, TEXT("Failed to remap channels for the colour data"));
			return nullptr;
		}
		
		ColorBuffer = remapped;
	}
	
	// Save colour texture to UAsset
	FString PackageName = TEXT("/Game/GISLandscapeData/");
	
	// Get unique asset name
	FString Name;
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(PackageName, FString::Printf(TEXT("T_%s_GISTexture"), *LandscapeName), PackageName, Name);
	
	// Create package
	UPackage* Package = CreatePackage(NULL, *PackageName);
	Package->FullyLoad();
	
	UTexture2D* ColorTexture;
	
	UTextureFactory* TextureFactory = NewObject<UTextureFactory>();
	TextureFactory->AddToRoot();
	
	ColorTexture = (UTexture2D*)TextureFactory->CreateTexture2D(
		Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
	
	if (ColorTexture)
	{
		ColorTexture->Source.Init(
			GISData.ColorBufferX,
			GISData.ColorBufferY,
			/*NumSlices=*/ 1,
			/*NumMips=*/ 1,
			ETextureSourceFormat::TSF_BGRA8,
			ColorBuffer.GetData()
		);
		ColorTexture->CompressionSettings = TC_Default;
		ColorTexture->LODGroup = TEXTUREGROUP_World;
		ColorTexture->MipGenSettings = TMGS_NoMipmaps;
	}
	
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(TextureFactory, ColorTexture);
	
	ColorTexture->PostEditChange();
	TextureFactory->RemoveFromRoot();
	
	FAssetRegistryModule::AssetCreated(ColorTexture);
	Package->SetDirtyFlag(true);
	
	// Setup heightmap for landscape
	TMap<FGuid, TArray<uint16>> HeightmapDataPerLayers;
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayer;
	
	mergetiff::RasterData<float> floatRasterData(const_cast<float*>(GISData.HeightBuffer.GetData()), 1, GISData.HeightBufferY, GISData.HeightBufferX, true);
	GDALDatasetRef floatDataset = mergetiff::DatasetManagement::datasetFromRaster(floatRasterData);
	
	// Get scale min and maxes in meters as float
	auto MinMax = GDALHelpers::ComputeRasterMinMax(floatDataset, 1);
	
	check(MinMax)
	
	// Convert meters in float to uint16 for Unreal while maximizing height sample resolution
	GDALDatasetRef uint16Dataset = GDALHelpers::Translate(floatDataset, GDALHelpers::UniqueMemFilename(),
		GDALHelpers::ParseGDALTranslateOptions({
			TEXT("-ot"),
			TEXT("UInt16"),
			TEXT("-scale"),
			FString::Printf(TEXT("%lf"), MinMax->Min),
			FString::Printf(TEXT("%lf"), MinMax->Max),
			FString::Printf(TEXT("%d"), 0),
			FString::Printf(TEXT("%d"), MAX_uint16)
			})
	);
	
	TArray<uint16> HeightGDAL;
	
	// Create a buffer to hold the heightmap data and wrap it in a RasterData object
	mergetiff::RasterData<uint16> heightmapData = GDALHelpers::AllocateAndWrap<uint16>(HeightGDAL, 1, GISData.HeightBufferY, GISData.HeightBufferX, 0);
	
	// Attempt to read the heightmap data into our buffer
	if (mergetiff::RasterIO::readDataset(uint16Dataset, heightmapData, { 1 }) == false) {
		UE_LOG(LogTemp, Log, TEXT("Failed to read the data from the heightmap"));
		return nullptr;
	}
	
	// Store corner coordinates projected if not already
	FVector2D UpperLeft = GISData.UpperLeft;
	FVector2D LowerRight = GISData.LowerRight;
	
	if (GISData.CornerType == ECornerCoordinateType::LatLon)
	{
		// Prior to GDAL 3.0, coordinate transformations expect WGS84 coordinates to be in (lon,lat) format instead of (lat,lon)
		// (See: https://gdal.org/tutorials/osr_api_tut.html#crs-and-axis-order)
		#if GDAL_VERSION_NUM < GDAL_COMPUTE_VERSION(3,0,0)
			UpperLeft = FVector2D(UpperLeft.Y, UpperLeft.X);
			LowerRight = FVector2D(LowerRight.Y, LowerRight.X);
		#endif
		
		FString WGS84_WKT = GDALHelpers::WktFromEPSG(4326);
		OGRCoordinateTransformationRef CoordTransform = GDALHelpers::CreateCoordinateTransform(WGS84_WKT, GISData.ProjectionWKT);
		
		FVector TempUL;
		check(GDALHelpers::TransformCoordinate(CoordTransform, FVector(UpperLeft, 0), TempUL));
		UpperLeft = FVector2D(TempUL);
		
		FVector TempLR;
		check(GDALHelpers::TransformCoordinate(CoordTransform, FVector(LowerRight, 0), TempLR));
		LowerRight = FVector2D(TempLR);
	}
	
	check(GDALHelpers::SetRasterCorners(uint16Dataset, UpperLeft, LowerRight))
	
	// Make the scale factor for X Y by calculating metres per pixel
	// Make Z scale factor as Unreals default heighmap range is -255cm to 255cm over a 0 to max_uint16 range
	FVector ScaleVector = Scale3D * 100 * FVector((LowerRight - UpperLeft).GetAbs() / FVector2D(GISData.HeightBufferX, GISData.HeightBufferY), (MinMax->Max - MinMax->Min) / 512.0);
	
	ALandscape* Landscape = WorldContext->GetWorld()->SpawnActor<ALandscape>();
	
	// Setup landscape configuration
	Landscape->ComponentSizeQuads = 255;
	Landscape->SubsectionSizeQuads = 255;
	Landscape->NumSubsections = 1;
	Landscape->SetLandscapeGuid(FGuid::NewGuid());
	
	// Build new material for landscape
	Landscape->LandscapeMaterial = GenerateUnlitLandscapeMaterial(LandscapeName, FString::Printf(TEXT("%s.%s"), *PackageName, *Name), FMath::CeilToInt(GISData.HeightBufferX / 255), FMath::CeilToInt(GISData.HeightBufferY / 255), 255);
	
	Landscape->CreateLandscapeInfo();
	Landscape->SetActorTransform(FTransform(FQuat::Identity, FVector(), ScaleVector));
	
	// Generate LandscapeActor from heightmap
	HeightmapDataPerLayers.Add(FGuid(), HeightGDAL);
	MaterialLayerDataPerLayer.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());
	
	// Build in engine only function for taking height buffer and generating landscape components
	Landscape->Import(Landscape->GetLandscapeGuid(), 0, 0, GISData.HeightBufferX - 1,
		GISData.HeightBufferY - 1, Landscape->NumSubsections, Landscape->SubsectionSizeQuads, HeightmapDataPerLayers,
		TEXT("NONE"), MaterialLayerDataPerLayer, ELandscapeImportAlphamapType::Layered
	);
	
	// Translate Landscape so that lowest point is 0 in WorldSpace
	FVector LandscapeOrigin;
	FVector LandscapeBounds;
	Landscape->GetActorBounds(false, LandscapeOrigin, LandscapeBounds);
	Landscape->SetActorLocation(FVector(0, 0, LandscapeBounds.Z));
	
	// Create attach and register GISDataComponent
	UGISDataComponent* GISDataComponent = NewObject<UGISDataComponent>(Landscape, NAME_None, RF_Transactional);
	GISDataComponent = (UGISDataComponent*)Landscape->CreateComponentFromTemplate(GISDataComponent, NAME_None);
	GISDataComponent->SetupAttachment(Landscape->GetRootComponent(), NAME_None);
	Landscape->ReregisterAllComponents();
	
	// Get last components to fill out useful information
	ULandscapeComponent* lastComponent = Landscape->LandscapeComponents.Last();
	GISDataComponent->NumComponentsX = lastComponent->SectionBaseX / lastComponent->ComponentSizeQuads + 1;
	GISDataComponent->NumComponentsY = lastComponent->SectionBaseY / lastComponent->ComponentSizeQuads + 1;
	GISDataComponent->ComponentSizeQuads = lastComponent->ComponentSizeQuads;
	
	// Fill GIS data into the component
	GISDataComponent->UpperLeft = UpperLeft;
	GISDataComponent->LowerRight = LowerRight;
	GISDataComponent->SetGeoTransforms(uint16Dataset);
	GISDataComponent->WKT = GISData.ProjectionWKT;
	GISDataComponent->NumPixelsX = GISData.HeightBufferX;
	GISDataComponent->NumPixelsY = GISData.HeightBufferY;
	
	Landscape->CreateLandscapeInfo();
	Landscape->SetActorLabel(LandscapeName);
	
	return Landscape;
}

UMaterial* ULandscapeGenerationBPFL::GenerateUnlitLandscapeMaterial(const FString& LandscapeName,
	const FString& TexturePath, const int32& NumComponentsX, const int32& NumComponentsY, const int32& NumQuads)
{
	FString PackageName = "/Game/GISLandscapeData/";
	
	// Get unique name
	FString Name;
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(PackageName, FString::Printf(TEXT("M_%s_Unlit"), *LandscapeName), PackageName, Name);
	
	UMaterial* UnrealMaterial;
	
	// Create package
	UPackage* Package = CreatePackage(NULL, *PackageName);
	Package->FullyLoad();
	
	// Create an unreal material asset
	auto MaterialFactory = NewObject<UMaterialFactoryNew>();
	UnrealMaterial = (UMaterial*)MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), Package, *Name, RF_Standalone | RF_Public, NULL, GWarn);
	
	UnrealMaterial->SetShadingModel(EMaterialShadingModel::MSM_Unlit);
	
	auto NumCompsX = NewObject<UMaterialExpressionScalarParameter>(UnrealMaterial);
	NumCompsX->DefaultValue = NumComponentsX;
	NumCompsX->ParameterName = FName("NumComponentsX");
	
	auto NumCompsY = NewObject<UMaterialExpressionScalarParameter>(UnrealMaterial);
	NumCompsY->DefaultValue = NumComponentsY;
	NumCompsY->ParameterName = FName("NumComponentsY");
	
	auto Append = NewObject<UMaterialExpressionAppendVector>(UnrealMaterial);
	Append->A.Connect(0, NumCompsX);
	Append->B.Connect(0, NumCompsY);
	
	auto NumQuadsParam = NewObject<UMaterialExpressionScalarParameter>(UnrealMaterial);
	NumQuadsParam->DefaultValue = NumQuads;
	NumQuadsParam->ParameterName = FName("NumQuads");
	
	auto Multiply = NewObject<UMaterialExpressionMultiply>(UnrealMaterial);
	Multiply->A.Connect(0, Append);
	Multiply->B.Connect(0, NumQuadsParam);
	
	auto LandscapeCoords = NewObject<UMaterialExpressionLandscapeLayerCoords>(UnrealMaterial);
	
	auto Divide = NewObject<UMaterialExpressionDivide>(UnrealMaterial);
	Divide->A.Connect(0, LandscapeCoords);
	Divide->B.Connect(0, Multiply);
	
	// Make texture sampler
	auto Texture = NewObject<UMaterialExpressionTextureSampleParameter2D>(UnrealMaterial);
	Texture->Texture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("Texture2D'%s'"), *TexturePath));
	Texture->ParameterName = FName("ColorMap");
	Texture->SamplerType = SAMPLERTYPE_Color;
	Texture->Coordinates.Connect(0, Divide);
	
	UnrealMaterial->Expressions.Add(Texture);
	UnrealMaterial->EmissiveColor.Expression = Texture;
	
	// let the material update itself if necessary
	UnrealMaterial->PreEditChange(NULL);
	UnrealMaterial->PostEditChange();
	
	UnrealMaterial->UpdateCachedExpressionData();
	
	FAssetRegistryModule::AssetCreated(UnrealMaterial);
	Package->SetDirtyFlag(true);
	
	return UnrealMaterial;
}
