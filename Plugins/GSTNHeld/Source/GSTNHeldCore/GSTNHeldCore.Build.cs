using UnrealBuildTool;

// Held objects, pure core: object state, tuning, the pickup race rule and the hold placement. Allowed edges: engine, GSTNCombatCore and
// GSTNProjectileCore (the throw is a projectile shot). Never GAS, Network Prediction or a world module.
public class GSTNHeldCore : ModuleRules
{
	public GSTNHeldCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNCombatCore",
			"GSTNProjectileCore"
		});
	}
}
