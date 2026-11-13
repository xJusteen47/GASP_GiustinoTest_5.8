using UnrealBuildTool;

// Feature animation decoupled from the main graph (anim-layers.md). Allowed edges: engine modules only. Presentation code: the
// project, the combat plugins and feature plugins may depend on it, but no simulation code may call it, and it must never depend on
// GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, Mover or another GSTN plugin.
public class GSTNAnimationCore : ModuleRules
{
	public GSTNAnimationCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags"
		});

		// Finds the plugin's own Config/Tags folder to register it with the tag manager
		PrivateDependencyModuleNames.Add("Projects");
	}
}
