using UnrealBuildTool;

// Minion tests that need a world and harness helpers (GSTNMinion.Runtime.*, M8 G3 and later). Empty shell in G2: the pure
// GSTNMinion.Core.* tests live in GSTNMinionCore/Private/Tests.
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
			"GSTNMinionCore",
			"GSTNMinionRuntime"
		});
	}
}
