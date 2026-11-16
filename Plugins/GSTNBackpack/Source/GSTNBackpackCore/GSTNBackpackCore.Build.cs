using UnrealBuildTool;

// Backpacks, pure layer (combat-loadout-mods.md §10, §16): the backpack's loadout extension, its tags and the pure equip/swap rule.
// Allowed edges: the combat core only. No ability system, no Network Prediction, no world: everything here is tested without one.
public class GSTNBackpackCore : ModuleRules
{
	public GSTNBackpackCore(ReadOnlyTargetRules Target) : base(Target)
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
	}
}
