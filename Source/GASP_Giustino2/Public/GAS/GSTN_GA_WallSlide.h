// Gameplay Ability System setup (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GAS/GSTN_GameplayAbility.h"
#include "DataTypes/SyncedvariableTypes/BasicSyncedVariablesTypes.h"
#include "GSTN_GA_WallSlide.generated.h"

/**
 * Being on a wall, as a gameplay ability. It has no input: UGSTN_AbilityWallEntryTransition asks whether it can
 * activate before grabbing a wall (so its blocked/required tags, cost and cooldown decide whether the character may
 * wall slide) and activates it when the wall is grabbed.
 *
 * While active it owns State.Movement.WallSliding (ActivationOwnedTags), which other abilities can use to block or
 * cancel themselves. It ends by itself once the Mover component is no longer in one of WallModeNames (jumped off,
 * detached, landed...). How the character moves on the wall stays entirely in the wall movement modes.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_WallSlide : public UGSTN_GameplayAbility
{
	GENERATED_BODY()

public:
	UGSTN_GA_WallSlide(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;

protected:
	/** Mover modes that count as "on the wall" (as registered on the Mover component) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wall Slide")
	TArray<FName> WallModeNames;

	/**
	 * Frames the ability waits for the Mover to reach a wall mode after activating. The transition activates it on the
	 * frame the mode changes, and the ability system ticks before Mover, so the new mode shows up one frame later
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wall Slide", meta = (ClampMin = "1"))
	int32 EntryGraceFrames = 2;

private:
	bool IsMoverInWallMode() const;

	/** Simulation frames since activation. A sync var: rolled back with the ability instance on corrections */
	UPROPERTY()
	FSyncInteger FramesSinceActivation;
};
