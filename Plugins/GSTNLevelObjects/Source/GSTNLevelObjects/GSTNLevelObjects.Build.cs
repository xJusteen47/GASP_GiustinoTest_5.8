using UnrealBuildTool;

// Level objects that offer traversal surfaces. Allowed edges: engine modules, GSTNSurfaceCore (the provider contract and the launch
// math) and GSTNCombatCore. Never GSTNCombatNP or anything with GAS, Network Prediction or Mover: the objects are data, and the
// simulation reads them through IGSTN_SurfaceProvider.
public class GSTNLevelObjects : ModuleRules
{
	public GSTNLevelObjects(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNSurfaceCore",
			"GSTNCombatCore"
		});
	}
}
