using UnrealBuildTool;

// Destructible parts, presentation half (combat-parts.md §6, plan P10-min). Cosmetic only: reads a part state and writes render data
// (custom primitive data). Allowed edges: engine, GSTNPartsCore. No simulation module may depend on it, and nothing it does writes back
// into the simulation.
public class GSTNPartsPresentation : ModuleRules
{
	public GSTNPartsPresentation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GSTNPartsCore"
		});
	}
}
