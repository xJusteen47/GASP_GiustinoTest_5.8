using UnrealBuildTool;

// The pure part of the depth marker (depth-marker.md §5, §6.1): types and target selection maths, no world, no Engine.
// Allowed edges: Core, CoreUObject, GameplayTags. Never GASP_Giustino2, another GSTN plugin, GameplayAbilities,
// AbilitySystemSimulation, NetworkPrediction or Mover. The BlueprintFunctionLibrary wrapper lives in GSTNDepthMarkerRuntime
// because UBlueprintFunctionLibrary is an Engine class.
public class GSTNDepthMarkerCore : ModuleRules
{
	public GSTNDepthMarkerCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"GameplayTags"
		});
	}
}
