using UnrealBuildTool;

// Experience framework (M12 G0/G1). Allowed edges: engine only (Game Features, Modular Gameplay, Asset Manager, developer settings).
// Never GameplayAbilities, NetworkPrediction, Mover or another GSTN plugin: the guard for NP pawns is an interface and counters, and the
// NP adapters call it, not the other way round (modularity-and-tags.md 3.7 rules 1 and 2).
public class GSTNExperience : ModuleRules
{
	public GSTNExperience(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"NetCore",
			"GameFeatures",
			"ModularGameplay",
			"DeveloperSettings"
		});
	}
}
