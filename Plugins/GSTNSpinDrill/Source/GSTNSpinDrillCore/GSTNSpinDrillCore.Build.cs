using UnrealBuildTool;

// Spin and drill, pure part (spin-drill.md 4). Allowed edges: engine modules and the Core modules of the combat and input plugins
// (GSTNCombatCore, GSTNInputCore), nothing else. It must never depend on GameplayAbilities, AbilitySystemSimulation,
// NetworkPrediction, Mover, an Editor module, GSTNLink, GSTNParts, GSTNLevelObjects or the project module: the NP, GAS and
// presentation halves are separate modules that depend on this one.
public class GSTNSpinDrillCore : ModuleRules
{
	public GSTNSpinDrillCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNInputCore",
			"GSTNCombatCore"
		});

		// Finds the plugin's own Config/Tags folder to register it with the tag manager
		PrivateDependencyModuleNames.Add("Projects");
	}
}
