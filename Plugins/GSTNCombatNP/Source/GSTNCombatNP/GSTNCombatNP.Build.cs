using UnrealBuildTool;

// Combat simulation for the GAS-NPP fork: the combat Network Prediction model and the code that drives the ability system
// from it. Engine-agnostic combat data and logic live in GSTNCombatCore, on top of the GSTNInputCore module of the GSTNInput plugin.
public class GSTNCombatNP : ModuleRules
{
	public GSTNCombatNP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AIModule",
			"GameplayTags",
			"GSTNCore", // IO-2b: the GameRules change serial
			"GSTNInputCore",
			"GSTNInputNP",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNSurfaceCore",
			"GSTNAnimationCore", // the run skid's placeholder lean through the directional additive node (presentation)
			"PhysicsCore",
			"GameplayAbilities",
			"AbilitySystemSimulation",
			"NetworkPrediction",
			"NetCore",
			"Mover"
		});

		// The precision reticle reads its local button and the aim stick (UGSTN_PrecisionAimComponent); presentation only
		PrivateDependencyModuleNames.Add("EnhancedInput");

		// CP4 cosmetic hooks: the move's cosmetic windows spawn Niagara systems (public headers only forward declare its types); presentation only
		PrivateDependencyModuleNames.Add("Niagara");
	}
}
