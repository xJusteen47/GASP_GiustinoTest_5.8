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
	 * mapping context the player has: the fork's ability input mapping is not involved. A soft reference, so the config does not
	 * pull the input actions of a project into every map that mentions it: UGSTN_InputConfig::ResolveActions loads them once
	 * at setup and the input source reads the resolved ones
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button")
	TSoftObjectPtr<UInputAction> Action;
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
	UFUNCTION(BlueprintPure, Category = "Input")
	int32 FindButtonIndex(const FGameplayTag& ButtonTag) const;

	/** Bit mask of a button, 0 if the config does not have it */
	uint32 GetButtonMask(const FGameplayTag& ButtonTag) const;

	/** The tag -> bit lookup the pattern matcher binds slot data to this config with */
	FGSTN_ButtonMap MakeButtonMap() const;

	/** The buttons, in bit order */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TArray<FGSTN_InputButtonDef> Buttons;

	/**
	 * The 2D move action, read as the stick on the controller (+Y up) by the player input source. Unset: the stick is
	 * always neutral. It sits next to the buttons so one asset describes everything the player drives the simulation with
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> MoveAction;

	/**
	 * Loads the input actions (synchronously, if they are not loaded yet) and keeps them for the getters below. Call it once
	 * when the host sets up (BeginPlay, a loadout change), never from a tick: the input source reads the resolved actions every
	 * frame and does not resolve anything itself. Calling it again after the asset changed refreshes them.
	 */
	void ResolveActions();

	/** The resolved action of a button, null if it has none or ResolveActions was not called */
	UInputAction* GetButtonAction(int32 ButtonIndex) const;

	/** The resolved move action, null if unset or ResolveActions was not called */
	UInputAction* GetMoveAction() const { return ResolvedMoveAction; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** Strong references to the loaded actions, parallel to Buttons: a soft pointer alone does not keep an asset alive */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> ResolvedButtonActions;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ResolvedMoveAction;
};
