using UnrealBuildTool;

// The traversal surface data contract (traversal-surfaces.md §2, §3). Allowed edges: engine modules only. It is the Core that
// GSTNCombatCore (impacts, launch), the wall movement (surface filter) and GSTNPlatform (shapes) depend on, so it must never
// depend on GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, Mover or another GSTN plugin.
public class GSTNSurfaceCore : ModuleRules
{
	public GSTNSurfaceCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"PhysicsCore",
			"DeveloperSettings"
		});

		// Finds the plugin's own Config/Tags folder to register it with the tag manager
		PrivateDependencyModuleNames.Add("Projects");
	}
}
