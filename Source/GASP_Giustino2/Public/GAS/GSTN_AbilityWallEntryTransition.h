// Gameplay Ability System setup (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_WallEntryTransition.h"
#include "GameplayAbilitySpecHandle.h"
#include "GSTN_AbilityWallEntryTransition.generated.h"

class UNpAbilitySystemComponent;

/**
 * UGSTN_WallEntryTransition that makes grabbing a wall a gameplay ability.
 *
 * Same detection and rules as the base transition (all its settings apply unchanged), plus: the wall is grabbed only if
 * an ability with one of AbilityTags (by default UGSTN_GA_WallSlide) is granted on the owner's UNpAbilitySystemComponent
 * and can activate now (blocked/required tags, cooldown, cost). When the wall is grabbed the ability is activated.
 *
 * Use it in place of UGSTN_WallEntryTransition in the Transitions of the Falling mode. The ability system ticks before
 * Mover in the fixed tick and its state is predicted and rolled back by Network Prediction, so the check gives the same
 * answer on the owning client and on the server for the same frame.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced, DisplayName = "GSTN Wall Entry (Gameplay Ability)")
class GASP_GIUSTINO2_API UGSTN_AbilityWallEntryTransition : public UGSTN_WallEntryTransition
{
	GENERATED_BODY()

public:
	UGSTN_AbilityWallEntryTransition(const FObjectInitializer& ObjectInitializer);

	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;

	/** Asset tags of the ability that grants wall entry. The first granted ability with any of them is used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Ability")
	FGameplayTagContainer AbilityTags;

	/**
	 * Behave like the base transition when the owner has no UNpAbilitySystemComponent (e.g. a pawn without GAS).
	 * Off: pawns without an ability system can never grab walls
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Ability")
	bool bAllowWithoutAbilitySystem = true;

protected:
	UNpAbilitySystemComponent* GetAbilitySystem() const;

	/** The granted wall ability that can activate now, or an invalid handle */
	FGameplayAbilitySpecHandle FindActivatableWallAbility(UNpAbilitySystemComponent* AbilitySystem) const;
};
