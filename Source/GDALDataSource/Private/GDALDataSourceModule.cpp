#include "GDALDataSourceModule.h"
#include "UnrealGDAL.h"

#define LOCTEXT_NAMESPACE "FGDALDataSourceModule"

void FGDALDataSourceModule::StartupModule()
{
	FUnrealGDALModule* UnrealGDAL = FModuleManager::Get().LoadModulePtr<FUnrealGDALModule>("UnrealGDAL");
	UnrealGDAL->InitGDAL();
}

void FGDALDataSourceModule::ShutdownModule() {}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGDALDataSourceModule, GDALDataSource)