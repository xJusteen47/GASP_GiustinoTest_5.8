using UnrealBuildTool;

// Development tooling of the input framework that runs in PIE and development builds: the input display (strip, corner overlay,
// editor tab), reading an input history through IGSTN_InputHistorySource of GSTNInputCore. The plugin descriptor leaves the module out
// of Shipping (TargetConfigurationDenyList), so nothing here may be needed by shipping code.
//
// Allowed edges: engine modules and GSTNInputCore. No GAS, Network Prediction, Mover or combat module: what it shows comes from sources
// that other plugins register (GSTNCombatDebug for the combat component, GSTNInputNPDebug for the standalone input component), so the
// plugin works in a project without the fork. Nothing of the runtime modules may depend on this one.
public class GSTNInputDebug : ModuleRules
{
	public GSTNInputDebug(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"SlateCore",
			"GSTNInputCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"Projects",
			"Slate",
			// The offscreen snapshot tests (GSTN.Visual.*): FWidgetRenderer and the render target read-back
			"UMG",
			"RenderCore",
			"RHI"
		});

		// The nomad tab of the editor: registered by this module itself, so the editor plugins have no edge to it
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("WorkspaceMenuStructure");
		}
	}
}
