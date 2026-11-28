using UnrealBuildTool;

// Rollback plan F0: a world entity simulated by Network Prediction (forward predicted on simulated proxies). Spike module: engine and
// the Network Prediction fork only, no GAS, no Mover, no other GSTN module, so what it measures is the cost of the model itself.
public class GSTNWorldSim : ModuleRules
{
	public GSTNWorldSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"GSTNCore",
			"CoreUObject",
			"Engine",
			"NetworkPrediction",
			"NetCore"
		});
	}
}
