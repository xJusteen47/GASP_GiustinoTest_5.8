using UnrealBuildTool;

// The pure core of the interaction focus (interaction.md §7): types, maths, registry, meaning table, interfaces, native tags.
// Allowed edges: engine modules only. Never GASP_Giustino2, another GSTN plugin, GameplayAbilities, AbilitySystemSimulation,
// NetworkPrediction or Mover, and never an Editor module. Automation-tested with no world.
public class GSTNInteractCore : ModuleRules
{
	public GSTNInteractCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"GameplayTags"
		});
	}
}
