using UnrealBuildTool;

// Respawn, pure core: the rules piece, the lives pool arithmetic, the checkpoint progress and the spawn placement choice. Allowed edges:
// engine, GSTNCore (the GameRules asset). Never GAS, Network Prediction or a world module: everything here is tested without a world.
public class GSTNRespawnCore : ModuleRules
{
	public GSTNRespawnCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNCore"
		});
	}
}
