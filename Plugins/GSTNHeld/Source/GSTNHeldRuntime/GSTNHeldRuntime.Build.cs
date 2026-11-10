using UnrealBuildTool;

// Held objects, world side: the object component and actor, the object definition, the host's object subsystem (pickups, flights,
// landings), the pickup and throw abilities and the gstn.Held.* commands. Allowed edges: engine, NetworkPrediction (the fixed tick hook),
// GSTNCombatCore, GSTNCombatNP (snapshot, entity ids, ability base), GSTNCombatGAS, GAS and AbilitySystemSimulation (abilities),
// GSTNProjectileCore and GSTNProjectileNP (the throw is a projectile shot), GSTNHeldCore. Never GSTNLink, GSTNMinion or a project module.
public class GSTNHeldRuntime : ModuleRules
{
	public GSTNHeldRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"NetworkPrediction",
			"GSTNCombatCore",
			"GSTNCombatNP",
			"GSTNCombatGAS",
			"GameplayAbilities",
			"AbilitySystemSimulation",
			"GSTNProjectileCore",
			"GSTNProjectileNP",
			"GSTNHeldCore",
			"GSTNInputCore",
			"GSTNInteractCore",
			"GSTNInteractRuntime"
		});
	}
}
