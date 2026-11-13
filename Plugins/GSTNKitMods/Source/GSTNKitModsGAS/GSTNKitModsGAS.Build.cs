using UnrealBuildTool;

// Kit mods, ability layer (combat-loadout-mods.md §16): the mod asset, the loadout extension registration (compile input, rules
// view, stat effects). Allowed edges: the combat core and its GAS layer. Never Network Prediction or GSTNCombatNP: the base timeline
// of an ability with an inline timeline is read by reflection, so this plugin stays out of the simulation modules.
public class GSTNKitModsGAS : ModuleRules
{
	public GSTNKitModsGAS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GameplayAbilities",
			"GSTNCore",
			"GSTNInputCore",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNKitModsCore"
		});
	}
}
