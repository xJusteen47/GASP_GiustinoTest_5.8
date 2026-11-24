using UnrealBuildTool;

// Mode verbs, pure core: native tags, the verb spec and payload, the once-only confirmation queue and the requestable gate (all plain data and
// integer math, tested with no world). Allowed edges: engine, GSTNInputCore (FGSTN_SimEvent, the authoritative event struct) and GSTNInteractCore
// (the candidate id). Never GAS, Network Prediction, Mover, combat, a world module, the Interact runtime or an Editor module.
public class GSTNModeVerbsCore : ModuleRules
{
	public GSTNModeVerbsCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNInputCore",
			"GSTNInteractCore"
		});
	}
}
