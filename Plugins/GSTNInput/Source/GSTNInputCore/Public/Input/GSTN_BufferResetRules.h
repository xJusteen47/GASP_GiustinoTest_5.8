// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "GSTN_BufferResetRules.generated.h"

struct FGSTN_ButtonMap;
struct FGSTN_InputHistory;

/**
 * What happened this frame that reset rules may react to. Filled by the caller from facts it already has in its own
 * rolled back state (the host tick: its events and the tags newly added to the owner), so the rules stay pure.
 */
struct FGSTN_BufferFrameEvents
{
	/** Events of the host simulation this frame, e.g. Input.BufferEvent.AbilityActivated, .Landed, .HitReceived */
	FGameplayTagContainer EventTags;

	/** Gameplay tags that appeared on the owner this frame */
	FGameplayTagContainer TagsAdded;
};

/**
 * A reason to throw away buffered presses. A rule that fires marks the unconsumed presses of its buttons as consumed,
 * which ends both their time buffer and their hold buffer; a button held through the reset needs a new press to act again.
 *
 * Rules are structs in lists at global, weapon and move level, applied in that order (the caller concatenates by calling
 * ApplyResetRules once per list). They look only at the history, the frame events and the frame, so they are
 * deterministic and rollback safe.
 */
USTRUCT(BlueprintType, meta = (Hidden))
struct GSTNINPUTCORE_API FGSTN_BufferResetRule
{
	GENERATED_BODY()

	virtual ~FGSTN_BufferResetRule() = default;

	/** Buttons the rule flushes. Empty = every button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reset", meta = (Categories = "Input.Button"))
	FGameplayTagContainer Buttons;

	/** Should the buffer be flushed this frame? */
	virtual bool ShouldFlush(const FGSTN_BufferFrameEvents& Events, const FGSTN_InputHistory& History, int32 Now) const
	{
		return false;
	}

	/** Tags the rule listens to, so the owner can watch them (combat.md section 8.1: the watched tag mask) */
	virtual void GetWatchedTags(FGameplayTagContainer& OutTags) const {}
};

/** Flush when the host simulation reports an event (a hit received, a move activated, a landing...). */
USTRUCT(BlueprintType, meta = (DisplayName = "On Event"))
struct GSTNINPUTCORE_API FGSTN_BufferResetRule_OnEvent : public FGSTN_BufferResetRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reset", meta = (Categories = "Input.BufferEvent"))
	FGameplayTag EventTag;

	virtual bool ShouldFlush(const FGSTN_BufferFrameEvents& Events, const FGSTN_InputHistory& History, int32 Now) const override
	{
		return EventTag.IsValid() && Events.EventTags.HasTagExact(EventTag);
	}
};

/** Flush when a gameplay tag appears on the owner. */
USTRUCT(BlueprintType, meta = (DisplayName = "On Tag Added"))
struct GSTNINPUTCORE_API FGSTN_BufferResetRule_OnTagAdded : public FGSTN_BufferResetRule
{
	GENERATED_BODY()

	/** A state tag of any root (granted by abilities, effects or the movement system), so no Categories filter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reset")
	FGameplayTag Tag;

	virtual bool ShouldFlush(const FGSTN_BufferFrameEvents& Events, const FGSTN_InputHistory& History, int32 Now) const override
	{
		return Tag.IsValid() && Events.TagsAdded.HasTagExact(Tag);
	}

	virtual void GetWatchedTags(FGameplayTagContainer& OutTags) const override { OutTags.AddTag(Tag); }
};

/** Flush when nothing happened on the buttons or the stick for a while: stale presses do not wait for ever. */
USTRUCT(BlueprintType, meta = (DisplayName = "After Idle Frames"))
struct GSTNINPUTCORE_API FGSTN_BufferResetRule_AfterIdleFrames : public FGSTN_BufferResetRule
{
	GENERATED_BODY()

	/** Frames without a button or stick event */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reset", meta = (ClampMin = "1"))
	int32 Frames = 30;

	virtual bool ShouldFlush(const FGSTN_BufferFrameEvents& Events, const FGSTN_InputHistory& History, int32 Now) const override;
};

/** Applies lists of reset rules to a history. */
struct GSTNINPUTCORE_API FGSTN_BufferResetRules
{
	/**
	 * Applies every rule of the list in order to History at frame Now. Call it once per list, global first, then weapon,
	 * then move.
	 * @return true if any rule fired.
	 */
	static bool Apply(FGSTN_InputHistory& History, const FGSTN_ButtonMap& Buttons,
		TConstArrayView<TInstancedStruct<FGSTN_BufferResetRule>> Rules, const FGSTN_BufferFrameEvents& Events, int32 Now);

	/** Adds the tags the rules of a list listen to */
	static void GetWatchedTags(TConstArrayView<TInstancedStruct<FGSTN_BufferResetRule>> Rules, FGameplayTagContainer& OutTags);
};
