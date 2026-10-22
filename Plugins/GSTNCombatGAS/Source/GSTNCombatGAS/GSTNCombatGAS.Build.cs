using UnrealBuildTool;

// Ability-system layer of the combat framework: native tags, attribute sets, effects and the kit, weapon and super data.
// Allowed edges: engine, GameplayAbilities, GSTNCombatCore, GSTNInputCore (the slot and requirement types of
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
			"GameplayTags",
			"GameplayAbilities",
			"GameplayTasks",
			"DeveloperSettings",
			"GSTNCombatCore",
			"GSTNInputCore"
		});
	}
}
