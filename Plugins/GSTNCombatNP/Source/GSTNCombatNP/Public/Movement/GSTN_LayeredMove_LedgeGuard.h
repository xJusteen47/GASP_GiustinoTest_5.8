// The Stop action of the Ledge Check track (owner 2026-11-16, FGSTN_TimelineTrack_LedgeCheck).

#pragma once

#include "CoreMinimal.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GameplayEffect.h"
#include "LayeredMove.h"
#include "GSTN_LayeredMove_LedgeGuard.generated.h"

/**
 * Holds a character in place at a ledge while its move plays on: the Stop action of a Ledge Check track.
 *
 * Queued (empty) by the combat component when a timeline with a Stop ledge window begins (UGSTN_CombatSimComponent::ExecuteLedgeGuard, from the
 * ability like the momentum slide). The decision is the combat clock's, not this move's: the clock probes the floor ahead in the combat tick and
 * writes FGSTN_CombatSyncCore::bLedgeStop, which this move reads from Mover's tick of the same frame (the combat model ticks before Mover), through
 * UGSTN_CombatSimComponent::ReadLedgeStop. One probe, one decision, on every machine and in every re-simulation.
 *
 * Every frame: while the timeline it started with runs (same start frame) it either does nothing (no ledge: GenerateMove returns false, the root
 * motion, the slide and the walking mode move the character as before) or, with bLedgeStop set, overrides the velocity with zero in the ground
 * plane (the vertical part is the walking mode's ground snap's). The turn of the montage is held too for that frame. The move ends with its
 * timeline (DurationMs set so the next flush removes it).
 *
 * Mixing: OverrideVelocity at priority 1: wins over the montage's root motion move and the momentum slide (priority 0). A hitstop freeze (also
 * priority 1) holds the pawn already: on a frozen frame this move stands aside.
 *
 * Rollback: the only latched value is the timeline's start frame, serialized; the stop itself is read from the restored combat state.
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_LayeredMove_LedgeGuard : public FLayeredMoveBase
{
	GENERATED_BODY()

	FGSTN_LayeredMove_LedgeGuard();
	virtual ~FGSTN_LayeredMove_LedgeGuard() = default;

	/** Simulation frame the timeline the guard belongs to started on, latched on its first frame; INDEX_NONE before */
	UPROPERTY()
	int32 TimelineStartFrame = INDEX_NONE;

	/** Finished, waiting for the flush to remove it */
	UPROPERTY()
	bool bDone = false;

	virtual bool SupportsAsync() const override { return false; }
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;

private:
	void Finish(const FMoverTimeStep& TimeStep);
};

template<>
struct TStructOpsTypeTraits<FGSTN_LayeredMove_LedgeGuard> : public TStructOpsTypeTraitsBase2<FGSTN_LayeredMove_LedgeGuard>
{
	enum { WithCopy = true };
};

/** Queues an empty ledge guard on the target's Mover once */
UCLASS(DisplayName = "GSTN Ledge Guard Move Creator")
class GSTNCOMBATNP_API UGSTN_LedgeGuardMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: starts the ledge guard of the move that begins this frame (UGSTN_CombatSimComponent::ExecuteLedgeGuard) */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_LedgeGuard : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_LedgeGuard();
};
