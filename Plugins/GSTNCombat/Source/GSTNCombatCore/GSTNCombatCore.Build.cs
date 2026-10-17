using UnrealBuildTool;

// Engine-agnostic combat logic: input samples, history, direction classifier, pattern matcher, buffer and slots.
// Everything here is pure and deterministic, so it must never depend on GameplayAbilities, AbilitySystemSimulation,
// NetworkPrediction or Mover (the simulation adapter lives in GSTNCombatNP).
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
			"GameplayTags"
		});
	}
}
