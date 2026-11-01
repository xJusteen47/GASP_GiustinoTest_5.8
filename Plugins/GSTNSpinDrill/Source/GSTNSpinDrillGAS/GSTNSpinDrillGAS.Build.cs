using UnrealBuildTool;

// Spin and drill, ability-system half (spin-drill.md 3, 4). Will hold the five abilities, the tuning data asset and the kit
// extension; empty in the skeleton (plan D1). Allowed edges: engine, GSTNCombatGAS, GSTNSpinDrillCore and GSTNSpinDrillNP (the abilities
// read the drill fragment). It must never depend on GSTNParts, GSTNLevelObjects, an Editor module or the project module.
public class GSTNSpinDrillGAS : ModuleRules
{
	public GSTNSpinDrillGAS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNSpinDrillCore",
			"GSTNSpinDrillNP"
		});
	}
}
