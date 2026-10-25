using UnrealBuildTool;

// Destructible parts, Network Prediction half (combat-parts.md §2, §5). The sync fragment, and from P7 the snapshot contributor, the tick
// extension and the outcome applier. Allowed edges: engine, the NP fork (NetworkPrediction), GSTNCombatNP (the combat NP model that hosts
// the fragments), GSTNCombatCore, GSTNCombatGAS, GSTNPartsCore and GSTNPartsGAS. It must never depend on an Editor module, the
// presentation module or the project module: presentation reads the events and the fragment through the GAS half only.
public class GSTNPartsNP : ModuleRules
{
	public GSTNPartsNP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GameplayAbilities",
			"NetworkPrediction",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNCombatNP",
			"GSTNPartsCore",
			"GSTNPartsGAS"
		});
	}
}
