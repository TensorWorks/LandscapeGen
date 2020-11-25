#include "LandscapeGeneration.h"
#include "UnrealGDAL.h"

#define LOCTEXT_NAMESPACE "FLandscapeGenerationModule"

void FLandscapeGenerationModule::StartupModule()
{
	FUnrealGDALModule* UnrealGDAL = FModuleManager::Get().LoadModulePtr<FUnrealGDALModule>("UnrealGDAL");
	UnrealGDAL->InitGDAL();
}

void FLandscapeGenerationModule::ShutdownModule() {}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLandscapeGenerationModule, LandscapeGenEditor)