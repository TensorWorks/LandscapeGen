using UnrealBuildTool;

public class MapboxDataSource : ModuleRules
{
	public MapboxDataSource(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Http",
				"Json",
				"JsonUtilities",
				"RHI",
				"RenderCore",
				"LandscapeGenEditor",
				"UnrealGDAL",
				"GDAL"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore"
			}
		);
	}
}
