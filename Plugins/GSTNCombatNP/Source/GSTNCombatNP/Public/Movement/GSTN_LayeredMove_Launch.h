// The launch of a surface that launches (a trampoline, M4 H11b).

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GameplayEffect.h"
#include "GSTN_LayeredMove_Launch.generated.h"

/**
 * Mover's launch (velocity replaced, Falling forced on the first frame) plus what the jump state does about it. It derives from
 * FLayeredMove_Launch on purpose: the Mover state tracker reports every child as an external launch (WasExternallyLaunchedThisFrame), so the
 * combat jump state ends a hold phase and refills or keeps the air jumps by the kit's reset rules without a line of this move's own. The flags
 * only override those rules for this launch (UGSTN_CombatSimComponent::StepJump reads them from the move in Mover's tracked state, so a rollback
 * restores them with the rest).
 *
 * The velocity is computed by the combat tick's surface probe from the surface's own frame, never from world up, so any gravity works.
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_LayeredMove_Launch : public FLayeredMove_Launch
{
	GENERATED_BODY()

	FGSTN_LayeredMove_Launch();
	virtual ~FGSTN_LayeredMove_Launch() = default;

	/** Give the air jumps back (the kit's rules say how many; this flag off keeps the count as it is) */
	UPROPERTY()
	bool bResetAirJumps = true;

	/** Restart the coyote window at its full length, so a jump right after the launch still counts as a ground jump */
	UPROPERTY()
	bool bResetCoyote = false;

	virtual bool SupportsAsync() const override { return true; }
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
};

template<>
struct TStructOpsTypeTraits<FGSTN_LayeredMove_Launch> : public TStructOpsTypeTraitsBase2<FGSTN_LayeredMove_Launch>
{
	enum { WithCopy = true };
};

/** Queues a surface launch on the target's Mover once. The velocity and the two flags travel as SetByCaller magnitudes of the spec */
UCLASS(DisplayName = "GSTN Surface Launch Move Creator")
class GSTNCOMBATNP_API UGSTN_SurfaceLaunchMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: launches the target with the SetByCaller velocity (Combat.SetByCaller.MoveVelocity*) */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_SurfaceLaunch : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_SurfaceLaunch();
};
