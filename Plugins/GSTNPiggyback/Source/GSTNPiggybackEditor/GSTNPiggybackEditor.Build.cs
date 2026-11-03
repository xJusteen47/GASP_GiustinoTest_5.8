using UnrealBuildTool;

// Piggyback, editor part (combat-m7-piggyback.md 2b and 9, plan G11): the pair-move factories and asset definitions, the pair-move ability
// validator, and the PIE debug of links (gstn.Combat.Debug.Anchor, the link transition log). Editor only; nothing depends on it.
//
// GSTNCombatEditor is not an edge: its validator base and report types are private to it, and the checks here need only the runtime's own
// Validate. DataValidation is the engine plugin of UEditorValidatorBase, AssetDefinition the Content Browser entries, AssetRegistry the ids
// the factory skips. GSTNLink* and GSTNCombatNP are for the debug: the presentation component, the link state and the step snapshot.
public class GSTNPiggybackEditor : ModuleRules
{
	public GSTNPiggybackEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GSTNPiggybackCore",
			"GSTNPiggybackRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"AssetDefinition",
			"AssetRegistry",
			"DataValidation",
			"GameplayTags",
			"GSTNCombatGAS",
			"GSTNCombatNP",
			"GSTNLinkCore",
			"GSTNLinkGAS",
			"GSTNLinkNP"
		});
	}
}
