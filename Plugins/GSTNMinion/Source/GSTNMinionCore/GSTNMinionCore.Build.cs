using UnrealBuildTool;

// Minion, pure part (M8 plan section 2). Allowed edges: engine modules, GSTNCombatCore and GSTNCore (shared net primitives).
// It must never depend on GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, Mover, an Editor module, another
// GSTNMinion module or the project module: the Runtime, Editor and Tests modules depend on this one.
public class GSTNMinionCore : ModuleRules
{
	public GSTNMinionCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNCombatCore",
			"GSTNCore"
		});

		// Finds the plugin's own Config/Tags folder to register it with the tag manager
		PrivateDependencyModuleNames.Add("Projects");
	}
}
