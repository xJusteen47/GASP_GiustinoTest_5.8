// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSTN_InputCoreTypes.h"
#include "Input/GSTN_InputMatcher.h"
#include "Input/GSTN_InputPattern.h"
#include "GSTN_InputSlot.generated.h"

class UGSTN_InputSettings;

/**
 * The rank of a command when several match at once (combat.md section 8.2), compared field by field: a higher tier always
 * wins whatever the pattern, then the more demanding pattern, then the manual offset. Deliberately not one summed number,
 * which would let a lot of specificity outweigh a tier.
 */
struct FGSTN_CommandPriority
{
	/** From the command tag root through the settings: special above attack */
	int32 Tier = 0;

	/** Sum of the pattern's conditions: crouch + direction above direction above bare button */
	int32 Specificity = 0;

	/** Manual tie breaker of the slot */
	int32 Offset = 0;

	bool operator==(const FGSTN_CommandPriority& Other) const
	{
		return Tier == Other.Tier && Specificity == Other.Specificity && Offset == Other.Offset;
	}

	bool operator<(const FGSTN_CommandPriority& Other) const
	{
		if (Tier != Other.Tier)
		{
			return Tier < Other.Tier;
		}
		if (Specificity != Other.Specificity)
		{
			return Specificity < Other.Specificity;
		}
		return Offset < Other.Offset;
	}
};

/**
 * A command and the input that triggers it (combat.md section 9). The command tag is the slot's identity everywhere:
 * bindings, cancel windows, AI requests, UI and events refer to the tag, never to the input, so changing how a command is
 * entered touches only its slot. A slot is plain data shared by every fighter that uses it.
 *
 * A command is the same input in every context (ground, air, wall...). What it starts depends on the character's
 * context, so the requirements live in the bindings of the command (GSTN_CommandBindings.h), not here.
 */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_InputSlotSpec
{
	GENERATED_BODY()

	/** What this input means, e.g. Command.Attack.CrouchDirectional. Unique within a slot set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot", meta = (Categories = "Command"))
	FGameplayTag Command;

	/** The input combination */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FGSTN_InputPattern Pattern;

	/** Added to the automatic rank when the tier and the pattern do not say what the designer wants */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot|Priority")
	int32 PriorityOffset = 0;

	/** Time buffer of this slot. -1 = the settings' DefaultBufferFrames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot|Buffer", meta = (ClampMin = "-1"))
	int32 BufferFramesOverride = -1;

	/** Hold buffer of this slot (Default = the settings' choice) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot|Buffer")
	EGSTN_OverrideBool HoldBuffer = EGSTN_OverrideBool::Default;

	/** Wait for a chord that could still complete before this slot fires (Default = the settings' choice, off) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot|Buffer")
	EGSTN_OverrideBool ChordGrace = EGSTN_OverrideBool::Default;

	/** Whether the stick is judged when the button is pressed or when the command comes out */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot|Buffer")
	EGSTN_DirectionPolicy DirectionPolicy = EGSTN_DirectionPolicy::AtPress;

	/**
	 * When the slot matches but cannot execute (cooldown, no meter), let lower slots use its press instead of reserving
	 * it. For versions that fall back to a plainer one: the EX move without meter becomes the normal move
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot|Priority")
	bool bFallThroughWhenUnavailable = false;

	/** Rank of the slot with the settings' tiers */
	FGSTN_CommandPriority GetPriority(const UGSTN_InputSettings& Settings) const;

	/** Whether the slot waits for chords, with the settings' default applied */
	bool UsesChordGrace(const UGSTN_InputSettings& Settings) const;

	/** Matcher params of this slot at frame Now: the settings' buffer defaults with the slot's overrides applied */
	FGSTN_MatchParams MakeMatchParams(const UGSTN_InputSettings& Settings, const FGSTN_ButtonMap& Buttons, int32 Now) const;
};

/** A slot shared as an asset (Asset Manager type GSTN_InputSlot). */
UCLASS(BlueprintType)
class GSTNINPUTCORE_API UGSTN_InputSlotAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	FGSTN_InputSlotSpec Spec;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

/** An entry of a slot set: a shared slot asset, or a spec written in place. */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_InputSlotEntry
{
	GENERATED_BODY()

	/** Shared slot. When set it is used and Inline is ignored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	TObjectPtr<UGSTN_InputSlotAsset> Asset;

	/** The slot written here, when no asset is given */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot", meta = (EditCondition = "!Asset", EditConditionHides))
	FGSTN_InputSlotSpec Inline;

	/** The spec this entry stands for */
	const FGSTN_InputSlotSpec& GetSpec() const { return Asset ? Asset->Spec : Inline; }
};

/**
 * A list of slots that are meant to live together: the plugin's standard set, a weapon's extra inputs, a kit's moves.
 * Slots are checked against each other for conflicts (UGSTN_InputSlotValidation).
 */
UCLASS(BlueprintType)
class GSTNINPUTCORE_API UGSTN_InputSlotSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slots")
	TArray<FGSTN_InputSlotEntry> Slots;

	/** The specs in order, for the resolver and the validator */
	void GetSpecs(TArray<const FGSTN_InputSlotSpec*>& OutSpecs) const;

#if WITH_EDITOR
	/** Reports the findings of UGSTN_InputSlotValidation: ambiguous and duplicate slots as errors, shadowed ones as warnings */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
