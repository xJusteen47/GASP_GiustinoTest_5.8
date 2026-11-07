using UnrealBuildTool;

// Minion runtime (batch subsystem, actors, presentation: M8 G3 and later). Empty shell in G2. The GAS and NP edges of the plan
// (GSTNCombatGAS, GSTNCombatNP, NetCore, NavigationSystem, StateTree) are added by the checkpoint that first uses them.
public class GSTNMinionRuntime : ModuleRules
{
	public GSTNMinionRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GSTNMinionCore"
		});
	}
}
