using UnrealBuildTool;

// Minion tests that need a world (GSTNMinion.Runtime.*, M8 G3 and later). The pure GSTNMinion.Core.* tests live in
// GSTNMinionCore/Private/Tests.
public class GSTNMinionTests : ModuleRules
{
	public GSTNMinionTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNMinionCore",
			"GSTNMinionRuntime",
			"GSTNCombatCore",
			"GSTNCombatNP",
			// FGSTN_InputHistory (exported struct) inside the fighter presentation state built by AttackerHitstopOnMinionHit
			"GSTNInputCore",
			// FGenericTeamId of the stub fighter in the hit tests (G5)
			"AIModule"
		});
	}
}
