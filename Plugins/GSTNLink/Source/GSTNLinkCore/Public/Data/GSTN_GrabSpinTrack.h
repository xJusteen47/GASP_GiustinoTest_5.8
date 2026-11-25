// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_GrabSpinTrack.generated.h"


/** Which way a grab spin turns, seen from above (the stick's rotation sense, decided by the slot: one command per sense) */
UENUM(BlueprintType)
enum class EGSTN_GrabSpinSense : uint8
{
	/** Yaw grows: clockwise seen from above, UE's left-handed Z-up frame */
	Clockwise,
	CounterClockwise
};

/**
 * The grab spin (owner 2026-11-25: "se si spinna lo stick con un nemico in mano lo si puo' usare per girare in tondo, far male ai nemici vicini e a quel
 * nemico, e poi lanciarlo"). A track of the grabber's spin move: over [StartFrame, EndFrame) the grabber's facing turns Turns full turns in Sense,
 * linearly, from the yaw the move started with (the move origin the combat sim stamps on the timeline's first frame).
 *
 * Why the grabber turns and not the partner: the held partner's parked placement is in the grabber's root space (UGSTN_LinkMath::NeckHoldAnchor), so
 * turning the grabber swings the partner round with no new placement code, and a Root-space hitbox in front of the grabber (where the partner hangs)
 * sweeps the circle as the partner does. The yaw is a pure function of the origin yaw and the timeline frame (never accumulated), so a dropped or
 * re-simulated frame cannot drift it; the link tick extension writes it as the combat core's SetFacing request every frame of the track.
 *
 * A link track kind found by its struct, like FGSTN_LinkOutcomeTrack, so the combat core's track kind enum stays untouched.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Grab Spin"))
struct GSTNLINKCORE_API FGSTN_GrabSpinTrack : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab Spin")
	EGSTN_GrabSpinSense Sense = EGSTN_GrabSpinSense::Clockwise;

	/** Full turns over the track's frames */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab Spin", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float Turns = 1.f;

	/** The first grab spin track of a timeline, or null */
	static const FGSTN_GrabSpinTrack* Find(const FGSTN_CombatTimelineData& Data);

	/**
	 * The facing at TimelineFrame (frames since the move started, pauses excluded), degrees, for a move that started facing OriginYawDeg. Before the
	 * track: the origin; after it: the origin plus the whole turn. False when the frame is outside [StartFrame, EndFrame] (nothing to write)
	 */
	bool YawAt(float OriginYawDeg, int32 TimelineFrame, float& OutYawDeg) const;

	/** One message per problem: an empty or inverted range, no turn */
	void Validate(const FString& Label, TArray<FString>& OutErrors) const;
};
