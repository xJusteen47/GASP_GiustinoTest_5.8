using UnrealBuildTool;

// Ability-system layer of the combat framework: native tags, attribute sets, effects and the kit, weapon and super data.
// Allowed edges: engine (NetCore for the replicated loadout component, ModularGameplay for the rules provider), GameplayAbilities, GSTNCombatCore, GSTNInputCore (the slot and requirement types of
// the data). It must never depend on NetworkPrediction, AbilitySystemSimulation, Mover or GSTNCombatNP (the NP adapter sits above this module), nor on the project module.
public class GSTNCombatGAS : ModuleRules
{
	public GSTNCombatGAS(ReadOnlyTargetRules Target) : base(Target)
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
			"GameplayTasks",
			"DeveloperSettings",
			"ModularGameplay",
			"GSTNCombatCore",
			"GSTNInputCore",
			// hit-react-directional: the reaction component feeds the directional hit additive node through IGSTN_AdditiveReactionSource
			"GSTNAnimationCore",
			// IO-2b: the GameRules change serial, to recompile a loadout built before the server picked the rules
			"GSTNCore"
		});

		// The loadout screen's rows list content the Asset Manager does not scan (GSTN_LoadoutComponent::ListAssetsOfClass)
		PrivateDependencyModuleNames.Add("AssetRegistry");
		// Surface feedback: the skin's footwear goes to the pawn's UGSTN_SurfaceFeedbackComponent (presentation only)
		PrivateDependencyModuleNames.Add("GSTNPresentationCore");
	}
}
