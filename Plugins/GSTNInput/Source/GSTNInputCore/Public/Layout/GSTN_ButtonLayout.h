// Button layout (IO-2): which logical button (Input.Button.*) triggers which function.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameRules/GSTN_GameRules.h"

#include "GSTN_ButtonLayout.generated.h"

/** One function of the layout and the logical button that triggers it */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_ButtonAssignment
{
	GENERATED_BODY()

	/**
	 * The function, named by the plugin that reads it ("Held.PickupDrop", "Held.Throw", "Held.KitSwap"...). A name and not a tag so a
	 * plugin adds its functions without declaring tags here
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FName Function;

	/** The logical button; the input config maps it to a key and a pad button, and IO-1 lets each player move that key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (Categories = "Input.Button"))
	FGameplayTag Button;
};

/**
 * The single button layout: logical button -> function, for every plugin (replaces the button fields that were in Project Settings). A
 * GameRules piece, so a mode or level picks its own layout with its GameRules asset; its class defaults are the shipped layout, from the
 * plugin's DefaultGSTNInput.ini (section [/Script/GSTNInputCore.GSTN_ButtonLayout]; a config class, not in Project Settings). A function
 * missing here falls back to the reading plugin's own default, so defaults never change by omission.
 *
 * Read when the kits compile their slots (every machine, same rules). Per-player choices (IO-3) are meant to layer on top of this piece,
 * per local player, never by editing it.
 */
UCLASS(config = GSTNInput, meta = (DisplayName = "Button layout"))
class GSTNINPUTCORE_API UGSTN_ButtonLayout : public UGSTN_GameRulesPiece
{
	GENERATED_BODY()

public:
	/** The active layout (the GameRules asset's piece, else these class defaults) */
	static const UGSTN_ButtonLayout& Get() { return FGSTN_ActiveGameRules::GetPiece<UGSTN_ButtonLayout>(); }

	/** The button of Function in the active layout, else Default */
	static FGameplayTag GetButton(FName Function, const FGameplayTag& Default);

	/** The button of Function in this layout, or an empty tag */
	FGameplayTag FindButton(FName Function) const;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (TitleProperty = "Function"))
	TArray<FGSTN_ButtonAssignment> Assignments;
};
