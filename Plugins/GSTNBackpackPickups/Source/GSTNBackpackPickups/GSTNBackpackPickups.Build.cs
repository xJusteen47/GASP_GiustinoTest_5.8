using UnrealBuildTool;

// Bridge: the backpack pickup payload. Allowed edges: the backpack's pure layer and the pickups plugin, nothing else.
public class GSTNBackpackPickups : ModuleRules
{
	public GSTNBackpackPickups(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GSTNCombatCore",
			"GSTNBackpackCore",
			"GSTNPickups"
		});
	}
}
