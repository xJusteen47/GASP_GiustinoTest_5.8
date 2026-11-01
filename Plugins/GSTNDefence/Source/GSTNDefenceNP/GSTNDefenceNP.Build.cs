using UnrealBuildTool;

// Defence, Network Prediction half (m6 spec 1b, plan F7/F8). The sync fragment, the snapshot contributor (the shield bubble as an extra
// hurtbox), the tick extension (clocks, hold stall, parry window stamps, tag projection), the Parry and Shield steps, the outcome applier,
// the punish inbox kind and the ability classes (UGSTN_GA_BaseCombat of GSTNCombatNP is their base, which is why they are not in the GAS
// module). Allowed edges: engine, the NP fork (NetworkPrediction, AbilitySystemSimulation for the UNpGameplayAbility base, Mover for the moves), GSTNCombatNP (the combat NP model that hosts the fragments),
// GSTNCombatCore, GSTNCombatGAS, GSTNInputCore, GSTNInputNP (the input state the hold hook reads), GSTNDefenceCore and GSTNDefenceGAS.
// It must never depend on a GSTNLink module, an Editor module or the project module (a grep gate of every M6 commit checks it).
public class GSTNDefenceNP : ModuleRules
{
	public GSTNDefenceNP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GameplayAbilities",
			"NetworkPrediction",
			"AbilitySystemSimulation",
			"Mover",
			"GSTNInputCore",
			"GSTNInputNP",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNCombatNP",
			"GSTNDefenceCore",
			"GSTNDefenceGAS"
		});
	}
}
