// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_CombatSimTypes.h"

/** What the swap step decided for the frame */
enum class EGSTN_LoadoutSwapResult : uint8
{
	/** The simulation is on the demanded table: nothing to do */
	Unchanged,

	/** The frame is the boundary: the state moved to the demanded table and the old kit's moves ended */
	Swapped,

	/** The demanded table is not here (the loadout has not reached this machine yet): the frame runs with no commands */
	Closed
};

/**
 * The loadout table swap of the combat tick (loadout-table-swap-design 2c), as a function of the sync state so it is tested without a
 * world. It runs once per frame at the top of the tick, before any step reads the table, so every step of a frame sees one table.
 *
 * The demand is FGSTN_CombatSyncCore::LoadoutRevision (the server's out-of-band write); TableRevision is what the simulation has swapped
 * to. The first frame that has the demanded table is the boundary, on every machine: a client that reconciles to a state already past
 * the boundary adopts the result without running it. Nothing here reads a role, a clock or a table: the same state and frame give the
 * same result in every re-simulation.
 */
struct GSTNCOMBATNP_API FGSTN_LoadoutSwapLogic
{
	/**
	 * @param Core The state this frame writes (a copy of the previous frame's, requests of older frames pruned)
	 * @param bDemandedTablePresent Whether a table set exists for Core.LoadoutRevision on this machine
	 * @param Frame The frame being simulated
	 *
	 * On Swapped: a running timeline is ended (CancelTimeline request carrying the OLD registry index, so the ability system's pre-tick of
	 * this frame cancels the old kit's ability, and the action state returns to Actionable); the active weapon returns to the primary; every
	 * buffered press and stick anchor made before this frame is used up (presses made for the old kit must not fire the new kit's command);
	 * TableRevision becomes LoadoutRevision. The jump state is kept (air jumps left, coyote time are the character's, not the kit's).
	 */
	static EGSTN_LoadoutSwapResult Step(FGSTN_CombatSyncCore& Core, bool bDemandedTablePresent, int32 Frame);
};
