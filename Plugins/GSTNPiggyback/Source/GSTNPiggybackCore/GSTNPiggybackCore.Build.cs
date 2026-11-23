using UnrealBuildTool;

// Piggyback, pure part (combat-m7-piggyback.md 2b and 3). Allowed edges: engine modules and the Core modules of the combat and link plugins
// (GSTNCombatCore, GSTNLinkCore), nothing else. It must never depend on GameplayAbilities, NetworkPrediction, Mover, an Editor module or
// the project module, and nothing outside this plugin may depend on it. The native tags live here, in a Runtime module: a tag defined in an
// UncookedOnly module ensures when a cooked game asks for it (lesson of GSTNExtensionTest).
public class GSTNPiggybackCore : ModuleRules
{
	public GSTNPiggybackCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNCombatCore",
			"GSTNLinkCore"
		});

		// Finds the plugin's own Config/Tags folder to register it with the tag manager
		PrivateDependencyModuleNames.Add("Projects");
	}
}
