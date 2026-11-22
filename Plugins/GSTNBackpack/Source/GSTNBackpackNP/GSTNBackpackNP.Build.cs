using UnrealBuildTool;

// Backpacks, Network Prediction half (M11 L7b): the combat tick extension that runs the backpack's memory and dome (FGSTN_BackpackStep on
// the fragment of GSTNBackpackCore) and the backpack moves that are timeline abilities (the dome). Allowed edges: engine, the NP fork
// (GameplayAbilities for the tag projection), GSTNCombatNP (the combat model hosting the fragment), GSTNCombatGAS, GSTNCombatCore,
// GSTNInputCore and GSTNBackpackCore. Never GSTNPickups, GSTNDefence, an Editor module or the project module.
public class GSTNBackpackNP : ModuleRules
{
	public GSTNBackpackNP(ReadOnlyTargetRules Target) : base(Target)
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
			"GSTNInputCore",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNCombatNP",
			"GSTNBackpackCore"
		});
	}
}
