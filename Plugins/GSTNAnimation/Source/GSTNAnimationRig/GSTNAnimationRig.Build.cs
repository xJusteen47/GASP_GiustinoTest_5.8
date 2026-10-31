using UnrealBuildTool;

// Control Rig units of the animation plugin (limb-contact-ik.md). Allowed edges: engine modules (Control Rig, RigVM, AnimationCore)
// and GSTNAnimationCore. Presentation only: it runs inside anim graphs; no simulation module may depend on it, and it must never
// depend on GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, Mover or another GSTN plugin.
public class GSTNAnimationRig : ModuleRules
{
	public GSTNAnimationRig(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ControlRig",
			"RigVM",
			"GSTNAnimationCore"
		});

		// AnimationCore::SolveTwoBoneIK
		PrivateDependencyModuleNames.Add("AnimationCore");
	}
}
