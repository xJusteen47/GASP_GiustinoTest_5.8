using UnrealBuildTool;

// Test only (M6 F14): a consumer of GSTNDefence's public API. It adds a defensive hit resolution step from outside the plugin and places it
// in a combat rules' pipeline; nothing in GSTNDefence or GSTNCombat knows it. It must never depend on a GSTNLink module.
public class GSTNExtTestDefence : ModuleRules
{
	public GSTNExtTestDefence(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNCombatCore",
			"GSTNCombatNP",
			"GSTNDefenceCore",
			"GSTNDefenceNP"
		});
	}
}
