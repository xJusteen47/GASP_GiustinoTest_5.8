using UnrealBuildTool;

// Adapter between the input logic of GSTNInputCore and the Network Prediction fork: the input command, the embeddable
// input simulation state and simulator, the command binding set, the input sources and context providers, and the Mover
// sim-state tracker. Hosts (the combat model, a standalone input component) embed these pieces in their own model; this
// module has no combat vocabulary.
public class GSTNInputNP : ModuleRules
{
	public GSTNInputNP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNInputCore",
			"GameplayAbilities",
			"AbilitySystemSimulation",
			"NetworkPrediction",
			"NetCore",
			"Mover"
		});

		// Enhanced Input is only used by the player input source; no header of this module exposes it
		PrivateDependencyModuleNames.Add("EnhancedInput");
	}
}
