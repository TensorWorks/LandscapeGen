#pragma once

#include "CoreMinimal.h"
#include "GISData.h"
#include "GISDataSource.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGISDataSourceDelegate, const FString&, Error, const FGISData&, Data);

UINTERFACE(Blueprintable)
class UGISDataSource : public UInterface
{
	GENERATED_BODY()
};

class LANDSCAPEGENEDITOR_API IGISDataSource
{
	GENERATED_BODY()
	
	public:
		
		virtual void RetrieveData(FGISDataSourceDelegate OnSuccess, FGISDataSourceDelegate OnFailure) = 0;
};
