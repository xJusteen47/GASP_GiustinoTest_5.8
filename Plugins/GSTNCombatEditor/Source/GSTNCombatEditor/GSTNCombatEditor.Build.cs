using UnrealBuildTool;

// Editor tooling of the combat framework (combat-m5-editor.md 3, 3b). Editor module only: nothing in a game module may
// depend on it, and no runtime module of GSTNInput* or GSTNCombat* names it (checked by grep, plan m5 E1 and E19).
//
// Allowed edges: engine editor modules, GSTNCombatCore (the timeline and hurtbox data it edits) and GSTNCombatGAS (kit data
// and the Combat.Hurtbox.* tags the settings map to). Not allowed: the project module, GSTNCombatNP and NetworkPrediction
// (the tick rate is read from the Network Prediction settings object by reflection, see UGSTN_CombatEditorSettings, so the
// editor does not link the fork), and GSTNCombatDebug (a sibling plugin: an optional tool the editor will reach through
// FModuleManager, never through a Build.cs edge, because UBT stops when a named module is missing).
//
// Slate arrived with the timeline panel (E4); the toolkit and the preview scene add their modules in E5 and E6. The model, edit
// context, undo helper, layout and gesture code use no Slate type at all: only Private/Timeline/Widgets and the commands do.
public class GSTNCombatEditor : ModuleRules
{
	public GSTNCombatEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNCombatCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ApplicationCore",
			"DeveloperSettings",
			"InputCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"GSTNCombatGAS"
		});
	}
}
