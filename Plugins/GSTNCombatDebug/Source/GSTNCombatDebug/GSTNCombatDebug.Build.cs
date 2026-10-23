using UnrealBuildTool;

// Development tooling of the combat framework that runs in PIE and development builds (combat-m5-editor.md 3, 3b): input
// display, later the Gameplay Debugger category, debug draw and the combat trace. The plugin descriptor excludes the module
// from Shipping (TargetConfigurationDenyList), so nothing here may be needed by shipping code.
//
// Allowed edges: engine modules, GSTNCombatCore, GSTNInputCore, GSTNInputNP, GSTNCombatNP (const accessors only) and GameplayDebugger.
// E14 (the input display) added Slate, Projects (tests read the plugin's own sources), GameplayTags, GSTNInputCore (history, script, resolver), GSTNCombatCore (button tags, the standard
// command set), GSTNInputNP (the command table, to name a request) and GSTNCombatNP (the component whose history it reads); E15 adds
// GameplayDebugger. In an editor build WorkspaceMenuStructure is added for the nomad tab this module registers itself: the editor plugin has
// no edge to this module and this module has none to the editor plugin. This module reads combat state through
// const accessors and the trace sink the runtime modules expose, so no GSTNCombat* or GSTNInput* runtime module may depend on it.
public class GSTNCombatDebug : ModuleRules
{
	public GSTNCombatDebug(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayTags",
			"Projects",
			"Slate",
			"GSTNInputCore",
			"GSTNInputNP",
			"GSTNCombatCore",
			"GSTNCombatNP"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("WorkspaceMenuStructure");
		}
	}
}
