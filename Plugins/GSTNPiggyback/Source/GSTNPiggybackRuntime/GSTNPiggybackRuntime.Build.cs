using UnrealBuildTool;

// Piggyback, runtime part (combat-m7-piggyback.md 4 and 5, plan section 2): the mount link rule and type, the mount, dismount, drop and
// pair-move abilities, the carry load effect, the pair-move registry and the mount hit step. Empty in the skeleton (plan G4); the
// dependencies are the plan's, so G5 adds code without touching this file. Only GSTNLink's and GSTNCombat's public API is used, and
// nothing outside this plugin may depend on this module.
public class GSTNPiggybackRuntime : ModuleRules
{
	public GSTNPiggybackRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GameplayAbilities",
			"AbilitySystemSimulation",
			"NetworkPrediction",
			"GSTNPiggybackCore",
			"GSTNInputCore",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNCombatNP",
			"GSTNLinkCore",
			"GSTNLinkGAS",
			"GSTNLinkNP"
		});
	}
}
