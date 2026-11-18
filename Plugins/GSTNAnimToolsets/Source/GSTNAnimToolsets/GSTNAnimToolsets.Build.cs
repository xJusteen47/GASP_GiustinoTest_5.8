using UnrealBuildTool;

// AI toolsets (ToolsetRegistry, editor MCP) for Animation Blueprints and Chooser Tables. Editor only: it edits graphs and assets
// and nothing in a game target may name it.
public class GSTNAnimToolsets : ModuleRules
{
	public GSTNAnimToolsets(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ToolsetCore",
			"ToolsetRegistry",
			"Chooser"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"BlueprintGraph",
			"AnimGraph",
			"AnimGraphRuntime",
			"ChooserEditor",
			"AssetTools",
			"AssetRegistry"
		});
	}
}
