// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Input/GSTN_InputConditions.h"
#include "GSTN_InputPattern.generated.h"

struct FGSTN_ButtonMap;
class FGSTN_InputScript;

/**
 * One step of an input combination: conditions that must hold around one frame, the step's anchor.
 *
 * The anchor condition is the first anchor-capable condition of AllOf (a press, a chord, a motion...); the matcher asks it
 * for candidate frames. Every other condition is checked relative to that frame, within SimultaneityFrames: "Back + Attack"
 * is one step with a Direction and a Press. An anchor-capable condition that is not first (a Spin next to the Press that
 * ends the move) must have its own reference event within the same tolerance of the anchor.
 */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_InputStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
	TArray<TInstancedStruct<FGSTN_InputCondition>> AllOf;

	/** Tolerance in frames around the anchor for the other conditions. -1 = the settings' DefaultSimultaneityFrames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step", meta = (ClampMin = "-1"))
	int32 SimultaneityFrames = -1;

	/** Largest distance from this step's anchor to the next step's anchor, in frames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step", meta = (ClampMin = "0"))
	int32 MaxGapFrames = 15;

	/** Index in AllOf of the anchor condition, INDEX_NONE if the step has none (an invalid step) */
	int32 FindAnchorIndex() const;

	/** Sum of the conditions' specificity */
	int32 GetSpecificity() const;
};

/**
 * An input combination: steps in the order the player enters them, the last one being the step whose anchor fires the
 * command. A pattern is plain data shared by every fighter that uses its slot; matching it never changes it.
 */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_InputPattern
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern")
	TArray<FGSTN_InputStep> Steps;

	/**
	 * Automatic priority inside a tier: the sum of the specificity of every condition. A crouch + direction attack outranks
	 * a plain direction attack, which outranks the bare button.
	 */
	int32 GetSpecificity() const;

	/** At least one step and an anchor condition in every step. OutReason says which step breaks the rule */
	bool IsStructurallyValid(FString* OutReason = nullptr) const;

	/**
	 * Adds to Script the minimal input that makes the pattern match, starting at Frame (leave room before it for holds)
	 * and leaving Frame at the terminal anchor: the frame the command fires. Used by the slot validator and by tests.
	 */
	void AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Buttons, int32& Frame) const;
};

/** What a successful match found. */
struct FGSTN_PatternMatch
{
	/** Anchor frame of every step, in step order (the last is the terminal anchor) */
	TArray<int32, TInlineAllocator<4>> StepAnchors;

	/** Button event slots the match used, in every step: the caller consumes them when the command fires */
	TArray<uint8, TInlineAllocator<8>> UsedButtonEvents;

	/** The press that triggers the command (its newest press); INDEX_NONE when the terminal step is not button based */
	int32 TerminalPressEvent = INDEX_NONE;

	/**
	 * Frame of the terminal anchor when it is a stick event (a motion, a spin), INDEX_NONE when it is made of button events.
	 * Such an anchor has no press to consume: the caller passes this frame to FGSTN_InputHistory::ConsumeSticksUpTo.
	 */
	int32 TerminalStickAnchor = INDEX_NONE;

	/** The terminal press was older than the time buffer and valid only because the button is still held */
	bool bViaHoldBuffer = false;
};
