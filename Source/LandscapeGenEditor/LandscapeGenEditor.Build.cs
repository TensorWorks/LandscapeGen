using UnrealBuildTool;

public class LandscapeGenEditor : ModuleRules
{
	public LandscapeGenEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"LandscapeGenRuntime"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Landscape",
				"Foliage",
				"UnrealEd",
				"RHI",
				"RenderCore",
				"UnrealGDAL",
				"GDAL"
			}
		);
	}
}
