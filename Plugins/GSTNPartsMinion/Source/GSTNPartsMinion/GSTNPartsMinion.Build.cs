using UnrealBuildTool;

// Bridge between parts and minions (combat-parts.md §2, §5.5; plan P15, P16). The only module that knows both plugins. Allowed edges:
// engine modules, GSTNPartsCore, GSTNMinionCore, GSTNMinionRuntime (P16: the hit flow seams of the minion subsystem) and GSTNCombatNP
// (only for the headers that subsystem exposes: the hit inbox entry). P16 sub-gate B adds GSTNPartsNP, privately and for one call: the
// promotion seed of FGSTN_PartsAttach (a promoted minion's row becomes the fighter's parts fragment). A minion itself still has no ability
// system and no fragment: never GSTNPartsGAS, nor GameplayAbilities, NetworkPrediction, Mover or the project module directly.
// Neither GSTNParts nor GSTNMinion lists this plugin.
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

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GSTNPartsNP"
		});
	}
}
