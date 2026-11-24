// Combat simulation (GAS-NPP fork): the movement mixer that tells the post-generation hook a layered move overrode the velocity.

#pragma once

#include "CoreMinimal.h"
#include "MoveLibrary/MovementMixer.h"
#include "GSTN_MovementMixer.generated.h"

/**
 * Mover's mixer, with one difference: when the layered moves of a substep override the velocity (EMoveMixMode::OverrideVelocity), the
 * combined move says so.
 *
 * Why (vertical-only launches, 2026-11-24): the stock UMovementMixer::MixProposedMoves copies an OverrideVelocity layered move's velocity
 * into the mode's move but keeps the mode's mix mode (AdditiveVelocity). UGSTN_CombatSimComponent's ProcessGeneratedMovement hook (the air
 * turn, FGSTN_AirTurn, and the air momentum, FGSTN_RunStageMover::ApplyAirMomentum) reads that mix mode to leave a frame owned by a layered
 * move alone, so it never saw the override: on a pawn with a locomotion profile, a knockback's launch frame in the falling mode had its
 * horizontal velocity replaced by the air momentum's (the start velocity of the frame, zero after the hitstop freeze), and every launch went
 * straight up. A pawn without a profile (the test dummy) skips the air momentum, which is why only players flew vertically.
 *
 * The movement modes do not read the mix mode of the move they execute, so only the hook sees the difference. Pure function of the moves
 * of the substep, like the stock mixer: deterministic in a resimulation.
 *
 * Installed by UGSTN_CombatSimComponent on its Mover when the Mover has no mixer or the stock one; a Mover with a mixer of its own keeps
 * it (warning in the log), and the hook then misses override frames as before.
 */
UCLASS()
class GSTNCOMBATNP_API UGSTN_MovementMixer : public UMovementMixer
{
	GENERATED_BODY()

public:
	virtual void MixProposedMoves(const FProposedMove& MoveToMix, FVector UpDirection, FProposedMove& OutCumulativeMove) override;

	/** Installs the mixer on MoverComp (see the class comment). True when MoverComp uses it afterwards */
	static bool Install(class UMoverComponent& MoverComp);
};
