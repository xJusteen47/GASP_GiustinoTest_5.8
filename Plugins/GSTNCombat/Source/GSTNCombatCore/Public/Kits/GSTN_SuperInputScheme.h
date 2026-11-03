// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_InputSlot.h"
#include "GSTN_SuperInputScheme.generated.h"

/** How a kit's supers are entered */
UENUM(BlueprintType)
enum class EGSTN_SuperInputMode : uint8
{
	/** One fixed input per super slot (a button chord or a motion): the slot decides which super */
	FixedSlots,
	/** The player enters a code per super: the compiler builds each super's slot from CodeEntry plus the super's own code */
	PerSuperCode
};

/**
 * How a kit's supers are entered (combat-m3 8). Pure data shared by kits: the compiler of the binding table reads it, nothing
 * here knows what a super does. Primary asset type GSTN_SuperInputScheme.
 */
UCLASS(BlueprintType)
class GSTNCOMBATCORE_API UGSTN_SuperInputScheme : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Super Input")
	EGSTN_SuperInputMode Mode = EGSTN_SuperInputMode::FixedSlots;

	/** FixedSlots: one slot per super slot, in slot order. The slot's command is the super slot's command */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Super Input", meta = (EditCondition = "Mode == EGSTN_SuperInputMode::FixedSlots", EditConditionHides))
	TArray<FGSTN_InputSlotSpec> FixedSlots;

	/**
	 * PerSuperCode: the template every super's slot starts from, i.e. the hold or release frame around the code the player
	 * types. Its command is replaced by the super slot's command when the table is compiled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Super Input", meta = (EditCondition = "Mode == EGSTN_SuperInputMode::PerSuperCode", EditConditionHides))
	FGSTN_InputSlotSpec CodeEntry;

	/** Carried while the code is being entered (so a binding can require or ignore it). A state tag of any root */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Super Input", meta = (Categories = "Combat.State"))
	FGameplayTag GrantedWhileEntering;

	/** Findings of the scheme, shared by IsDataValid and tests. Errors make the asset invalid. */
	void Validate(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
