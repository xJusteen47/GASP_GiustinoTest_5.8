// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Jump/GSTN_JumpTypes.h"

/** What the movement looked like at the end of the previous frame, from the Mover sim-state tracker, relative to the local gravity */
struct GSTNCOMBATCORE_API FGSTN_JumpMoverSample
{
	bool bGrounded = true;

	/** Speed along up = -GravityDir, positive moving away from the ground (FGSTN_JumpLogic::UpSpeed) */
	double UpSpeed = 0.0;

	/** The last step started a launch that was not the combatant's own jump */
	bool bExternalLaunch = false;
};

/** Which jump a request would be */
enum class EGSTN_JumpKind : uint8
{
	None,
	Ground,
	Air
};

/** Result of FGSTN_JumpLogic::SimulateApex */
struct GSTNCOMBATCORE_API FGSTN_JumpApex
{
	/** Highest point along up, from the take-off point */
	double Height = 0.0;

	/** Where the highest point is, from the take-off point */
	FVector Position = FVector::ZeroVector;

	/** Frames from the take-off to the highest point */
	int32 Frames = 0;
};

/**
 * The jump state machine and its helpers (combat-m3 7.9, combat.md 11b), pure: every direction is relative to a GravityDir the
 * caller passes in (up = -GravityDir), so a combatant under tilted, sideways or inverted gravity jumps, fast falls and air jumps
 * along its own up. No UWorld, no globals; the same inputs give the same state on every machine and in every re-simulation.
 */
struct GSTNCOMBATCORE_API FGSTN_JumpLogic
{
	/** Speed along local up for a velocity under the gravity direction GravityDir (zero gravity gives 0) */
	static double UpSpeed(const FVector& Velocity, const FVector& GravityDir);

	/**
	 * One frame of bookkeeping, before the input of the frame is looked at. Grounded: everything resets and the coyote window
	 * opens. Airborne: the coyote window runs down, the hold ends after MaxHoldFrames or, once MinHoldFrames have passed, when
	 * the button is not held, a launch by something else is applied through Rules, and the phase follows the speed along up.
	 * bButtonHeld is whether the button that started the jump (State.Button) is still held
	 */
	static FGSTN_JumpState Step(const FGSTN_JumpState& In, int32 Frame, const FGSTN_JumpMoverSample& Sample, bool bButtonHeld, const FGSTN_JumpParams& Params, const FGSTN_JumpResetRules& Rules);

	/** The jump a request would be now: a ground jump on the ground or inside the coyote window, an air jump while some are left, else none */
	static EGSTN_JumpKind ClassifyJump(const FGSTN_JumpState& State, bool bGrounded, const FGSTN_JumpParams& Params);

	/** Fast fall is allowed in the air, while descending, once */
	static bool CanFastFall(const FGSTN_JumpState& State, bool bGrounded);

	/** The take-off of a request of this frame: rising from Frame, the button's hold begins, an air jump uses one of the air jumps */
	static FGSTN_JumpState ApplyTakeoff(const FGSTN_JumpState& In, int32 Frame, int32 Button, EGSTN_JumpKind Kind);

	static FGSTN_JumpState ApplyFastFall(const FGSTN_JumpState& In);

	/** The multiplier of gravity for the state: the hold, cut, fall or fast fall scale of Params; 1 for no jump or a launch */
	static float GravityScale(const FGSTN_JumpState& State, const FGSTN_JumpParams& Params);

	/**
	 * GravityScale, lowered so that one step of gravity does not take the speed along local down past the terminal speed of the state
	 * (FastFallSpeed while fast falling, else MaxFallSpeed; <= 0 = none). A cap on the acceleration, so nothing is written to the
	 * velocity: DownSpeed is the speed along down at the start of the step, GravityMagnitude the authored gravity. A speed already
	 * past the cap (a launch downwards) is kept, not reduced: the scale is 0 then.
	 */
	static float LimitedGravityScale(const FGSTN_JumpState& State, const FGSTN_JumpParams& Params, double DownSpeed, double GravityMagnitude, double DeltaSeconds);

	/**
	 * Jumps under constant gravity of magnitude GravityMagnitude along GravityDir with the button held for HoldFrames frames
	 * (semi-implicit Euler, one frame of DeltaTime per step) and returns the highest point along up. It drives the state machine, so
	 * the variable height of the real simulation is the one measured. For tests and for tools that show the height a stat set gives
	 */
	static FGSTN_JumpApex SimulateApex(const FVector& GravityDir, double GravityMagnitude, double JumpSpeed, int32 HoldFrames, const FGSTN_JumpParams& Params, double DeltaTime = 1.0 / 60.0);
};
