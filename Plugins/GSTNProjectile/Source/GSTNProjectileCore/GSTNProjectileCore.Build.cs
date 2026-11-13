using UnrealBuildTool;

// Projectile, pure part: the ballistic mode (closed-form position from the spawn parameters), the spawn parameters and the per-frame
// sweep against one hurtbox shape. Allowed edges: engine modules and GSTNCombatCore (the hit shapes and hit data). Never GAS, Network
// Prediction, Mover, an Editor module or the project module (Tools/Modules/check_modules.py checks it).
public class GSTNProjectileCore : ModuleRules
{
	public GSTNProjectileCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNCombatCore"
		});
	}
}
