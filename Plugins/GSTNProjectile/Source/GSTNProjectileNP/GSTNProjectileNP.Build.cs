using UnrealBuildTool;

// Projectile, world side: the projectile actor (replicated once at spawn, then simulated locally), the world subsystem that steps the
// host's projectiles at the Network Prediction fixed tick and posts their hits into the combat inboxes, and the gstn.Projectile.Fire
// command. Allowed edges: engine, NetworkPrediction (the fixed tick hook), GSTNCombatCore, GSTNCombatNP (the step snapshot, the
// inboxes and the combat ability base), GSTNCombatGAS, GSTNInputCore, GameplayAbilities and AbilitySystemSimulation (the fire ability)
// and GSTNProjectileCore. Never a GSTNMinion module (minions are reached through the combat world's providers).
public class GSTNProjectileNP : ModuleRules
{
	public GSTNProjectileNP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"AIModule",
			"NetworkPrediction",
			"GSTNCombatCore",
			"GSTNCombatNP",
			"GSTNCombatGAS",
			"GSTNInputCore",
			"GameplayAbilities",
			"AbilitySystemSimulation",
			"GSTNProjectileCore"
		});
	}
}
