using UnrealBuildTool;

// Presentation events and their effects table (Plugins/GSTNPresentation/README.md). Allowed edges: engine modules and GSTNAnimationCore
// (the short additive goes through its directional additive node). Presentation code: the combat plugins and the project may depend on it,
// no simulation code may call it, and it must never depend on GameplayAbilities, AbilitySystemSimulation, NetworkPrediction or Mover.
public class GSTNPresentationCore : ModuleRules
{
	public GSTNPresentationCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNAnimationCore",
			"PhysicsCore"  // EPhysicalSurface in the presentation component (VFX v3 floor colour)
		});

		PrivateDependencyModuleNames.Add("Niagara");
	}
}
