using UnrealBuildTool;

// Kit mods, pure layer (combat-loadout-mods.md §8, §16): mod data types, the loadout extension, the baking of derived timelines.
// Allowed edges: the combat core only. No ability system, no Network Prediction, no world: everything here is tested without one.
public class GSTNKitModsCore : ModuleRules
{
	public GSTNKitModsCore(ReadOnlyTargetRules Target) : base(Target)
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
