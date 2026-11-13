using UnrealBuildTool;

// The pure part of the camera (camera.md §8, §10): types and maths, no world, no Engine.
// Allowed edges: Core, CoreUObject, GameplayTags. Never GASP_Giustino2, another GSTN plugin, GameplayAbilities,
// AbilitySystemSimulation, NetworkPrediction or Mover. The BlueprintFunctionLibrary wrapper lives in GSTNCameraRuntime
// because UBlueprintFunctionLibrary is an Engine class.
public class GSTNCameraCore : ModuleRules
{
	public GSTNCameraCore(ReadOnlyTargetRules Target) : base(Target)
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
