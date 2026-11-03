using UnrealBuildTool;

// The runtime of the platform plugin: settings, the definition asset, the enemy platform component, the shape colliders, the
// world subsystem that runs the pose point, the scripted test fighter. Allowed edges: GSTNPlatformCore, GSTNSurfaceCore, Mover and
// engine modules (plan T3 acceptance: "GSTNPlatform builds with GSTNSurfaceCore and Mover only"). Never GASP_Giustino2,
// GameplayAbilities, AbilitySystemSimulation, NetworkPrediction (the pose point reads Mover's time step, not NP), GSTNCombat*,
// GSTNParts or an Editor module.
public class GSTNPlatformRuntime : ModuleRules
{
	public GSTNPlatformRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"DeveloperSettings",
			"PhysicsCore",
			"GSTNSurfaceCore",
			"GSTNPlatformCore",
			"Mover"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NetCore"
		});
	}
}
