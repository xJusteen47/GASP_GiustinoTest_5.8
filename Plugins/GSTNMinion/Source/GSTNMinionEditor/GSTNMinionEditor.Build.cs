using UnrealBuildTool;

// Minion editor tooling: the test brain tree (G8) and the Data Validation validators of definitions and waves (G12).
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
			"GameplayTags",
			// G12: UEditorValidatorBase, the validators of definitions and waves
			"DataValidation"
		});
	}
}
