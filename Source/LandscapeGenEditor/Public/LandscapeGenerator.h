#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GISData.h"
#include "LandscapeGenerator.generated.h"

UCLASS()
class LANDSCAPEGENEDITOR_API ULandscapeGenerator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	public:
		
		UFUNCTION(BlueprintCallable, Category = "LandscapeGenerator|Single")
		static ALandscape* GenerateLandscapeFromGISData(const UObject* WorldContext, const FGISData& GISData, const FVector& Scale3D);
};
