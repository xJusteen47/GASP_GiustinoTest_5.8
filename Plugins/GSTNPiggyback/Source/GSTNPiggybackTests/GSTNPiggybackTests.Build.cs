using UnrealBuildTool;

// Piggyback, test module (combat-m7-piggyback.md 2b, plan section 2): the Automation tests of the plugin and, from G5 on, its harness content.
// A DeveloperTool module, so it never ships. The Core tests need no world, no ability system and no project content (spec 2b isolation
// test b). Nothing may depend on this module.
public class GSTNPiggybackTests : ModuleRules
{
	public GSTNPiggybackTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GameplayAbilities",
			"AbilitySystemSimulation",
			"NetworkPrediction",
			"GSTNInputCore",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNCombatNP",
			"GSTNLinkCore",
			"GSTNLinkGAS",
			"GSTNLinkNP",
			"GSTNPiggybackCore",
			"GSTNPiggybackRuntime"
		});
	}
}
