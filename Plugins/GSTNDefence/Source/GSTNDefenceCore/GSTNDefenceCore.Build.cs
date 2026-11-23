using UnrealBuildTool;

// Defence, pure part (m6 spec 1b). Allowed edges: engine modules and the Core modules of the combat and input plugins
// (GSTNCombatCore, GSTNInputCore), nothing else. It must never depend on GameplayAbilities, AbilitySystemSimulation,
// NetworkPrediction, Mover, an Editor module, the sibling optional plugin (GSTNLink) or the project module: the GAS and NP
// halves are separate modules that depend on this one.
public class GSTNDefenceCore : ModuleRules
{
	public GSTNDefenceCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"DeveloperSettings",
			"GSTNInputCore",
			"GSTNCombatCore"
		});

		// Finds the plugin's own Config/Tags folder to register it with the tag manager
		PrivateDependencyModuleNames.Add("Projects");
	}
}
