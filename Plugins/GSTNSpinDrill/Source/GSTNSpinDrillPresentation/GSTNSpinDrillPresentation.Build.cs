using UnrealBuildTool;

// Spin and drill, presentation (spin-drill.md 9). Will hold the procedural spin and wobble component; empty in the skeleton
// (plan D1). Runs on clients and listen hosts; nothing in the simulation may depend on it. Allowed edges: engine, GSTNSpinDrillCore and
// GSTNSpinDrillNP (it reads the shown drill state).
public class GSTNSpinDrillPresentation : ModuleRules
{
	public GSTNSpinDrillPresentation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNSpinDrillCore",
			"GSTNSpinDrillNP"
		});
	}
}
