using UnrealBuildTool;

// The game-thread runtime of the interaction focus (interaction.md §7): world subsystem, interactable and interactor
// components, the WorldStatic line of sight, settings, a minimal presentation, console variables.
// Allowed edges: GSTNInteractCore and engine modules only. Never GASP_Giustino2, another GSTN plugin, GameplayAbilities,
// AbilitySystemSimulation, NetworkPrediction or Mover, and never an Editor module.
public class GSTNInteractRuntime : ModuleRules
{
	public GSTNInteractRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"DeveloperSettings",
			"GSTNInteractCore"
		});
	}
}
