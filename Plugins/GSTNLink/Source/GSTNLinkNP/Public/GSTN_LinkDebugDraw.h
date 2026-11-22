// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UGSTN_LinkPresentationComponent;

/**
 * Debug drawing of links (console variable gstn.Combat.Debug.Links 1; not in Shipping), from each pawn's presentation component tick on
 * every machine, so a client shows what IT believes:
 *  - a line from the entity to its Body partner in the definition's DebugColor, with "role type serial phase" text;
 *  - for a parked role: the capsule (where the simulation keeps it, its last real position), the linked transform this machine shows
 *    (GetLinkedTransform), and the hurtbox anchor of the step snapshot (the partner's snapshot transform x baked socket x offset) as three
 *    axes, so the gap between what is shown and what is hit is visible;
 *  - a predicted link (Pending) as a dashed line to the victim, and Interact.Target.Grab as a ring on the entity that projects it.
 * Reads presentation state and the snapshot only; never writes anything.
 */
struct GSTNLINKNP_API FGSTN_LinkDebugDraw
{
	/** Whether gstn.Combat.Debug.Links is on */
	static bool IsEnabled();

	/** Draws the links of one entity (called by its presentation component) */
	static void DrawEntity(const AActor* Owner, const UGSTN_LinkPresentationComponent& Presentation);
};
