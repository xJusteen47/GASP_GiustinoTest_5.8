using UnrealBuildTool;

// Editor tooling of the presentation effects: the authoring helpers the VFX setup script calls (Tools/GAS/setup_presentation_vfx.py)
// for what neither Python nor the editor MCP bridge can do on a Niagara system (add a user parameter, link a module input to it).
// Allowed edges: engine editor modules (Niagara, NiagaraEditor). No runtime module may name this one.
public class GSTNPresentationEditor : ModuleRules
{
	public GSTNPresentationEditor(ReadOnlyTargetRules Target) : base(Target)
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
			"UnrealEd",
			"Niagara",
			"NiagaraCore",
			"NiagaraEditor"
		});
	}
}
