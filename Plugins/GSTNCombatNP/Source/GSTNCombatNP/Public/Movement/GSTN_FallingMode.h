// Combat simulation (GAS-NPP fork): Mover's falling mode without the start-of-fall floor snap while an attack hitbox is active.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/FallingMode.h"
#include "GSTN_FallingMode.generated.h"

struct FGSTN_CombatTimelineData;

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
 * What: while the running move has an attack hitbox active on the frame being simulated (the combat model ticked before Mover:
 * UGSTN_CombatSimComponent::ReadEvaluatedTimeline, as the body capsule modifier reads it), a start speed of exactly zero along up is handed to
 * the parent as StartNudgeSpeed downward instead, so that case does not run and the pawn lands only on a real contact (the swept move's hit).
 * Only the copy of the start state the parent reads changes: nothing is written to the sync state, the location is untouched (the parent's
 * move uses the proposed velocity, not the start one). Outside an active hitbox it is the parent, unchanged. Deterministic: the gate is
 * simulation data of the same frame on every machine
 */
UCLASS(Blueprintable, BlueprintType)
class GSTNCOMBATNP_API UGSTN_FallingMode : public UFallingMode
{
	GENERATED_BODY()

public:
	/** Skip Mover's start-of-fall floor snap while the running move has an attack hitbox active on the frame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mover|Combat")
	bool bNoFloorSnapWhileHitboxActive = true;

	/**
	 * Downward start speed handed to the parent in place of an exact zero (only to get past its IsNearlyZero test, 1e-4): tiny, it moves
	 * nothing (the parent integrates the proposed velocity)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mover|Combat", meta = (ClampMin = "0.001", ForceUnits = "cm/s", EditCondition = "bNoFloorSnapWhileHitboxActive"))
	float StartNudgeSpeed = 0.01f;

	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	/** Whether Data has an attack hitbox track (not a hurtbox track) holding Frame. False for null Data or INDEX_NONE. Pure, for the tests */
	static bool IsAttackHitboxActive(const FGSTN_CombatTimelineData* Data, int32 Frame);

	/**
	 * The start velocity the parent should read: Velocity with its component along Up replaced by -NudgeSpeed when that component is nearly zero
	 * (the parent's own test), else Velocity unchanged. Pure, for the tests
	 */
	static FVector NudgeStartVelocity(const FVector& Velocity, const FVector& Up, float NudgeSpeed);
};
