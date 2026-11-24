using UnrealBuildTool;

// Modular cosmetics, pure layer (skin-cosmetici-modulari.md §5): theme and host data, the deterministic placement plan, validation, the
// cache key. Allowed edges: engine modules and gameplay tags only. Never the combat, ability or simulation modules: cosmetics are
// presentation and nothing in the simulation may depend on them. The assembler (mesh fusion, phase 2) gets its own module.
public class GSTNCosmeticsCore : ModuleRules
{
	public GSTNCosmeticsCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags"
		});
	}
}
