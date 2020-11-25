using UnrealBuildTool;

public class LandscapeGenRuntime : ModuleRules
{
	public LandscapeGenRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Landscape",
				"UnrealGDAL",
				"GDAL"
			}
		);
	}
}
