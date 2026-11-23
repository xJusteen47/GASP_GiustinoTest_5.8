using UnrealBuildTool;

// The parts bridge of the platform plugin (traversal-surfaces.md 4.4, plan T6): part state -> platform shape mask. A bridge in the sense
// of modularity-and-tags 3.7 rule 3: it is the only module that knows both GSTNPlatform and GSTNParts, so neither of them names the other.
// Allowed edges: engine, GSTNPlatformCore/Runtime, GSTNPartsCore/NP (the part set and the sync fragment), GSTNCombatCore/NP (the combat
// sync state the fragment rides in). Never the project module, an Editor module or the parts presentation.
public class GSTNPlatformParts : ModuleRules
{
	public GSTNPlatformParts(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GSTNPlatformCore",
			"GSTNPlatformRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayTags",
			"GameplayAbilities",
			"NetworkPrediction",
			"GSTNPartsCore",
			"GSTNPartsNP",
			"GSTNCombatCore",
			"GSTNCombatNP"
		});
	}
}
