// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_CombatCoreTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "GSTN_InputConditions.generated.h"

struct FGSTN_ButtonMap;
struct FGSTN_InputHistory;
class FGSTN_InputScript;

/**
 * Everything a condition may look at while it is evaluated. Conditions never see the sync state, the ability system or
 * the clock: only the history, the button map and these frame numbers, so the same call gives the same answer on the
 * server, on the client and after a rollback.
 */
struct FGSTN_ConditionContext
{
	const FGSTN_InputHistory& History;
	const FGSTN_ButtonMap& Buttons;

	/** Latest frame of the history: the frame the command would come out */
	int32 Now = 0;

	/** Anchor conditions: the oldest frame their reference event may have */
	int32 EarliestFrame = MIN_int32;

	/** Tolerance in frames around the anchor for the conditions that are checked relative to it */
	int32 SimultaneityFrames = 0;

	/** Button event slots that cannot be used: consumed elsewhere, reserved, or taken by another step */
	uint32 ExcludedEvents = 0;

	bool IsExcluded(int32 Slot) const { return (ExcludedEvents & (1u << Slot)) != 0; }
};

/** What a condition found: the frame of its reference event and the button events it used (to be consumed). */
struct FGSTN_ConditionResult
{
	/** Frame of the reference event (for anchor conditions: the candidate anchor) */
	int32 Frame = INDEX_NONE;

	/** Button event slots used; the resolver consumes them when the command fires */
	TArray<uint8, TInlineAllocator<8>> UsedEvents;

	/** The press that triggers the condition (the newest one), the only one the hold buffer can extend; none for non-press conditions */
	int32 TerminalPressSlot = INDEX_NONE;
};

/**
 * One test on the input history. Conditions are structs held as TInstancedStruct in the steps of a pattern: no UObject
 * allocation or garbage collection, trivially copied with the slot, and a new kind of input is a new struct deriving from
 * this one.
 *
 * Two roles. An *anchor* condition produces a frame (a press, a chord, the end of a motion...); the matcher asks it for
 * candidates newest first. Every other condition (direction, held button, charge) is checked relative to the anchor
 * frame, within the step's simultaneity tolerance. An anchor-capable condition placed after the anchor of its step is
 * checked relative to it too: its reference event must fall within the tolerance of the anchor.
 *
 * Only Press, Release, Chord and Mash use button events, and report them so the resolver can consume them; the rest
 * never consume anything. Implementations must stay pure and allocation free in the common path.
 */
USTRUCT(BlueprintType, meta = (Hidden))
struct GSTNCOMBATCORE_API FGSTN_InputCondition
{
	GENERATED_BODY()

	virtual ~FGSTN_InputCondition() = default;

	/**
	 * Anchor conditions: is there a reference event at or before AnchorFrame and no earlier than Ctx.EarliestFrame, not
	 * excluded by Ctx.ExcludedEvents? Reports the newest one. Calling again with the previous result's events added to
	 * ExcludedEvents gives the next older candidate.
	 * Other conditions: does the condition hold around AnchorFrame (within Ctx.SimultaneityFrames)?
	 */
	virtual bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const
	{
		return false;
	}

	/** True for conditions that produce a frame (see class comment) */
	virtual bool IsAnchorCondition() const { return false; }

	/** True for conditions about the stick: they are evaluated at the execution frame when the slot's direction policy says so */
	virtual bool IsDirectionCondition() const { return false; }

	/** Automatic priority: more demanding conditions weigh more (see the table in the M1 specification) */
	virtual int32 GetSpecificity() const { return 0; }

	/**
	 * Adds to Script the minimal input that satisfies the condition, starting at Frame and leaving Frame at the frame of
	 * the condition's reference event. Used by the slot validator to test slots against each other and by tests.
	 */
	virtual void AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Buttons, int32& Frame) const {}

	/** Adds every button tag the condition refers to (validation: the config must have them) */
	virtual void GetReferencedButtons(FGameplayTagContainer& OutButtons) const {}
};

/** A set of stick directions, edited as a checklist. */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_DirectionSet
{
	GENERATED_BODY()

	/** Bit (1 << direction) per EGSTN_StickDirection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Direction", meta = (Bitmask, BitmaskEnum = "/Script/GSTNCombatCore.EGSTN_StickDirection"))
	int32 Directions = 0;

	FGSTN_DirectionSet() = default;
	explicit FGSTN_DirectionSet(FGSTN_DirectionMask InMask) : Directions(InMask) {}

	FGSTN_DirectionMask GetMask() const { return static_cast<FGSTN_DirectionMask>(Directions); }
};

/** Rotation sense of a Spin. */
UENUM(BlueprintType)
enum class EGSTN_SpinRotation : uint8
{
	Clockwise,
	CounterClockwise,
	Either,
};

// ---------------------------------------------------------------------------------------------------------------------
// Button conditions
// ---------------------------------------------------------------------------------------------------------------------

/** A button press: an unconsumed, unreserved press of any of the buttons. Anchor. Specificity 10. */
USTRUCT(BlueprintType, meta = (DisplayName = "Press"))
struct GSTNCOMBATCORE_API FGSTN_InputCondition_Press : public FGSTN_InputCondition
{
	GENERATED_BODY()

	/** Any of these buttons */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Press", meta = (Categories = "Input.Combat.Button"))
	FGameplayTagContainer Buttons;

	virtual bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const override;
	virtual bool IsAnchorCondition() const override { return true; }
	virtual int32 GetSpecificity() const override { return 10; }
	virtual void AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const override;
	virtual void GetReferencedButtons(FGameplayTagContainer& OutButtons) const override { OutButtons.AppendTags(Buttons); }
};

/** The negative edge: a button released after being held long enough. Anchor. Specificity 10. */
USTRUCT(BlueprintType, meta = (DisplayName = "Release"))
struct GSTNCOMBATCORE_API FGSTN_InputCondition_Release : public FGSTN_InputCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Release", meta = (Categories = "Input.Combat.Button"))
	FGameplayTag Button;

	/** The press must have lasted at least this many frames (charge-and-release moves) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Release", meta = (ClampMin = "0"))
	int32 MinHeldFrames = 0;

	virtual bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const override;
	virtual bool IsAnchorCondition() const override { return true; }
	virtual int32 GetSpecificity() const override { return 10; }
	virtual void AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const override;
	virtual void GetReferencedButtons(FGameplayTagContainer& OutButtons) const override { OutButtons.AddTag(Button); }
};

/**
 * A button held for a while, ending at the anchor (within the simultaneity tolerance). MinFrames 0 means "held at the
 * anchor", which is how modifier buttons such as crouch are expressed; a modifier pressed up to the tolerance after the
 * attack press still counts. Never consumes anything and does not need an unconsumed press. Not an anchor. Specificity 5.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Hold"))
struct GSTNCOMBATCORE_API FGSTN_InputCondition_Hold : public FGSTN_InputCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hold", meta = (Categories = "Input.Combat.Button"))
	FGameplayTag Button;

	/** Frames the button must have been down when it is checked */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hold", meta = (ClampMin = "0"))
	int32 MinFrames = 0;

	virtual bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const override;
	virtual int32 GetSpecificity() const override { return 5; }
	virtual void AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const override;
	virtual void GetReferencedButtons(FGameplayTagContainer& OutButtons) const override { OutButtons.AddTag(Button); }
};

/** Several buttons pressed together: unconsumed presses of every button, all within WithinFrames. Anchor = the newest. Specificity 10 per button. */
USTRUCT(BlueprintType, meta = (DisplayName = "Chord"))
struct GSTNCOMBATCORE_API FGSTN_InputCondition_Chord : public FGSTN_InputCondition
{
	GENERATED_BODY()

	/** All of these buttons */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chord", meta = (Categories = "Input.Combat.Button"))
	FGameplayTagContainer Buttons;

	/** Largest distance between the first and the last press of the chord */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chord", meta = (ClampMin = "0"))
	int32 WithinFrames = 2;

	virtual bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const override;
	virtual bool IsAnchorCondition() const override { return true; }
	virtual int32 GetSpecificity() const override { return 10 * FMath::Max(Buttons.Num(), 1); }
	virtual void AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const override;
	virtual void GetReferencedButtons(FGameplayTagContainer& OutButtons) const override { OutButtons.AppendTags(Buttons); }
};

/** Pressing one button repeatedly: Count unconsumed presses within WithinFrames. Anchor = the newest. Specificity 5 per press. */
USTRUCT(BlueprintType, meta = (DisplayName = "Mash"))
struct GSTNCOMBATCORE_API FGSTN_InputCondition_Mash : public FGSTN_InputCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mash", meta = (Categories = "Input.Combat.Button"))
	FGameplayTag Button;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mash", meta = (ClampMin = "1"))
	int32 Count = 3;

	/** Largest distance between the first and the last press */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mash", meta = (ClampMin = "0"))
	int32 WithinFrames = 20;

	virtual bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const override;
	virtual bool IsAnchorCondition() const override { return true; }
	virtual int32 GetSpecificity() const override { return 5 * Count; }
	virtual void AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const override;
	virtual void GetReferencedButtons(FGameplayTagContainer& OutButtons) const override { OutButtons.AddTag(Button); }
};

// ---------------------------------------------------------------------------------------------------------------------
// Stick conditions
// ---------------------------------------------------------------------------------------------------------------------

/**
 * The stick in a direction set and a zone range, in a chosen space, around the anchor frame. "Stick neutral" is the
 * whole set with MaxZone Aim (a light tilt only aims); "tilted" is a set of directions with MinZone Tilt. Not an anchor.
 * Specificity 5, +3 with MinZone Tilt, +2 with a flick.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Direction"))
struct GSTNCOMBATCORE_API FGSTN_InputCondition_Direction : public FGSTN_InputCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Direction")
	EGSTN_DirectionSpace Space = EGSTN_DirectionSpace::Facing;

	/** Accepted directions in Space. The whole set (the default) accepts any stick */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Direction", meta = (Bitmask, BitmaskEnum = "/Script/GSTNCombatCore.EGSTN_StickDirection"))
	int32 Directions = GSTN_DirectionMasks::All;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Direction")
	EGSTN_StickZone MinZone = EGSTN_StickZone::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Direction")
	EGSTN_StickZone MaxZone = EGSTN_StickZone::Tilt;

	/** The stick must have just flicked (smash-style inputs) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Direction")
	bool bRequireFlick = false;

	virtual bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const override;
	virtual bool IsDirectionCondition() const override { return true; }
	virtual int32 GetSpecificity() const override { return 5 + (MinZone == EGSTN_StickZone::Tilt ? 3 : 0) + (bRequireFlick ? 2 : 0); }
	virtual void AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const override;
};

/**
 * A sequence of direction sets entered in order (quarter circle, back then forward...). Other stick events in between
 * are allowed. The anchor is the frame the last element was entered. Anchor. Specificity 5 per element.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Motion"))
struct GSTNCOMBATCORE_API FGSTN_InputCondition_Motion : public FGSTN_InputCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	EGSTN_DirectionSpace Space = EGSTN_DirectionSpace::Facing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	TArray<FGSTN_DirectionSet> Sequence;

	/** Largest distance between two consecutive elements, in frames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0"))
	int32 MaxFramesBetween = 12;

	/** Largest distance between the first and the last element, in frames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0"))
	int32 MaxTotalFrames = 30;

	/** Zone every non-neutral element needs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	EGSTN_StickZone MinZone = EGSTN_StickZone::Tilt;

	virtual bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const override;
	virtual bool IsAnchorCondition() const override { return true; }
	virtual bool IsDirectionCondition() const override { return true; }
	virtual int32 GetSpecificity() const override { return 5 * Sequence.Num(); }
	virtual void AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const override;
};

/**
 * A rotation of the stick through consecutive 45 degree sectors, starting anywhere (a 360 spin). Reading the stick events
 * backwards from the anchor: each step moves 1 to 1 + MaxSkippedSectors sectors in the same sense, every sector is at
 * least MinZone, the total reaches MinDegrees and the whole thing fits in MaxFrames. Anchor = the frame of the last
 * sector. Anchor. Specificity 5 per 45 degrees.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Spin"))
struct GSTNCOMBATCORE_API FGSTN_InputCondition_Spin : public FGSTN_InputCondition
{
	GENERATED_BODY()

	/** The physical stick by default, so the spin does not depend on where the character faces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spin")
	EGSTN_DirectionSpace Space = EGSTN_DirectionSpace::Stick;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spin", meta = (ClampMin = "45", ClampMax = "720", ForceUnits = "deg"))
	float MinDegrees = 360.f;

	/** Time allowed for the whole spin, in frames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spin", meta = (ClampMin = "1"))
	int32 MaxFrames = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spin")
	EGSTN_SpinRotation Rotation = EGSTN_SpinRotation::Either;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spin")
	EGSTN_StickZone MinZone = EGSTN_StickZone::Tilt;

	/** Sectors a fast stick may skip between two stick events */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spin", meta = (ClampMin = "0", ClampMax = "3"))
	int32 MaxSkippedSectors = 1;

	virtual bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const override;
	virtual bool IsAnchorCondition() const override { return true; }
	virtual bool IsDirectionCondition() const override { return true; }
	virtual int32 GetSpecificity() const override { return 5 * FMath::CeilToInt(MinDegrees / 45.f); }
	virtual void AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const override;
};

/**
 * The stick kept in a direction set for a while and left shortly before the anchor (back-charge moves). The direction
 * may also still be held at the anchor. Not an anchor. Specificity 15.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Charge"))
struct GSTNCOMBATCORE_API FGSTN_InputCondition_Charge : public FGSTN_InputCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Charge")
	EGSTN_DirectionSpace Space = EGSTN_DirectionSpace::Facing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Charge", meta = (Bitmask, BitmaskEnum = "/Script/GSTNCombatCore.EGSTN_StickDirection"))
	int32 Directions = 0;

	/** Frames the stick must have stayed in the set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Charge", meta = (ClampMin = "1"))
	int32 MinFrames = 30;

	/** The stick may have left the set at most this many frames before the anchor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Charge", meta = (ClampMin = "0"))
	int32 MaxReleaseFrames = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Charge")
	EGSTN_StickZone MinZone = EGSTN_StickZone::Tilt;

	virtual bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const override;
	virtual bool IsDirectionCondition() const override { return true; }
	virtual int32 GetSpecificity() const override { return 15; }
	virtual void AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const override;
};

// ---------------------------------------------------------------------------------------------------------------------
// Composition
// ---------------------------------------------------------------------------------------------------------------------

/**
 * Any of several conditions of the same kind. Used as an anchor it reports the newest candidate among its children (the
 * first one on a tie), so that enumerating candidates stays in newest-first order. Role and direction-ness follow the
 * first child. Specificity is the highest of the children.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Any Of"))
struct GSTNCOMBATCORE_API FGSTN_InputCondition_AnyOf : public FGSTN_InputCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Any Of")
	TArray<TInstancedStruct<FGSTN_InputCondition>> Conditions;

	virtual bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const override;
	virtual bool IsAnchorCondition() const override;
	virtual bool IsDirectionCondition() const override;
	virtual int32 GetSpecificity() const override;
	virtual void AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Map, int32& Frame) const override;
	virtual void GetReferencedButtons(FGameplayTagContainer& OutButtons) const override;
};
