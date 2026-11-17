using UnrealBuildTool;

// GSTN Motion Tools panel (editor only): a docked tab (Window > GSTN Animation > Motion Tools) that drives the Python
// package Content/Python/gstn_motion_tools. All the animation logic lives in Python so agents (MCP, scripts) and the
// panel run the same code; this module is only the hand UI. Allowed edges: engine editor modules only.
public class GSTNMotionToolsEditor : ModuleRules
{
	public GSTNMotionToolsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"Json",
			"PythonScriptPlugin",   // IPythonScriptPlugin::ExecPythonCommandEx
			"Slate",
			"SlateCore",
			"UnrealEd",
			"WorkspaceMenuStructure"
		});
	}
}
