using UnrealBuildTool;

// The runtime of the camera (camera.md §8): settings, component, subsystems, modes, sections, manual control, occlusion, adapters.
// Allowed edges: GSTNCameraCore and engine modules only. Never GASP_Giustino2, another GSTN plugin, GameplayAbilities,
// AbilitySystemSimulation, NetworkPrediction or Mover (a Mover adapter is a separate plugin), and never an Editor module.
public class GSTNCameraRuntime : ModuleRules
{
	public GSTNCameraRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"DeveloperSettings",
			"InputCore",
			"EnhancedInput",
			"GSTNCameraCore"
		});
	}
}
