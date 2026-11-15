// Combat simulation (GAS-NPP fork). Per-move movement capsule (owner 2026-11-15, dropkick).

#pragma once

#include "CoreMinimal.h"
#include "InstantMovementEffect.h"
#include "MovementModifier.h"
#include "Timeline/GSTN_BodyCapsule.h"
#include "GSTN_BodyCapsuleModifier.generated.h"

class UMoverComponent;

/**
 * Permanent Mover modifier that gives the movement capsule the shape and place the running move asks for (FGSTN_TimelineTrack_BodyCapsule;
 * the pawn's own capsule outside every track). Queued with the stats modifier by UGSTN_CombatSimComponent, never ends.
 *
 * State: Applied, the shape the capsule has now, lives in the modifier, i.e. in Mover's sync state: a rollback restores it, the server's
 * state carries it to the other machines. The capsule's size and the mesh offset are NOT state (component properties): they are assigned
 * from Applied on every substep (OnPreMovement) and on every presented frame (ApplyPresentation, also on simulated proxies), so a restored
 * or replicated Applied always shows its own capsule, never a stale one.
 *
 * Per substep: the target comes from the combat state of the frame being simulated (the combat model ticked before Mover) through
 * ReadEvaluatedTimeline; FGSTN_BodyCapsule::Step gives the new Applied and how far the actor moves. A move is done by
 * FGSTN_BodyCapsuleShiftEffect, queued on the same substep: Mover applies it before the movement mode generates its move and starts the
 * mode from the moved location. Mesh offset: mesh relative location = its archetype's + (standing offset - Applied.Offset), so the mesh
 * root sits at actor - Applied.Offset (Mover's base visual transform, which the smoothing uses too).
 *
 * Pushbox (FGSTN_Pushbox): the same substep also queues FGSTN_PushboxShiftEffect with the move the combat tick of the frame computed
 * (UGSTN_CombatSimComponent::ReadFramePush). Fighters' capsules ignore each other, so this soft capped push is what keeps them apart.
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_BodyCapsuleModifier : public FMovementModifierBase
{
	GENERATED_BODY()

	FGSTN_BodyCapsuleModifier();

	/** The capsule now. HalfHeight 0 = the pawn's own (not changed yet) */
	UPROPERTY()
	FGSTN_BodyCapsuleShape Applied;

	virtual void OnPreMovement(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep) override;
	virtual FMovementModifierBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;

	/** The pawn's own capsule (its archetype's), from MoverComp's owner. False without a capsule */
	static bool GetStanding(const UMoverComponent& MoverComp, FGSTN_BodyCapsuleShape& OutStanding);

	/** Sets the capsule size and the mesh offset of Shape (no move). Changes nothing already right. Returns whether something changed */
	static bool ApplyShape(UMoverComponent& MoverComp, const FGSTN_BodyCapsuleShape& Shape, const FGSTN_BodyCapsuleShape& Standing);

	/** Presentation, every machine (simulated proxies too): the shape of the modifier in MoverComp's current sync state. Called by the combat component's tick */
	static void ApplyPresentation(UMoverComponent& MoverComp);
};

template<>
struct TStructOpsTypeTraits<FGSTN_BodyCapsuleModifier> : public TStructOpsTypeTraitsBase2<FGSTN_BodyCapsuleModifier>
{
	enum { WithCopy = true };
};

/**
 * Moves the actor by the body capsule's step and gives the capsule its new shape, on the substep FGSTN_BodyCapsuleModifier queued it. A
 * growing capsule moves first then grows, a shrinking one shrinks first then moves, so the swept move never starts inside the floor. Body
 * anchor moves are swept (a wall stops the capsule; the mesh then sits that much off the body for the move), except a growth whose grown
 * capsule fits at the destination, which is set there directly as Mover's stance modifier does; Capsule anchor moves only keep the bottom
 * and are set directly. Queued for its own substep (QueueInstantMovementEffectThisStep_Internal), never with the network scheduling delay
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_BodyCapsuleShiftEffect : public FInstantMovementEffect
{
	GENERATED_BODY()

	UPROPERTY()
	FGSTN_BodyCapsuleShape From;

	UPROPERTY()
	FGSTN_BodyCapsuleShape To;

	/** In the capsule's frame (X forward, Z up) */
	UPROPERTY()
	FVector ShiftLocal = FVector::ZeroVector;

	UPROPERTY()
	bool bSweep = true;

	virtual bool ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState) override;
	virtual FInstantMovementEffect* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
};

template<>
struct TStructOpsTypeTraits<FGSTN_BodyCapsuleShiftEffect> : public TStructOpsTypeTraitsBase2<FGSTN_BodyCapsuleShiftEffect>
{
	enum { WithCopy = true };
};

/**
 * Moves the actor by the frame's pushbox push (horizontal, at most FGSTN_PushboxSettings::MaxPushPerFrame), swept against the world: a wall or
 * a corner stops it, and the partner, still overlapping, keeps being pushed out instead (Smash corner behaviour). Queued for its own substep by
 * FGSTN_BodyCapsuleModifier. Velocity is untouched: the push is a displacement, it does not carry into the next frame
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_PushboxShiftEffect : public FInstantMovementEffect
{
	GENERATED_BODY()

	/** World space */
	UPROPERTY()
	FVector Move = FVector::ZeroVector;

	virtual bool ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState) override;
	virtual FInstantMovementEffect* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
};

template<>
struct TStructOpsTypeTraits<FGSTN_PushboxShiftEffect> : public TStructOpsTypeTraitsBase2<FGSTN_PushboxShiftEffect>
{
	enum { WithCopy = true };
};