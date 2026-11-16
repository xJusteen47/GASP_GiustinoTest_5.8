using UnrealBuildTool;

// Destructible parts, ability-system half (combat-parts.md §2, §3.3, §5.4). Allowed edges: engine, GameplayAbilities, GSTNPartsCore,
// GSTNCombatCore, GSTNCombatGAS and GSTNInputCore (the simulation event type). It must never depend on NetworkPrediction,
// AbilitySystemSimulation, Mover, GSTNCombatNP, an Editor module or the project module: the NP half sits above this module and
// pushes what this one builds.
public class GSTNPartsGAS : ModuleRules
{
	public GSTNPartsGAS(ReadOnlyTargetRules Target) : base(Target)
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
			"GSTNPartsCore"
		});
	}
}
