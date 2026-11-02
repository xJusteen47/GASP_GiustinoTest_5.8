using UnrealBuildTool;

// Piggyback, editor part (combat-m7-piggyback.md 2b, plan G11): the pair-move factory and validators and the anchor debug panel. Empty in the
// skeleton (plan G4). Editor only; GSTNCombatEditor stays an optional edge, added when G11 needs it.
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
	}
}
