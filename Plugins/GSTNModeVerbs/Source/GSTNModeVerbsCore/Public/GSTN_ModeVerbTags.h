// Mode verbs, pure core (no GAS, no Network Prediction, no Mover, no world).

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Native tags of the mode verbs (game-modes 8.2, owner GSTNModeVerbs). Designer verbs are Ability.Mode.<Name> leaves in the content's own ini;
 * only the verbs the frameworks name are native. Nothing here is ever granted to a pawn as simulated state: Ability.Mode.* identifies an ability,
 * Interact.Target.Objective is projected by the interact probe, Message.Mode.* travel on the authoritative event stream (server only).
 */
namespace GSTNModeVerbTags
{
	/** Root of the verbs a mode adds as abilities. Every UGSTN_GA_ModeVerb carries it, so one cancel-by-tag stops them all */
	GSTNMODEVERBSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Mode);

	/** Activate a terminal, arm a charge, open a gate (the target is the interact focus, Interact.Target.Objective) */
	GSTNMODEVERBSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Mode_Interact);
	/** Call a reinforcement (GSTNRespawn handles it; aim point) */
	GSTNMODEVERBSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Mode_CallReinforcement);
	/** Call a supply drop (aim point) */
	GSTNMODEVERBSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Mode_CallDrop);
	/** Call extraction (the extraction console is an interact target) */
	GSTNMODEVERBSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Mode_CallExtraction);

	/** The command of the Interact verb: Hold(Interact) requiring Interact.Target.Objective. A binding in the kit's slot set (data), not code */
	GSTNMODEVERBSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Mode_Interact);

	/** The meaning the interact probe projects while a requestable prop is the focus (leaf owned by GSTNModeVerbs, root by GSTNInteract) */
	GSTNMODEVERBSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Target_Objective);

	/**
	 * A verb was confirmed AND validated by the host, once (Instigator = the pawn, Target = the prop if any, Magnitude = the ability's, Frame = the
	 * confirmation frame; Tags = the verb tag plus the prop's identity tags, e.g. Mission.Objective.*). Missions and respawn count this; nothing else
	 * may count a verb.
	 */
	GSTNMODEVERBSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Mode_Verb);

	/**
	 * Raw seam D-1 key: a timeline Event key whose OutboxEventTag is this (Tags = the verb tag) says "this verb is confirmed" for a verb that needs
	 * no target (an aim or no-target verb). UGSTN_VerbSubsystem validates it and republishes Message.Mode.Verb. Not for terminals: those go
	 * through UGSTN_GA_ModeVerb, which names no target in a timeline key.
	 */
	GSTNMODEVERBSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Mode_VerbConfirmed);

	/** The host refused a confirmed verb (Magnitude = EGSTN_VerbRefusal, Tags = the verb tag). It changes no state; UI and tests listen */
	GSTNMODEVERBSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Mode_VerbRefused);

	/** Appends every tag this module declares, so tests and validators cannot miss one */
	GSTNMODEVERBSCORE_API void GetDeclaredTags(TArray<FGameplayTag>& OutTags);
}
