using UnrealBuildTool;

// Spin and drill, ability-system half (spin-drill.md 3, 4). Holds the tuning data asset, the kit extension and its loadout extension, and the
// abilities (the spin of D4; the cancel, dive, bore and launch follow). Allowed edges: engine, the NP fork (the abilities are UNpGameplayAbility
// children: GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, Mover), GSTNInputCore, GSTNInputNP, GSTNCombatCore, GSTNCombatGAS,
// GSTNCombatNP (UGSTN_GA_BaseCombat), GSTNSpinDrillCore and GSTNSpinDrillNP (the abilities read the drill fragment). It must never depend on
// GSTNParts, GSTNLevelObjects, an Editor module or the project module.
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
			"GSTNSpinDrillNP"
		});
	}
}
