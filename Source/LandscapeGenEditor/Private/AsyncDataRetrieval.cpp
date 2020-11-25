#include "AsyncDataRetrieval.h"

UAsyncDataRetrieval* UAsyncDataRetrieval::RetrieveDataFromSource(const TScriptInterface<IGISDataSource>& DataSource)
{
	UAsyncDataRetrieval* retriever = NewObject<UAsyncDataRetrieval>();
	retriever->DataSource = DataSource;
	return retriever;
}

void UAsyncDataRetrieval::Activate() {
	this->DataSource->RetrieveData(this->OnSuccess, this->OnFailure);
}

UAsyncDataRetrieval::UAsyncDataRetrieval(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if ( HasAnyFlags(RF_ClassDefaultObject) == false )
	{
		AddToRoot();
	}
}