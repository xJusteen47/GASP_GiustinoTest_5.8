// Combat simulation (GAS-NPP fork): Mover's falling mode whose start-of-fall floor snap runs only on the frame the pawn leaves the ground.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/FallingMode.h"
#include "GSTN_FallingMode.generated.h"

struct FGSTN_CombatTimelineData;

/** When Mover's start-of-fall floor snap may run (UGSTN_FallingMode) */
UENUM(BlueprintType)
enum class EGSTN_FallStartSnap : uint8
{
	/** Mover's own rule: every frame that starts with an exactly zero vertical speed */
	Stock,
	/** Not while the running move has an attack hitbox active (owner 2026-11-25 first choice, PR 57) */
	NotWhileHitboxActive,
	/**
	 * Only on the first airborne frame, i.e. when the previous frame ended on the ground (walking off a step, what the snap exists for). Any
	 * later frame never snaps, whatever left the speed at zero: a hitstop with or without a hitbox (armor, the victim), the SDI freeze, a grab
	 * release, an air stall, a wind that balances gravity (owner 2026-11-26)
	 */
	FirstAirFrameOnly
};

/**
 * Mover's falling mode, extended (owner 2026-11-25, dropkick "goes into the tech bounce too early, even when it seems it should not"; owner
 * chose extending the class over editing the Mover fork). GASP's BP_MovementMode_Falling is reparented onto it
 * (Tools/GAS/setup_gstn_falling_mode.py), so its GenerateMove override (the turn) stays as it is.
 *
 * Why: UFallingMode::SimulationTick has a "just starting to fall" case: when the vertical speed at the start of the frame is EXACTLY zero and
 * the move goes down, it looks for a walkable floor up to FloorSweepDistance (40 cm) under the capsule and, if it finds one, puts the pawn on it
 * and lands it on that frame, with no real contact. It exists for walking off a small step. In combat the vertical speed is exactly zero after
 * every hitstop (FGSTN_LayeredMove_Freeze overrides it to zero) and at a held apex: an aerial that hits near the ground landed on the first
 * frame after its hitstop, up to 40 cm above the floor, and the dropkick's KnockdownOnLanding window downed its user at once.
 *
 * What (FallStartSnap, default FirstAirFrameOnly since owner 2026-11-26: "with super armor and hitstop, or wind from below, we would still
 * snap"): when the snap must be blocked on the frame, a start speed of exactly zero along up is handed to the parent as StartNudgeSpeed downward
 * instead, so that case does not run and the pawn lands only on a real contact (the swept move's hit). FirstAirFrameOnly blocks it on every
 * frame but the first airborne one (previous frame ended with Mover.IsOnGround, read as the body capsule modifier reads it); NotWhileHitboxActive
 * only while the running move has an attack hitbox (UGSTN_CombatSimComponent::ReadEvaluatedTimeline). Only the copy of the start state the
 * parent reads changes: nothing is written to the sync state, the location is untouched (the parent's move uses the proposed velocity, not the
 * start one). Deterministic: the gates are simulation data every machine reads the same way
 */
UCLASS(Blueprintable, BlueprintType)
class GSTNCOMBATNP_API UGSTN_FallingMode : public UFallingMode
{
	GENERATED_BODY()

public:
	/** When Mover's start-of-fall floor snap may run */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mover|Combat")
	EGSTN_FallStartSnap FallStartSnap = EGSTN_FallStartSnap::FirstAirFrameOnly;

	/**
	 * Downward start speed handed to the parent in place of an exact zero (only to get past its IsNearlyZero test, 1e-4): tiny, it moves
	 * nothing (the parent integrates the proposed velocity)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mover|Combat", meta = (ClampMin = "0.001", ForceUnits = "cm/s", EditCondition = "FallStartSnap != EGSTN_FallStartSnap::Stock"))
	float StartNudgeSpeed = 0.01f;

	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	/**
	 * Whether the snap must be blocked on this frame. bPreviousFrameGrounded: the last finished frame ended on the ground (Mover.IsOnGround, as
	 * the body capsule modifier reads it); bHitboxActive: the running move has an attack hitbox on the frame. Pure, for the tests
	 */
	static bool ShouldBlockSnap(EGSTN_FallStartSnap Policy, bool bPreviousFrameGrounded, bool bHitboxActive);

	/** Whether Data has an attack hitbox track (not a hurtbox track) holding Frame. False for null Data or INDEX_NONE. Pure, for the tests */
	static bool IsAttackHitboxActive(const FGSTN_CombatTimelineData* Data, int32 Frame);

	/**
	 * The start velocity the parent should read: Velocity with its component along Up replaced by -NudgeSpeed when that component is nearly zero
	 * (the parent's own test), else Velocity unchanged. Pure, for the tests
	 */
	static FVector NudgeStartVelocity(const FVector& Velocity, const FVector& Up, float NudgeSpeed);
};
