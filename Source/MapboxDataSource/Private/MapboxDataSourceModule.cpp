#include "MapboxDataSourceModule.h"
#include "UnrealGDAL.h"

#define LOCTEXT_NAMESPACE "FMapboxDataSourceModule"

void FMapboxDataSourceModule::StartupModule()
{
	FUnrealGDALModule* UnrealGDAL = FModuleManager::Get().LoadModulePtr<FUnrealGDALModule>("UnrealGDAL");
	UnrealGDAL->InitGDAL();
}

void FMapboxDataSourceModule::ShutdownModule() {}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMapboxDataSourceModule, MapboxDataSource)