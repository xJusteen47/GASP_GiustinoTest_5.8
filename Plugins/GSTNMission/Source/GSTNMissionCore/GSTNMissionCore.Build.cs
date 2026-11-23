using UnrealBuildTool;

// Missions, pure core: objective rules, mission definitions, the rules piece and the mission tracker. Allowed edges: engine, GSTNCore (the
// GameRules asset), GSTNInputCore (the authoritative event struct and its matching rule). Never GAS, Network Prediction, combat or a world module.
public class GSTNMissionCore : ModuleRules
{
	public GSTNMissionCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNCore",
			"GSTNInputCore"
		});

		// The plugin's Config/Tags folder is registered at startup (IPluginManager)
		PrivateDependencyModuleNames.Add("Projects");
	}
}
