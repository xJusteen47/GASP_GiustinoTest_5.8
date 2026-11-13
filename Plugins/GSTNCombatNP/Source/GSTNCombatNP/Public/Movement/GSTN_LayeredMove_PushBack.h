// The slide of a victim that is pushed back (combat-m4-hits 7, M4 H9).

#pragma once

#include "CoreMinimal.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GameplayEffect.h"
#include "LayeredMove.h"
#include "GSTN_LayeredMove_PushBack.generated.h"

/**
 * A horizontal slide on the ground: the victim keeps its movement mode (no Falling, no tumble, no DI) and the ground snap stays on. The
 * speed falls linearly from Velocity to zero over the move's life, so it ends at zero speed or when its duration (the hitstun) is out.
 * It stops at a ledge, where the next step has no floor under it (a short probe along Up), and at a wall, where the movement mode's own
 * collision takes the speed away: no splat, no bounce, no fall (combat-m4-hits 7, Proposal).
 *
 * Stateless like the knockback: the frame is the step time minus the start time. ElapsedFrames is written for the trace only.
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_LayeredMove_PushBack : public FLayeredMoveBase
{
	GENERATED_BODY()

	FGSTN_LayeredMove_PushBack();
	virtual ~FGSTN_LayeredMove_PushBack() = default;

	/** World space velocity, horizontal (relative to the up of Mover), cm/s */
	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	/** Fraction of the initial speed lost per frame: 1 / frames of the slide makes it reach zero exactly at the end */
	UPROPERTY()
	float DecayPerFrame = 0.f;

	UPROPERTY()
	int32 ElapsedFrames = 0;

	/** How far under the feet the ledge probe looks, cm */
	static constexpr double LedgeProbeDepth = 60.0;

	virtual bool SupportsAsync() const override { return false; }
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
};

template<>
struct TStructOpsTypeTraits<FGSTN_LayeredMove_PushBack> : public TStructOpsTypeTraitsBase2<FGSTN_LayeredMove_PushBack>
{
	enum { WithCopy = true };
};

/** Queues a push back on the target's Mover once; same SetByCaller keys as the knockback creator (velocity, decay, seconds) */
UCLASS(DisplayName = "GSTN Push Back Move Creator")
class GSTNCOMBATNP_API UGSTN_PushBackMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: slides the target with the SetByCaller velocity */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_PushBack : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_PushBack();
};
