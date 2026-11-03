using UnrealBuildTool;

// Development tooling of the combat framework that runs in PIE and development builds (combat-m5-editor.md 3, 3b): the combat side of
// the input display (the combat component as a source of GSTNInputDebug), later the Gameplay Debugger category, debug draw and the
// combat trace. The plugin descriptor excludes the module from Shipping (TargetConfigurationDenyList), so nothing here may be needed by
// shipping code.
//
// Allowed edges: engine modules, GSTNInputDebug (the display and its registry), GSTNCombatCore, GSTNInputCore, GSTNCombatNP (const
// accessors only) and, from E15, GameplayDebugger. The input display itself moved to GSTNInputDebug: this module registers a finder
// for the combat component and the combat buttons and standard command set as the display's default profile, so it needs the input
// debug module's public registry and nothing of Slate. No GSTNCombat* or GSTNInput* runtime module may depend on this module.
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
			"GSTNInputDebug",
			"GSTNCombatCore",
			"GSTNCombatNP"
		});
	}
}
