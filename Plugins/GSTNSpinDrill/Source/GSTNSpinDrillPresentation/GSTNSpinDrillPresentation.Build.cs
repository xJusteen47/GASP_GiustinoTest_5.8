using UnrealBuildTool;

// Spin and drill, presentation (spin-drill.md 9). Holds the procedural spin and wobble component (D11) and the loadout extension that adds it.
// Runs on clients and listen hosts; nothing in the simulation may depend on it. Allowed edges: engine, Mover (its visual component),
// GSTNCombatCore, GSTNCombatNP (the shown combat state), GSTNCombatGAS (the loadout extension registry), GSTNSpinDrillCore and
// GSTNSpinDrillNP (the drill fragment).
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
			"Mover",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNCombatNP",
			"GSTNSpinDrillCore",
			"GSTNSpinDrillNP"
		});
	}
}
