using UnrealBuildTool;

// Links, ability-system half (m6 spec 1b, 7.0 items 3 and 4). Allowed edges: engine modules, GameplayAbilities through GSTNCombatGAS
// (the kit and weapon assets whose extensions carry the link data), GSTNLinkCore, the Core modules of the combat and input plugins and
// GSTNInteractCore (the candidate provider contract of the interact probe). It must never depend on NetworkPrediction,
// AbilitySystemSimulation, Mover, GSTNCombatNP, a GSTNDefence module, an Editor module or the project module: the NP half
// (GSTNLinkNP, F10) sits above this module and is the only one that runs inside a simulation tick.
public class GSTNLinkGAS : ModuleRules
{
	public GSTNLinkGAS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNInputCore",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNInteractCore",
			"GSTNLinkCore"
		});
	}
}
