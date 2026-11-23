// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_CommandBindings.h"
#include "Input/GSTN_InputPattern.h"
#include "GSTN_SuperDefinition.generated.h"

class UGameplayAbility;
class UGSTN_CombatTimeline;

/**
 * A super: an ability that spends a meter and is entered through the kit's super input scheme (combat-m3 8). Primary asset
 * type GSTN_Super. Which kits may use it is their AllowedSupers query against SuperTags.
 */
UCLASS(BlueprintType)
class GSTNCOMBATGAS_API UGSTN_SuperDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Super")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Super")
	TSoftClassPtr<UGameplayAbility> Ability;

	/** Replaces the timeline of the ability (timeline abilities only) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Super")
	TSoftObjectPtr<UGSTN_CombatTimeline> TimelineOverride;

	/** Identity and classification of the super (a leaf under the super root, plus any classifying tags) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Super", meta = (Categories = "Super"))
	FGameplayTagContainer SuperTags;

	/** The meter the super spends, by its meter tag */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Super|Cost", meta = (Categories = "Combat.Meter"))
	FGameplayTag Meter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Super|Cost", meta = (ClampMin = "0.0"))
	float MeterCost = 0.f;

	/** The code the player enters when the kit's scheme is PerSuperCode */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Super|Input")
	FGSTN_InputPattern InputCode;

	/** What the owner must have and must not have to use the super. State tags of any root */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Super|Input")
	FGSTN_TagRequirements Requirements;

	/** Errors make the asset invalid. */
	void Validate(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
