// Hitstop freeze of a pawn's Mover movement (combat-m4-hits 8).

#pragma once

#include "CoreMinimal.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GameplayEffect.h"
#include "LayeredMove.h"
#include "GSTN_LayeredMove_Freeze.generated.h"

/**
 * Holds the pawn still for the hitstop window: OverrideAll with zero velocity, so whatever the movement mode, the ground or the other
 * layered moves want, the pawn does not move. Carries Combat.Mover.Frozen while active, which Mover reads from the sync state (the wall
 * modes lock on it, see UGSTN_WallMovementModeBase::IsMovementLocked).
 *
 * The velocity the pawn had when the freeze was queued is kept (IncomingVelocity) and set back when the freeze finishes, so an
 * attacker, or a victim that is not staggered, carries on as if time had stopped. A staggered victim's knockback or push replaces it
 * (it is a later move, started one frame after the window: Mover applies the finish velocity of a freeze over the proposed move of the
 * tick it finishes on, which is the first frame outside the window), and a stagger-in-place victim sets bZeroVelocityOnFinish: it leaves the freeze with
 * zero velocity. The restore uses the finish velocity settings of the layered move, which Mover already serializes, so no extra state
 * is needed to survive a rollback.
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_LayeredMove_Freeze : public FLayeredMoveBase
{
	GENERATED_BODY()

	FGSTN_LayeredMove_Freeze();
	virtual ~FGSTN_LayeredMove_Freeze() = default;

	/** The velocity the pawn had when the freeze started (cm/s, world space), restored on finish unless bZeroVelocityOnFinish */
	UPROPERTY()
	FVector IncomingVelocity = FVector::ZeroVector;

	/** The freeze ends with zero velocity instead of restoring IncomingVelocity */
	UPROPERTY()
	bool bZeroVelocityOnFinish = false;

	/** Fills the duration and the finish velocity from the arguments. The only way a freeze gets its finish velocity: call it after setting the fields */
	void Setup(const FVector& InIncomingVelocity, float InDurationMs, bool bInZeroVelocityOnFinish);

	virtual bool SupportsAsync() const override { return false; }
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	virtual void GetGameplayTags(FGameplayTagContainer& InOutTags) const override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
};

template<>
struct TStructOpsTypeTraits<FGSTN_LayeredMove_Freeze> : public TStructOpsTypeTraitsBase2<FGSTN_LayeredMove_Freeze>
{
	enum { WithCopy = true };
};

/**
 * Queues a freeze on the target's Mover once. The incoming velocity is the Mover's committed velocity: the ability system ticks before
 * Mover, so it is the end of the previous frame on every machine. The duration is the SetByCaller value Combat.SetByCaller.FreezeSeconds
 * of the spec (the hitstop window length), or, without one, the effect's own duration settings like any layered move creator.
 */
UCLASS(DisplayName = "GSTN Freeze Move Creator")
class GSTNCOMBATNP_API UGSTN_FreezeMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	/** The freeze ends with zero velocity instead of restoring the incoming one (a stagger-in-place victim) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Freeze")
	bool bZeroVelocityOnFinish = false;

	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: freezes the target for the SetByCaller duration and restores its velocity afterwards */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_Freeze : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_Freeze();
};

/** Instant: freezes the target for the SetByCaller duration and leaves it with zero velocity (a victim staggered in place) */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_FreezeZeroVelocity : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_FreezeZeroVelocity();
};
