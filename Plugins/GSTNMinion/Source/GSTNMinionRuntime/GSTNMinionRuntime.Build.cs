using UnrealBuildTool;

// Minion runtime (M8 G3): the authority batch subsystem, the pooled actor and its component, the spawner and the bench.
// Plan section 2: never GSTNPlatform*, GSTNParts*, GSTNLink*, GSTNDefence* or the project module; no /Game/ path, no ConstructorHelpers.
// GSTNCombatGAS and GSTNInputCore joined in G5 (the KO event tags and the authoritative event stream); NetCore, NavigationSystem and
// StateTree join with the checkpoints that first use them (G6-G8).
public class GSTNMinionRuntime : ModuleRules
{
	public GSTNMinionRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public: the subsystem header names the hit provider (GSTNCombatNP), the timeline player (GSTNCombatCore) and the store
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNMinionCore",
			"GSTNCombatCore",
			"GSTNCombatNP"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// OnPostFixedTick of the world manager (the G1 fork hook) drives the batch
			"NetworkPrediction",
			// FGenericTeamId of the snapshot entry
			"AIModule",
			// Combat.Event.KO and Combat.KOReason.Health of the minion KO event (G5)
			"GSTNCombatGAS",
			// UGSTN_SimEventSubsystem: the authoritative event stream the KO goes to (G5)
			"GSTNInputCore"
		});
	}
}
