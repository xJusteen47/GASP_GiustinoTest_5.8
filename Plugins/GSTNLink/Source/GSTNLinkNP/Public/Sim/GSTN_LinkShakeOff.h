// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"

class UGSTN_LinkRegistry;
class UGSTN_LinkTypeDefinition;
struct FGSTN_LinkFragment;

/**
 * A link ended by one of its own roles' moves (m6 spec 8 Release, F12b shake-off): a Release key of a link outcome track on the running timeline of
 * an entity that holds a live Body link sets PendingBreak with the reason of ITS role (FGSTN_LinkRoleDef::ReleaseBreakReason): the grabber's
 * release move gives Release, the latched enemy's shake-off move gives ShakeOff. The watchdog does the rest (exit, immunity, events). Nothing here
 * names a link type: what a role's release means is the definition's data, so a drill latch needs only its definition.
 *
 * The latched enemy's shake-off is its own ability: the AI (or a command) starts a timeline whose outcome track holds the Release key; there is no
 * mash on the enemy side.
 */
struct GSTNLINKNP_API FGSTN_LinkShakeOff
{
	/** The reason index of a Release played by Role of Definition: the role's ReleaseBreakReason in the registry's reason table, Release when unset or unknown */
	static uint8 ReleaseReasonOf(const UGSTN_LinkTypeDefinition& Definition, uint8 Role, const UGSTN_LinkRegistry* Registry);

	/** PendingBreak on the fragment's live Body link with its role's release reason. False when the Body link is not live or already pending */
	static bool ApplyReleaseKey(FGSTN_LinkFragment& Fragment, const UGSTN_LinkTypeDefinition& Definition, const UGSTN_LinkRegistry* Registry);
};
