// The facing turn of a move's startup steering (plan precisione-weakpoint 5, slice 3).

#pragma once

#include "CoreMinimal.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GameplayEffect.h"
#include "InstantMovementEffect.h"
#include "GSTN_SetFacingEffect.generated.h"

/**
 * Turns the character to a world yaw on the next Mover step, keeping its location, velocity, pitch, roll, movement mode and base. It is an
 * instant effect: Mover applies it at the start of the step and copies the result into the state the movement mode starts from, so the mode
 * then simulates from the new facing (and may keep turning it toward its own orientation intent, as it always does).
 *
 * The combat tick asks for it with a SetFacing request every startup frame the aim stick steers (FGSTN_StartupSteer); the ability system's
 * pre-tick of the same frame applies UGSTN_GE_SetFacing, whose creator queues this effect. The yaw comes from the predicted command and the
 * move origin of the combat sync state, so a resimulated frame queues the same turn again.
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_SetFacingEffect : public FInstantMovementEffect
{
	GENERATED_BODY()

	/** World yaw the character faces after the effect, degrees */
	UPROPERTY()
	float YawDeg = 0.f;

	virtual bool ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState) override;
	virtual FInstantMovementEffect* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;

	/** The rotation the effect writes: OrientationDeg with its yaw replaced. Pure, for the tests */
	static FRotator MakeOrientation(const FRotator& OrientationDeg, float YawDeg);
};

template<>
struct TStructOpsTypeTraits<FGSTN_SetFacingEffect> : public TStructOpsTypeTraitsBase2<FGSTN_SetFacingEffect>
{
	enum { WithCopy = true };
};

/** Queues FGSTN_SetFacingEffect on the target's Mover; the yaw is the SetByCaller Combat.SetByCaller.FacingYaw (degrees) */
UCLASS(DisplayName = "GSTN Set Facing Move Creator")
class GSTNCOMBATNP_API UGSTN_SetFacingMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: turns the target to the SetByCaller yaw */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GE_SetFacing : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_SetFacing();
};
