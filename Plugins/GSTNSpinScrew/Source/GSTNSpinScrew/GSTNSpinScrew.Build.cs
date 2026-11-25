using UnrealBuildTool;

// The spin screw (spin-drill.md 13c): a level object that implements GSTN Spin Turnable and, unscrewed, becomes a held object. A bridge in the
// sense of modularity-and-tags 3.7 rule 3: the only module that knows both GSTNSpinDrill and GSTNHeld. Allowed edges: engine,
// GSTNSpinDrillCore (the interface), GSTNHeldRuntime (the held object actor and the host's pickup). Never the project module, GSTNCombat*
// directly, Network Prediction or an Editor module: a screw is ordinary replicated state, it is never simulated.
public class GSTNSpinScrew : ModuleRules
{
	public GSTNSpinScrew(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GSTNSpinDrillCore",
			"GSTNHeldRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NetCore"
		});
	}
}
