using UnrealBuildTool;

// Combat content built on the input core: the standard button and command tags and the default fighting-game command
// set. Pure and deterministic like the input core, so it must never depend on GameplayAbilities,
// AbilitySystemSimulation, NetworkPrediction or Mover (the simulation adapter lives in GSTNCombatNP).
public class GSTNCombatCore : ModuleRules
{
	public GSTNCombatCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNInputCore",
			"DeveloperSettings"
		});

		// The module registers the plugin's tag ini folder at startup
		PrivateDependencyModuleNames.Add("Projects");
	}
}
