using UnrealBuildTool;

// Spin and drill, test module (spin-drill.md 4, 11). Will hold the harness abilities and test content names; empty in the
// skeleton (plan D1). The plugin descriptor excludes it from Shipping (TargetConfigurationDenyList), and nothing may depend on it. Unit
// tests of each module stay in that module's Private/Tests (GSTNSpinDrill.Core.*...), as in the other GSTN plugins.
public class GSTNSpinDrillTests : ModuleRules
{
	public GSTNSpinDrillTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNSpinDrillCore",
			"GSTNSpinDrillNP",
			"GSTNSpinDrillGAS",
			"GSTNSpinDrillPresentation"
		});
	}
}
