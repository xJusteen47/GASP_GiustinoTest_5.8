using UnrealBuildTool;

// Spin and drill, Network Prediction half (spin-drill.md 4, 7). Holds the drill sync fragment, the combat tick extension (D3), the tuning attach, the hop and the low gravity effect (D4); will add
// the snapshot extension and the latched movement mode. Allowed edges: engine, the NP fork (NetworkPrediction, Mover; GameplayAbilities and
// AbilitySystemSimulation for the tag projection and the effects), GSTNCombatNP (the combat model hosting the fragment), GSTNCombatGAS (the combat stats the low gravity scales), GSTNInputNP (the Mover sim state tracker), GSTNCombatCore, GSTNInputCore and GSTNSpinDrillCore. It must never
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
			"GameplayAbilities",
			"AbilitySystemSimulation",
			"NetworkPrediction",
			"Mover",
			"GSTNInputCore",
			"GSTNInputNP",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNCombatNP",
			"GSTNSpinDrillCore",
			"GSTNLinkCore",
			"GSTNLinkGAS",
			"GSTNLinkNP"
		});
	}
}
