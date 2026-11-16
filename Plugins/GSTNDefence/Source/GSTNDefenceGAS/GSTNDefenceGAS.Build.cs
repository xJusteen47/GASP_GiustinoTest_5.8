using UnrealBuildTool;

// Defence, ability-system half (m6 spec 1b). Allowed edges: engine, GameplayAbilities, GSTNDefenceCore, GSTNCombatCore, GSTNCombatGAS
// and GSTNInputCore (the binding and slot types the loadout contribution builds). It must never depend on NetworkPrediction,
// AbilitySystemSimulation, Mover, GSTNCombatNP, a GSTNLink module, an Editor module or the project module: the NP half
// (GSTNDefenceNP) sits above this module and is the only one that runs inside a simulation tick.
public class GSTNDefenceGAS : ModuleRules
{
	public GSTNDefenceGAS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GameplayAbilities",
			"GSTNInputCore",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNDefenceCore"
		});
	}
}
