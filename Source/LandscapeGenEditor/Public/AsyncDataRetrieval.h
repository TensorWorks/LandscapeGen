#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "GISDataSource.h"
#include "AsyncDataRetrieval.generated.h"

UCLASS(Blueprintable)
class LANDSCAPEGENEDITOR_API UAsyncDataRetrieval : public UBlueprintAsyncActionBase
{
	GENERATED_UCLASS_BODY()
	
public:
	
	UFUNCTION(BlueprintCallable, meta=( BlueprintInternalUseOnly="true" ))
	static UAsyncDataRetrieval* RetrieveDataFromSource(const TScriptInterface<IGISDataSource>& DataSource);
	
	UPROPERTY(BlueprintAssignable)
	FGISDataSourceDelegate OnSuccess;
	
	UPROPERTY(BlueprintAssignable)
	FGISDataSourceDelegate OnFailure;
	
	virtual void Activate();
	
private:
	TScriptInterface<IGISDataSource> DataSource;
};
