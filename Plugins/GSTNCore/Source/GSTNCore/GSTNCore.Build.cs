using UnrealBuildTool;

// Shared helpers of the GSTN plugins (modularity-and-tags.md 3.6, layer L2): today the net primitives of Net/ (pose history) used by
// GSTNPlatform and GSTNMinion (M8 plan Q7). Engine modules only: never another GSTN module, GASP_Giustino2, GameplayAbilities,
// AbilitySystemSimulation, NetworkPrediction or Mover (Tools/Modules/check_modules.py enforces it).
public class GSTNCore : ModuleRules
{
	public GSTNCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core"
		});
	}
}
