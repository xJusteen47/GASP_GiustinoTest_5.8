using UnrealBuildTool;

// Development tooling of the combat framework that runs in PIE and development builds (combat-m5-editor.md 3, 3b): input
// display, later the Gameplay Debugger category, debug draw and the combat trace. The plugin descriptor excludes the module
// from Shipping (TargetConfigurationDenyList), so nothing here may be needed by shipping code.
//
// Allowed edges: engine modules (SlateCore for the widget the module interface returns), GSTNCombatCore, GSTNInputCore, GSTNCombatNP (const accessors only) and GameplayDebugger.
// None is needed by the skeleton; E14 and E15 add each one when it first reads it. This module reads combat state through
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
	}
}
