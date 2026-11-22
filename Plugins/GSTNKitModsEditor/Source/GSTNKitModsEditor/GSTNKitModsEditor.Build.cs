using UnrealBuildTool;

// Editor tooling of kit mods (M11 L8). A bridge: it is the only module that names both GSTNKitMods and GSTNCombatEditor, so the mods
// plugin stays free of the M5 editor (and of the NP fork the editor needs), and the M5 editor knows nothing of mods. No runtime module
// may name this one.
public class GSTNKitModsEditor : ModuleRules
{
	public GSTNKitModsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"GameplayTags",
			"GameplayAbilities",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNKitModsCore",
			"GSTNKitModsGAS",
			"GSTNCombatEditor"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"ToolMenus",
			"ContentBrowser",
			"Slate",
			"SlateCore",
			"AssetRegistry"
		});
	}
}
