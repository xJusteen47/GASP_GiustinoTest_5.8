using UnrealBuildTool;

// Links, Network Prediction half (m6 spec 7.1-7.4, 9 steps 3 and 4; plan F10). The link sync fragment (feature id 18), the snapshot
// contributor (link state, immunity, request tags; the parked role's hurtbox anchor on its partner's socket), the tick extension (request
// timeouts, PendingBreak on Release, the action veto of a non-acting parked role, Combat.Link.* tags, Interact.Target.Grab through the
// GSTNLinkGAS candidate provider), the parked movement mode with its modifier and transition, the presentation component and the debug
// draw; F10b adds the LinkResponse / LinkCommand inbox kinds and the GrabImmunity and LinkRequest steps. Allowed edges: engine, the NP fork
// (NetworkPrediction, Mover; GameplayAbilities for the tag projection), GSTNCombatNP (the combat model hosting the fragment),
// GSTNCombatCore, GSTNCombatGAS, GSTNInputCore, GSTNInteractCore and GSTNInteractRuntime (the probe and its meanings), GSTNLinkCore and
// GSTNLinkGAS. It must never depend on a GSTNDefence module, an Editor module or the project module (a grep gate of every M6 commit checks it).
public class GSTNLinkNP : ModuleRules
{
	public GSTNLinkNP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GameplayAbilities",
			"NetworkPrediction",
			"Mover",
			"GSTNInputCore",
			"GSTNCombatCore",
			"GSTNCombatGAS",
			"GSTNCombatNP",
			"GSTNInteractCore",
			"GSTNInteractRuntime",
			"GSTNLinkCore",
			"GSTNLinkGAS"
		});
	}
}
