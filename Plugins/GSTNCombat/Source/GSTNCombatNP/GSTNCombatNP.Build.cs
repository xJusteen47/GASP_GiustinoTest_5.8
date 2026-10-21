using UnrealBuildTool;

// Combat simulation for the GAS-NPP fork: the combat Network Prediction model and the code that drives the ability system
// from it. Engine-agnostic combat data and logic live in GSTNCombatCore, on top of the GSTNInputCore module of the GSTNInput plugin.
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
			"GSTNInputCore",
			"GSTNInputNP",
			"GSTNCombatCore",
			"GameplayAbilities",
			"AbilitySystemSimulation",
			"NetworkPrediction",
			"NetCore",
			"Mover"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EnhancedInput"
		});
	}
}
