// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"

/** One finding of the timeline validation */
struct GSTNCOMBATCORE_API FGSTN_TimelineMessage
{
	bool bError = false;
	FString Text;
};

/** What the validation may know besides the data itself. Every field is optional: what is missing is not checked. */
struct FGSTN_TimelineValidationContext
{
	/** Length in seconds of the montage at play rate 1, only when it is loaded or the caller has it; negative = unknown */
	float MontageLengthSeconds = -1.f;

	/** Length of the montage section when the timeline names one; negative = unknown (then the whole montage is compared) */
	float MontageSectionSeconds = -1.f;

	/** Frames per second of the fixed simulation tick, to turn the montage length into frames */
	float TickRate = 60.f;

	/** Override frames to assume for a timeline that says "default of its command tier" (-1) */
	int32 DefaultOverrideFrames = 0;

	/** The part and group names of the character; null = the ids are not checked (M4 defines the part vocabulary) */
	const TSet<FName>* KnownPartNames = nullptr;
	const TSet<FName>* KnownGroupNames = nullptr;
};

/**
 * Pure checks of a timeline's data (combat-m3 6.3 and 7.2). Errors make the asset invalid; warnings are for the designer.
 * Nothing here loads an asset: the montage length is only compared when the caller passes it (MakeContext fills it from a
 * montage that is already loaded).
 */
struct GSTNCOMBATCORE_API FGSTN_TimelineValidation
{
	/** Context with the montage length taken from the soft pointer when the montage is already in memory */
	static FGSTN_TimelineValidationContext MakeContext(const FGSTN_CombatTimelineData& Data, float TickRate, int32 DefaultOverrideFrames = 0);

	/**
	 * Checks:
	 *  - FirstActionableFrame <= TotalFrames; the tail start lies inside the timeline (a start past the end is clamped and
	 *    reported; a tail with a start before FirstActionableFrame is reported too: it would cancel a locked part)
	 *  - deferred commit: nothing that costs or acts (event, movement, weapon switch, hitbox) starts before the end of the
	 *    override window (OverrideFrames, or the tier default), and the window ends inside the timeline
	 *  - tracks have a positive range inside the timeline; hitbox and hurtbox tracks address a part, group or tag, and
	 *    the ids exist when the context lists them
	 *  - the montage length (at the play rate) matches TotalFrames within one frame, when it is known
	 *  - the ricochet response, when authored, has a positive length and its movement stays inside it
	 */
	static void Validate(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineValidationContext& Context, TArray<FGSTN_TimelineMessage>& OutMessages);
};
