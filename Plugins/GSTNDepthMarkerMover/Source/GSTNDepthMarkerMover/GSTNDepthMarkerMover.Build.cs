using UnrealBuildTool;

// Mover adapter of the depth marker (depth-marker.md §6.1: "a Mover adapter is a separate plugin").
// Allowed edges: the depth marker runtime/core, Mover and engine modules. Presentation only.
public class GSTNDepthMarkerMover : ModuleRules
{
	public GSTNDepthMarkerMover(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GSTNDepthMarkerCore",
			"GSTNDepthMarkerRuntime",
			"Mover"
		});
	}
}
