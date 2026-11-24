using UnrealBuildTool;

// Fast review capture (README.md): scene captures, an async GPU readback per frame and view, PNG encoding on worker tasks. Tooling only:
// the descriptor keeps it out of Shipping and no game module may depend on it. The capture drivers (Tools/GASNPPTest/pie_capture_move.py)
// reach it from Python through UGSTN_FastCapture's UFUNCTIONs.
public class GSTNCapture : ModuleRules
{
	public GSTNCapture(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "RHI", "ImageCore", "ImageWrapper" });
	}
}
