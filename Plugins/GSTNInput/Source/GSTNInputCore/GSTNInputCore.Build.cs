using UnrealBuildTool;

// Engine-agnostic input logic: input samples, history, direction classifier, pattern matcher, buffer, slots, priority
// and command bindings. Everything here is pure and deterministic, so it must never depend on GameplayAbilities,
// AbilitySystemSimulation, NetworkPrediction or Mover (the simulation adapter will live in GSTNInputNP).
public class GSTNInputCore : ModuleRules
{
	public GSTNInputCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"DeveloperSettings"
		});

		// Only for UInputAction references in the input config; the Core never reads input itself.
		PrivateDependencyModuleNames.Add("EnhancedInput");
	}
}
