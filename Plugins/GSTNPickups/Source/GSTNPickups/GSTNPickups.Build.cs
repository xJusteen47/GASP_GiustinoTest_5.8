using UnrealBuildTool;

// Equipment pickups (M11 G2, combat-loadout-mods.md §12). Allowed edges: the combat core and its GAS layer (the loadout component applies a
// pickup), the input core (the TH-1b hold event). Never Network Prediction, Mover, GSTNHeld, GSTNLink or an Editor module: the plugins that
// bring a better trigger (GSTNHeld's framed kit swap) depend on this one, not the reverse.
public class GSTNPickups : ModuleRules
{
	public GSTNPickups(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"NetCore",
			"GameplayTags",
			"GameplayAbilities",
			"GSTNCore",
			"GSTNInputCore",
			"GSTNCombatCore",
			"GSTNCombatGAS"
		});
	}
}
