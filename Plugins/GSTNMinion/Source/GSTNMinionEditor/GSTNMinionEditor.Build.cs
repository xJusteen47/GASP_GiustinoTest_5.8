using UnrealBuildTool;

// Minion editor tooling (definition and registry validators, wave details: M8 G12). Empty shell in G2.
public class GSTNMinionEditor : ModuleRules
{
	public GSTNMinionEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GSTNMinionCore",
			"GSTNMinionRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			// G8: builds the test brain tree in code and compiles it
			"StateTreeModule",
			"StateTreeEditorModule",
			"PropertyBindingUtils",
			"GameplayTags"
		});
	}
}
