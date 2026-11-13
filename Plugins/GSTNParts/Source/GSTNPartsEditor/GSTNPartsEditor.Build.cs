using UnrealBuildTool;

// Editor tooling of the destructible parts (plan P13): the asset validator, the balance report and the Physics Asset draft helper.
// Allowed edges: engine editor modules and GSTNPartsCore. It must not depend on GAS, Network Prediction, Mover or a presentation
// module, and no runtime module may name this one (an Editor module is not compiled into a game target).
public class GSTNPartsEditor : ModuleRules
{
	public GSTNPartsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GSTNPartsCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"DataValidation",
			// UBodySetupCore::BoneName, read by the Physics Asset draft helper
			"PhysicsCore"
		});
	}
}
