// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_ButtonMap.h"
#include "GSTN_InputConfig.generated.h"

class UInputAction;

/** One logical button: its identity in data (the tag) and the Enhanced Input action that drives it. */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_InputButtonDef
{
	GENERATED_BODY()

	/** Identity of the button in conditions and slot data, e.g. Input.Button.Attack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button", meta = (Categories = "Input.Button"))
	FGameplayTag Tag;

	/**
	 * The action whose Started/held state is the button. Empty for a button only AI or scripts press. Mapped in any
	 * mapping context the player has: the fork's ability input mapping is not involved
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button")
	TObjectPtr<UInputAction> Action;
};

/**
 * The buttons of a character's input. The array index is the bit of the button in every input mask (command,
 * history, sync state), so the same config must be used by every machine and its order must not change while a match is
 * running. Conditions refer to buttons by tag and are bound to bits through this config, which keeps slot sets reusable
 * across configs.
 */
UCLASS(BlueprintType)
class GSTNINPUTCORE_API UGSTN_InputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Bits available in the input masks */
	static constexpr int32 MaxButtons = 32;

	/** Index (= bit) of a button, INDEX_NONE if the config does not have it */
	int32 FindButtonIndex(const FGameplayTag& ButtonTag) const;

	/** Bit mask of a button, 0 if the config does not have it */
	uint32 GetButtonMask(const FGameplayTag& ButtonTag) const;

	/** The tag -> bit lookup the pattern matcher binds slot data to this config with */
	FGSTN_ButtonMap MakeButtonMap() const;

	/** The buttons, in bit order */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TArray<FGSTN_InputButtonDef> Buttons;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
