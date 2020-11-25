#pragma once

#include "CoreMinimal.h"
#include "GISDataSource.h"
#include "GDALDataSource.generated.h"

UCLASS(Blueprintable)
class GDALDATASOURCE_API UGDALDataSource : public UObject, public IGISDataSource
{
	GENERATED_BODY()
	
	public:
		
		// Attempts to retrieve the GIS data from the specified heightmap and RGB datasets
		virtual void RetrieveData(FGISDataSourceDelegate OnSuccess, FGISDataSourceDelegate OnFailure);
		
		// The path to the GDAL raster dataset containing the heightmap data
		UPROPERTY(BlueprintReadWrite, meta=(ExposeOnSpawn="true"))
		FString HeightmapDataset;
		
		// The path to the GDAL raster dataset containing the RGB data
		UPROPERTY(BlueprintReadWrite, meta=(ExposeOnSpawn="true"))
		FString RGBDataset;
		
	private:
		FString RetrieveDataInternal(FGISData& data);
};
