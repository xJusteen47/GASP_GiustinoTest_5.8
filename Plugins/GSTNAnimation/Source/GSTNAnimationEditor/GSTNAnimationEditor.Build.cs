using UnrealBuildTool;

// Editor-only authoring of anim graphs from scripts (anim-layers.md, A2): the layer interfaces, the layer graphs and the override-layer seam of
// the main graph are built by Tools/GAS/setup_wall_anim_layer.py through UGSTN_AnimGraphAuthoringLibrary, so the assets can be rebuilt
// reproducibly instead of by hand. Nothing at runtime depends on this module, and it knows no feature: the script names the assets, clips and
// rules. Allowed edges: engine editor modules only (no GSTN plugin, no project module).
//
// The property access node of a transition rule (PropertyAccessNode plugin) is private to its module, so it is created by class name and its
// Path set by reflection; no module dependency on it.
public class GSTNAnimationEditor : ModuleRules
{
	public GSTNAnimationEditor(ReadOnlyTargetRules Target) : base(Target)
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
			"UnrealEd",          // FBlueprintEditorUtils, FKismetEditorUtilities, the compiler results log
			"BlueprintGraph",    // K2 nodes (variable get, enum equality) and the K2 schema
			"AnimGraph",         // anim graph nodes, state machine graphs, the anim schema
			"AnimGraphRuntime",  // runtime node structs behind the editor nodes (blend list)
			"KismetCompiler"
		});
	}
}
