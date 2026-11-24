using UnrealBuildTool;

// Levels mode (M12 G6): level set, score rules, goal zone, collectible counting, mode subsystem, results widget. One module on purpose: a mode is
// a Game Feature, not a framework. Allowed edges: engine, GSTNCore (GameRules), GSTNInputCore (the authoritative event stream), GSTNCombatCore
// (UGSTN_CombatRules, for the friendly-fire default), GSTNMissionCore and GSTNRespawnCore (rules pieces and tags only) and GSTNMissionRuntime
// (only for the two actors the goal zone and the collectible derive from). Never GSTNRespawnRuntime and never another mode: Levels meets Respawn
// through events on the stream and sibling pieces of one GameRules asset.
public class GSTNMode_LevelsRuntime : ModuleRules
{
	public GSTNMode_LevelsRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"NetCore",
			"GSTNCore",
			"GSTNInputCore",
			"GSTNCombatCore",
			"GSTNMissionCore",
			"GSTNMissionRuntime",
			"GSTNRespawnCore"
		});

		// Test-only edges (Private/Tests/GSTN_M12SweepTests.cpp, M12 G8): the sweep composes the Experience loader and the mode verbs with this mode.
		// Editor builds only; the runtime never includes them, so the mode still builds with the frameworks removed from a game target.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"GSTNExperience",
				"GSTNModeVerbsCore"
			});
		}

		// The placeholder results screen is Slate (no UMG asset to author)
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore"
		});
	}
}
