using UnrealBuildTool;

// Destructible parts, pure part (combat-parts.md §2, modularity-and-tags.md §3.2 layer L3b). Allowed edges: engine modules and
// the Core module of the combat plugin (GSTNCombatCore), nothing else. It must never depend on GameplayAbilities,
// AbilitySystemSimulation, NetworkPrediction, Mover, an Editor module, another optional sibling (GSTNMinion, GSTNDefence,
// GSTNLink) or the project module: the GAS, NP and presentation halves are separate modules that depend on this one.
public class GSTNPartsCore : ModuleRules
{
	public GSTNPartsCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"DeveloperSettings",
			"GSTNCombatCore"
		});

		// Finds the plugin's own Config/Tags folder to register it with the tag manager
		PrivateDependencyModuleNames.Add("Projects");
	}
}
