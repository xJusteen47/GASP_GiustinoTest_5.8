using UnrealBuildTool;

// Development tooling of the input NP plugin that runs in PIE and development builds: it teaches the input display of GSTNInputDebug to
// read the standalone input component (UGSTN_InputSimComponent) through its public const accessors. The plugin descriptor leaves the
// module out of Shipping (TargetConfigurationDenyList), so nothing here may be needed by shipping code.
//
// Allowed edges: engine modules, GSTNInputCore, GSTNInputNP (public headers) and GSTNInputDebug (the source registry). It reads the
// component, it never changes it, and no runtime module may depend on it.
public class GSTNInputNPDebug : ModuleRules
{
	public GSTNInputNPDebug(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayTags",
			"GSTNInputCore",
			"GSTNInputNP",
			"GSTNInputDebug"
		});
	}
}
