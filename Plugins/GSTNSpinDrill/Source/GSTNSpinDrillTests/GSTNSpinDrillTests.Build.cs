using UnrealBuildTool;

// Spin and drill, test module (spin-drill.md 4, 11): harness helpers (UGSTN_SpinDrillTestLibrary, the drill immunity effect).
// The plugin descriptor excludes it from Shipping (TargetConfigurationDenyList), and nothing may depend on it. Unit
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
			"GameplayAbilities",
			"GameplayTags",
			"GSTNSpinDrillCore",
			"GSTNSpinDrillNP",
			"GSTNSpinDrillGAS",
			"GSTNSpinDrillPresentation"
		});
	}
}
