// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitData.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_LinkOutcomeTrack.generated.h"

/** What the linking role's move does with the link at one of its outcome keys (m6 spec 3 and 8) */
UENUM(BlueprintType)
enum class EGSTN_LinkOutcome : uint8
{
	/** The partner is released where it stands, placed by the link type's exit placer */
	Release,
	/** The partner takes the key's throw hit and leaves the link; the key's exit placement names the placer */
	Throw,
	/** The move continues and the link stays (a pummel, a pose change) */
	KeepLink,
	/** The two entities swap roles (a reversal) */
	SwapRoles
};

/** One outcome of a link move, at a frame of the move's timeline */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkOutcomeKey
{
	GENERATED_BODY()

	/** The frame counted from the track's StartFrame at which the outcome applies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 Frame = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	EGSTN_LinkOutcome Outcome = EGSTN_LinkOutcome::Release;

	/** The hit the partner takes with the outcome Throw (damage, knockback, hit type Throw). Ignored by the other outcomes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome", meta = (EditCondition = "Outcome == EGSTN_LinkOutcome::Throw"))
	FGSTN_HitData ThrowHit;

	/** The exit placer to use, by name, instead of the link type's own. None = the link type's placer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
	FName ExitPlacement = NAME_None;
};

/**
 * The grabber's throw / release moment (m6 spec 3, timeline tracks): a list of outcome keys on a move's timeline. It is a link track
 * kind found by its struct, like the defence tracks, so the combat core's closed track kind enum stays untouched and GSTNCombat knows
 * nothing about links.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Link Outcome"))
struct GSTNLINKCORE_API FGSTN_LinkOutcomeTrack : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link")
	TArray<FGSTN_LinkOutcomeKey> Keys;

	/** The first key at this frame (counted from StartFrame), or null */
	const FGSTN_LinkOutcomeKey* FindKeyAt(int32 FrameFromStart) const;

	/** Appends one message per problem: an empty or inverted frame range, no key, keys out of order or outside the track, a Throw key with a hit that is not a throw */
	void Validate(const FString& Label, TArray<FString>& OutErrors) const;
};
