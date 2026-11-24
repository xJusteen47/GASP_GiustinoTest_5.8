using UnrealBuildTool;

// Mode verbs, Network Prediction half: the verb ability (UGSTN_GA_Base child), the requestable prop (a GSTNInteract record), the world subsystem
// that validates a confirmed verb on the host and republishes it on the event stream, and the interact meaning registration.
// Allowed edges: engine, the NP fork (NetworkPrediction for the fixed tick hook, AbilitySystemSimulation and GameplayAbilities for the ability
// base), GSTNCombatNP (UGSTN_GA_Base and the combat component's simulation frame), GSTNCombatCore, GSTNInputCore (the event stream),
// GSTNInteractCore and GSTNInteractRuntime (the record registry and the interactable component), GSTNModeVerbsCore.
// Never GSTNMission, GSTNRespawn, GSTNLink, GSTNHeld, an Editor module or the project module: frameworks listen to the event stream, they are not linked.
public class GSTNModeVerbsNP : ModuleRules
{
	public GSTNModeVerbsNP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"NetCore",
			"GameplayTags",
			"GameplayAbilities",
			"NetworkPrediction",
			"AbilitySystemSimulation",
			"GSTNCore",
			"GSTNInputCore",
			"GSTNCombatCore",
			"GSTNCombatNP",
			"GSTNInteractCore",
			"GSTNInteractRuntime",
			"GSTNModeVerbsCore"
		});
	}
}
