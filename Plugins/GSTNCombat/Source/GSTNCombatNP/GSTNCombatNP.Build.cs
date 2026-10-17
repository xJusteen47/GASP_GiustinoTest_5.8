using UnrealBuildTool;

// Combat simulation for the GAS-NPP fork: the combat Network Prediction model and the code that drives the ability system
// from it. Engine-agnostic combat data and logic will live in a separate GSTNCombatCore module.
public class GSTNCombatNP : ModuleRules
{
	public GSTNCombatNP(ReadOnlyTargetRules Target) : base(Target)
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
			"NetCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EnhancedInput"
		});
	}
}
