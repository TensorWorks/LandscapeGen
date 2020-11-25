#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GDALHelpers.h"
#include "GISDataComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LANDSCAPEGENRUNTIME_API UGISDataComponent : public USceneComponent
{
	GENERATED_BODY()
	
public:
	
	// Sets default values for this component's properties
	UGISDataComponent();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int NumComponentsX;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int NumComponentsY;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int ComponentSizeQuads;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int NumPixelsX;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int NumPixelsY;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector2D UpperLeft;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector2D LowerRight;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString WKT;
	
protected:
	
	// Called when the game starts
	virtual void BeginPlay() override;
	
public:
	void SetGeoTransforms(GDALDatasetRef& GPSCoordinate);
	
	UFUNCTION(BlueprintCallable, CallInEditor)
	FVector GetWorldSpaceLocation(FVector2D GPSCoordinate);
	
	UFUNCTION(BlueprintCallable, CallInEditor)
	FVector2D GetGPSLocation(const FVector& WorldSpaceCoordinate);
	
private:
	UPROPERTY()
	TArray<double> GeoTransform;
	
	UPROPERTY()
	TArray<double> InvGeoTransform;
};
