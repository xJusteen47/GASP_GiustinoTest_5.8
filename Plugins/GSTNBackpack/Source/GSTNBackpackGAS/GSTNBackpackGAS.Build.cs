using UnrealBuildTool;

// Backpacks, ability layer (combat-loadout-mods.md §10, §16): the backpack asset, the loadout extension registration (compile input,
// rules view, effects on grant) and the attachment provider that puts the backpack on the implant's connector. Allowed edges: the
// combat core and its GAS layer. Never Network Prediction or GSTNCombatNP: a backpack verb is an ordinary ability of the loadout.
public class GSTNBackpackGAS : ModuleRules
{
	public GSTNBackpackGAS(ReadOnlyTargetRules Target) : base(Target)
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
			"GSTNBackpackCore"
		});
	}
}
