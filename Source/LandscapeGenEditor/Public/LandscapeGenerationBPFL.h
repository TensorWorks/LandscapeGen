#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GISData.h"
#include "LandscapeGenerationBPFL.generated.h"

UCLASS()
class LANDSCAPEGENEDITOR_API ULandscapeGenerationBPFL : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	UFUNCTION(BlueprintCallable, Category = "LandscapeGen|Single")
	static ALandscape* GenerateLandscapeFromGISData(
		const UObject* WorldContext, const FString& LandscapeName, const FGISData& GISData, const FVector& Scale3D
	);
	
	UFUNCTION(BlueprintCallable, Category = "LandscapeGen|Utils")
	static UMaterial* GenerateUnlitLandscapeMaterial(
		const FString& LandscapeName, const FString& TexturePath, const int32& NumComponentsX,
		const int32& NumComponentsY, const int32& NumQuads
	);
};
