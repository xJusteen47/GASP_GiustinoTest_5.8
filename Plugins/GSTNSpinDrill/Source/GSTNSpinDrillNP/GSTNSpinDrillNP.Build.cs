using UnrealBuildTool;

// Spin and drill, Network Prediction half (spin-drill.md 4, 7). Will hold the drill sync fragment, the combat tick extension,
// the snapshot extension and the latched movement mode; empty in the skeleton (plan D1). Allowed edges: engine, the NP fork (NetworkPrediction,
// Mover), GSTNCombatNP (the combat model hosting the fragment), GSTNCombatCore, GSTNInputCore and GSTNSpinDrillCore. It must never
// depend on GSTNParts, GSTNLevelObjects, an Editor module or the project module.
public class GSTNSpinDrillNP : ModuleRules
{
	public GSTNSpinDrillNP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"NetworkPrediction",
			"Mover",
			"GSTNInputCore",
			"GSTNCombatCore",
			"GSTNCombatNP",
			"GSTNSpinDrillCore"
		});
	}
}
