using UnrealBuildTool;

// Bridge between parts and minions (combat-parts.md §2, §5.5; plan P15). The only module that knows both plugins. Allowed edges:
// engine modules, GSTNPartsCore and GSTNMinionCore public headers. It must not depend on GSTNMinionRuntime until P16 needs the hit
// flow seams, nor on GSTNPartsGAS or GSTNPartsNP (a minion has no ability system and no fragment), nor on GameplayAbilities,
// NetworkPrediction, Mover or the project module. Neither GSTNParts nor GSTNMinion lists this plugin.
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
			"GameplayTags",
			"GSTNCombatCore",
			"GSTNPartsCore",
			"GSTNMinionCore"
		});
	}
}
