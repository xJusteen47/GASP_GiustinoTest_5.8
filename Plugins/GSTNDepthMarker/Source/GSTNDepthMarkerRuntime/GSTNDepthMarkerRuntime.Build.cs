using UnrealBuildTool;

// The runtime of the depth marker (depth-marker.md §6.1): settings, subsystem, component, selector, adapters, presenters.
// Allowed edges: GSTNDepthMarkerCore and engine modules only. Never GASP_Giustino2, another GSTN plugin, GameplayAbilities,
// AbilitySystemSimulation, NetworkPrediction or Mover (a Mover adapter is a separate plugin), and never an Editor module.
public class GSTNDepthMarkerRuntime : ModuleRules
{
	public GSTNDepthMarkerRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"DeveloperSettings",
			"GSTNDepthMarkerCore"
		});

		// Only the automation snapshots (GSTN.Visual.DepthMarker.*) use these: they read back a scene capture
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RHI",
			"RenderCore"
		});
	}
}
