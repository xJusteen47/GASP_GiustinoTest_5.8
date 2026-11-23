// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Sync/GSTN_EntityRef.h"

class AActor;
class UGSTN_LinkRegistry;
class UGSTN_LinkTypeDefinition;
struct FGSTN_LinkFragment;
struct FGSTN_LinkState;

/**
 * Which other entity's capsule an entity ignores this frame because of a link (M6 F15, HANDOVER 61 OPEN 2), as a function of its own link
 * fragment only:
 *  - while its Body link is live (Active or Exiting, a role held) and the type ignores partner collision (the parked role's
 *    bCollisionWithPartner false): the partner;
 *  - after that link ended, for the type's ExitCollisionIgnoreFrames from the exit frame: the former partner (the window both sides write
 *    at the break from the same stamped frame);
 *  - otherwise nobody.
 *
 * Why it is simulation state: the capsule sweep of Mover reads the moving component's ignore list, which is a component field outside the
 * sync state. Until F15 the presentation component set it from the state each machine SHOWS, so the grabber's client (whose partner is
 * interpolated, a few frames behind) restored the collision later than the authority, and a rollback never restored it: the authority's
 * grabber stopped against the released partner while its client walked on (the link-end second correction). Now the link tick extension
 * derives the ignored actor from the fragment of the frame being simulated, before that frame's Mover tick, and rewrites the field every
 * simulated frame (as the stats modifier rewrites Mover's gravity override), so a resimulation sweeps exactly as the first pass did.
 * Each entity writes its own capsule's list only; both fragments carry the window, so the pair stays symmetric.
 */
struct GSTNLINKNP_API FGSTN_LinkExitCollision
{
	/**
	 * Opens the window on a fragment whose Body link Ending is about to be cleared (the watchdog's Break command, a throw on either side):
	 * the former partner until ExitFrame + the type's ExitCollisionIgnoreFrames. Nothing for a Hand link, a type that keeps partner
	 * collision, a zero window or a state without partner. True when the fragment changed
	 */
	static bool Begin(FGSTN_LinkFragment& Fragment, const FGSTN_LinkState& Ending, const UGSTN_LinkTypeDefinition* Definition, int32 ExitFrame);

	/** The entity whose capsule the fragment's owner ignores on Frame (the rules above); the null reference for nobody */
	static FGSTN_EntityRef GetIgnoredPartner(const FGSTN_LinkFragment* Fragment, const UGSTN_LinkRegistry* Registry, int32 Frame);

	/** Drops a window that has run out (Frame at or past its end), so an idle entity is idle on the wire. True when the fragment changed */
	static bool DropExpired(FGSTN_LinkFragment& Fragment, int32 Frame);

	/**
	 * Makes Owner's root primitive ignore exactly Partner (null = nobody) among the actors this rule added, through the owner's link
	 * presentation component, which remembers the last one. Called every simulated frame of the owner
	 */
	static void Apply(AActor* Owner, AActor* Partner);
};
