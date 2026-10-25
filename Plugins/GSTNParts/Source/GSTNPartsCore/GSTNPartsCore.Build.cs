using UnrealBuildTool;

// Pure part logic: part sets, penetration table, break-effect data, native tags. Like GSTNCombatCore it must never depend on
// GameplayAbilities, AbilitySystemSimulation, NetworkPrediction or Mover (those arrive in GSTNPartsGAS and GSTNPartsNP), and the
// only other plugin module it may link is GSTNCombatCore.
public class GSTNPartsCore : ModuleRules
{
	public GSTNPartsCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"GameplayTags",
			"GSTNCombatCore",
			// UDeveloperSettings (GSTN_PartsSettings.h); Engine comes through GSTNCombatCore
			"DeveloperSettings"
		});

		// The module registers the plugin's tag ini folder at startup
		PrivateDependencyModuleNames.Add("Projects");
	}
}
