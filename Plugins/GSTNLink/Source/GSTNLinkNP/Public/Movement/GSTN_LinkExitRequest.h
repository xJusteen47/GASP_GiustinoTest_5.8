// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GameplayEffect.h"
#include "State/GSTN_LinkState.h"
#include "GSTN_LinkExitRequest.generated.h"

class UAbilitySystemComponent;

/**
 * The exit of a parked role (m6 spec 7.7): a teleport to the watchdog's exit transform that also sets the exit velocity (zero for a release,
 * the throw's launch later). A Mover instant effect, so it runs inside Mover's own tick and rolls back with it.
 */
USTRUCT(BlueprintType, DisplayName = "GSTN Link Exit Instant Movement Effect")
struct GSTNLINKNP_API FGSTN_LinkExitEffect : public FTeleportEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	FVector ExitVelocity = FVector::ZeroVector;

	virtual bool ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState) override;
	virtual FInstantMovementEffect* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
};

/** Makes the exit effect from the spec's set-by-caller values (the exit record of the link fragment, see FGSTN_LinkExitRequest) */
UCLASS()
class GSTNLINKNP_API UGSTN_LinkExitMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** The instant effect that carries the exit (CLAUDE.md rule 2: the link moves its entity through a Mover queued move, never directly) */
UCLASS()
class GSTNLINKNP_API UGSTN_GE_LinkExit : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_LinkExit();
};

/**
 * The exit request of a broken link (m6 spec 7.6 and 7.7), run by the link tick extension on the frame the LinkCommand's exit record is
 * written (FGSTN_LinkActions::FExit::Frame, the command's stamp + 1) when it carries GSTNLinkExitFlags::Teleport: it applies UGSTN_GE_LinkExit
 * with the exit transform and velocity as set-by-caller values. The parked modifier is cancelled on the same frame by the parked edge
 * (FGSTN_LinkedModifier::Cancel), so Mover leaves the parked mode (to Falling) and teleports in one tick. A re-simulation from a restored
 * later frame does not run it again (the record's frame is past); one from an earlier frame runs it again exactly once, like any queued move.
 *
 * Collision between the two capsules is restored by the presentation component when it stops showing the parked role (F10), not here: it is
 * not simulation state.
 */
struct GSTNLINKNP_API FGSTN_LinkExitRequest
{
	/** The set-by-caller names of the spec (names, not tags: nothing else reads them) */
	static FName PositionX();
	static FName PositionY();
	static FName PositionZ();
	static FName Yaw();
	static FName VelocityX();
	static FName VelocityY();
	static FName VelocityZ();

	/** Whether the exit record asks for the teleport on Frame */
	static bool IsDue(const FGSTN_LinkActions::FExit& Exit, int32 Frame);

	/** Applies the exit effect to the owner. False without an ability system or when the spec could not be made */
	static bool Apply(UAbilitySystemComponent* AbilitySystem, const FGSTN_LinkActions::FExit& Exit);
};
