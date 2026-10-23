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
// Validation (E10) adds DataValidation (UEditorValidatorBase, the engine plugin of the same name), MessageLog (the findings of a Validate
// button), AssetRegistry (the kits that can carry a weapon) and GSTNInputCore (the slot validator the kit, weapon and preset checks wrap).
// GameplayAbilities is only for the ability class a test binding names; the kit data is in GSTNCombatGAS.
//
// Slate arrived with the timeline panel (E4), the toolkit (UnrealEd), the asset definition (AssetDefinition) and the details panel
// (PropertyEditor) with E5; the preview scene adds its modules in E6. The model, edit context, undo helper, layout, gesture code and
// the track editor registry use no Slate type at all: only Private/Timeline/Widgets, the toolkit and the commands do.
public class GSTNCombatEditor : ModuleRules
{
	public GSTNCombatEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// The automation tests share helper names (a fixture, a step counter) through file-scope using-directives, which a unity bundle of two
		// test files turns into ambiguities. Which files UBT bundles changes with every file added, so the module is compiled file by file.
		bUseUnity = false;

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
			"AssetDefinition",
			"DeveloperSettings",
			"AssetRegistry",
			"DataValidation",
			"EditorFramework",
			"InputCore",
			"MessageLog",
			"Projects",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"GameplayAbilities",
			"GSTNCombatGAS",
			"GSTNInputCore"
		});
	}
}
