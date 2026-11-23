using UnrealBuildTool;

// Respawn, server side: the respawn subsystem (KO listener, timers, team restart), the checkpoint actor, the replicated state actor, the
// spawn service interface and the gstn.Respawn.* variables and commands. Allowed edges: engine, GSTNCore, GSTNRespawnCore, GSTNInputCore
// (the authoritative event stream), GSTNCombatGAS (KO tags), GSTNCombatNP (KO zones, in-place respawn). Never GSTNMission: the two meet
// only through events on the stream.
public class GSTNRespawnRuntime : ModuleRules
{
	public GSTNRespawnRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"NetCore",
			"GSTNCore",
			"GSTNRespawnCore",
			"GSTNInputCore",
			"GSTNCombatGAS",
			"GSTNCombatNP",
			"NetworkPrediction"
		});
	}
}
