using UnrealBuildTool;

// Missions, server side: the mission subsystem (event listener, poll, publishing Message.Mission.*), the replicated state actor, the objective
// zone and the collectible. Allowed edges: engine, GSTNCore, GSTNInputCore (the event stream), GSTNMissionCore. No combat or respawn module:
// KOs and restarts arrive as tagged events, so this plugin builds and runs without them.
public class GSTNMissionRuntime : ModuleRules
{
	public GSTNMissionRuntime(ReadOnlyTargetRules Target) : base(Target)
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
			"GSTNInputCore",
			"GSTNMissionCore"
		});
	}
}
