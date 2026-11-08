using UnrealBuildTool;

// Bridge between parts and minions (combat-parts.md §2, §5.5; plan P15, P16). The only module that knows both plugins. Allowed edges:
// engine modules, GSTNPartsCore, GSTNMinionCore, GSTNMinionRuntime (P16: the hit flow seams of the minion subsystem) and GSTNCombatNP
// (only for the headers that subsystem exposes: the hit inbox entry). Never GSTNPartsGAS or GSTNPartsNP (a minion has no ability system and
// no fragment), nor GameplayAbilities, NetworkPrediction, Mover or the project module. Neither GSTNParts nor GSTNMinion lists this plugin.
public class GSTNPartsMinion : ModuleRules
{
	public GSTNPartsMinion(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"GameplayTags",
			"GSTNCombatCore",
			"GSTNCombatNP",
			"GSTNPartsCore",
			"GSTNMinionCore",
			"GSTNMinionRuntime"
		});
	}
}
