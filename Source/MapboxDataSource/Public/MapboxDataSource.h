#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "GISDataSource.h"
#include "UObject/NoExportTypes.h"
#include "MapboxDataSource.generated.h"

enum class EMapboxRequestDataType
{
	RGB,
	HEIGHT
};

struct FMapboxRequestData
{
	EMapboxRequestDataType DataType;
	int RelX;
	int RelY;
};

UCLASS(Blueprintable)
class MAPBOXDATASOURCE_API UMapboxDataSource : public UObject, public IGISDataSource
{
	GENERATED_BODY()
	
public:
	const static FString ProjectionWKT;
	
	virtual void RetrieveData(FGISDataSourceDelegate OnSuccess, FGISDataSourceDelegate OnFailure);
	
	void RequestSectionRGBHeight(float upperLat, float leftLon, float lowerLat, float rightLon, int zoom);
	
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = "true"))
	FString reqAPIKey;
	
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = "true"))
	float reqUpperLat;
	
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = "true"))
	float reqLeftLon;
	
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = "true"))
	float reqLowerLat;
	
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = "true"))
	float reqRightLon;
	
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = "true"))
	int reqZoom;

	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = "true"))
	bool bIgnoreMissingTiles;
	
	FGISDataSourceDelegate OnSuccess;
	
	FGISDataSourceDelegate OnFailure;
	
	void Start(FString URL, FMapboxRequestData data);
	
	TArray<float> HeightData;
	TArray<FColor> RGBData;
	
private:
	bool hasReqFailed = false;
	
	int DimX;
	int DimY;
	int TileDimX;
	int TileDimY;
	int OffsetX;
	int OffsetY;
	int Zoom;
	int MaxX;
	int MaxY;
	float MinU;
	float MaxU;
	float MinV;
	float MaxV;
	int TotalRequests;
	int CompletedRequests;
	int NumXHeightPixels;
	int NumYHeightPixels;
	
	bool ValidateRequest(FString& OutError);
	void HandleMapboxRequest(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, FMapboxRequestData data);
};
