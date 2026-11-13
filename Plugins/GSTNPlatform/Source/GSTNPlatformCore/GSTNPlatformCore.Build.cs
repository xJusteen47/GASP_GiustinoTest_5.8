using UnrealBuildTool;

// The pure part of the platform plugin (many-enemies-replication.md 4.2, 4.4; traversal-surfaces.md 4.1): the platform spec and
// its shapes, the shape state timeline, the collision rules derived from a surface behaviour and the scripted path (the pose history
// moved to GSTNCore, M8 Q7; GSTN_PoseHistory.h here is a redirect). No world, no component, no Mover.
// Allowed edges: engine modules, GSTNCore and GSTNSurfaceCore (Core -> Core, modularity 3.7 rule 2).
// Never GASP_Giustino2, GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, Mover, GSTNCombat* or GSTNParts.
public class GSTNPlatformCore : ModuleRules
{
	public GSTNPlatformCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNCore",
			"GSTNSurfaceCore"
		});
	}
}
