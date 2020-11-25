#include "LandscapeGenRuntime.h"
#include "UnrealGDAL.h"

#define LOCTEXT_NAMESPACE "FLandscapeGenRuntimeModule"

void FLandscapeGenRuntimeModule::StartupModule()
{
	FUnrealGDALModule* UnrealGDAL = FModuleManager::Get().LoadModulePtr<FUnrealGDALModule>("UnrealGDAL");
	UnrealGDAL->InitGDAL();
}

void FLandscapeGenRuntimeModule::ShutdownModule() {}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLandscapeGenRuntimeModule, LandscapeGenRuntime)