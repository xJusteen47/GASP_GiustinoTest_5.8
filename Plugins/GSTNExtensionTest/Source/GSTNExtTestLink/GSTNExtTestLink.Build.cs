using UnrealBuildTool;

// Test only (M6 F14): a consumer of GSTNLink's public API, as another plugin (M7 mounts, M10 held bodies) would be. It adds a link type,
// its roles and a request rule from outside the plugin; nothing in GSTNLink knows it. It must never depend on a GSTNDefence module (the other
// module of this plugin does that) and nothing may depend on it.
public class GSTNExtTestLink : ModuleRules
{
	public GSTNExtTestLink(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNInputCore",
			"GSTNCombatCore",
			"GSTNCombatNP",
			"GSTNInteractCore",
			"GSTNLinkCore",
			"GSTNLinkGAS",
			"GSTNLinkNP"
		});
	}
}
