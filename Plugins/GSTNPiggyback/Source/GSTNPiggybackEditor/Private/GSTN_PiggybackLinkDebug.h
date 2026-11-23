// Piggyback, editor part (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"

/**
 * PIE debug of links that needs every world of the process at once, which only the editor has (combat-m7-piggyback.md 9):
 *  - gstn.Combat.Debug.Anchor 1: for every parked role (a pawn whose presentation shows it on a partner) in a client world, the hurtbox anchor
 *    of that client's step snapshot (green) and of the server's (red) for the same entity, a line between them and the distance in cm. Each
 *    world shows its own latest step, so a few cm of gap while the carrier moves are the frame offset between the two, not a bug; a gap
 *    that stays when the pair stands still is (acceptance 4, under 10 cm).
 *  - gstn.Combat.Debug.LinkLog 1: one LogGSTNPiggybackEditor line per change of a pawn's shown Body link (type, role, phase, serial), per world,
 *    so a harness log shows the transitions each machine saw and in which order.
 * Both read presentation state and the snapshot only, on the game thread after the worlds ticked; nothing in the simulation sees them. The
 * module is editor only, so a -game client or a cooked game has neither.
 */
struct FGSTN_PiggybackLinkDebug
{
	/** The core ticker's callback: does nothing while both console variables are 0 */
	static bool Tick(float DeltaTime);

	/** Forgets the last link state of every pawn */
	static void Reset();
};
