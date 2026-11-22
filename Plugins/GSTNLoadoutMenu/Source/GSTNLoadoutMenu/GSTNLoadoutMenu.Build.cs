using UnrealBuildTool;

// Pre-mission loadout screen: local presentation only (Slate), outside the simulation.
// Allowed edges: engine UI and Enhanced Input, GSTNCore (game rules), GSTNInputCore (buttons, layout, the config source interface),
// GSTNCombatCore/GSTNCombatGAS (loadout data, component, screen rows). Never Network Prediction, Mover, an NP adapter or the project module:
// the rows of optional plugins arrive through FGSTN_LoadoutChoiceRegistry, the input config through IGSTN_InputConfigSource.
public class GSTNLoadoutMenu : ModuleRules
{
	public GSTNLoadoutMenu(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNCore",
			"GSTNInputCore",
			"GSTNCombatCore",
			"GSTNCombatGAS"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
			"EnhancedInput"
		});
	}
}
