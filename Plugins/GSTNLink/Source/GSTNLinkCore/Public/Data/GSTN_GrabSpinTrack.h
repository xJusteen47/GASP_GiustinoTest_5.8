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
 * Owner answers 2026-11-25 (3, 4): one stick rotation makes TWO turns, then a settle that is only for the eye (the turns end on TurnEndFrame, the
 * move is actionable from there and its cancellable tail carries the settle: the facing overshoots by SettleOvershootDeg in the spin's sense and comes
 * back, so the capsule shows the momentum before an animation exists). Slamming the held partner into a wall hurts it with some hitstop: the
 * track names the hit group of that hitbox (WallHitGroup) and the link tick masks the group on every frame the probe from the grabber to the
 * hold point does not touch static world geometry (FGSTN_LinkTickExtension::StepGrabSpinWall).
 *
 * A link track kind found by its struct, like FGSTN_LinkOutcomeTrack, so the combat core's track kind enum stays untouched.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Grab Spin"))
struct GSTNLINKCORE_API FGSTN_GrabSpinTrack : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab Spin")
	EGSTN_GrabSpinSense Sense = EGSTN_GrabSpinSense::Clockwise;

	/** Full turns over [StartFrame, TurnEndFrame] */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab Spin", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float Turns = 1.f;

	/** Timeline frame the turns end on; the frames after it, up to EndFrame, are the settle. INDEX_NONE (or anything outside (StartFrame, EndFrame]) = EndFrame, no settle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab Spin", meta = (ForceUnits = "Frames"))
	int32 TurnEndFrame = INDEX_NONE;

	/** Settle (presentation of momentum, owner 2026-11-25 "solo estetico"): the facing goes this far past the last turn and back, half a sine over the settle frames */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab Spin", meta = (ClampMin = "0.0", ClampMax = "90.0", ForceUnits = "deg"))
	float SettleOvershootDeg = 0.f;

	/** The hit group of the wall slam hitbox (held partner only). The link tick masks it unless the wall probe touches; 255 = no wall slam */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab Spin|Wall")
	uint8 WallHitGroup = 255;

	/** The wall probe: a sphere swept from the grabber's move origin this far along the frame's facing (the hold point plus the partner's half body) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab Spin|Wall", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float WallProbeReach = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab Spin|Wall", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float WallProbeRadius = 30.f;

	/** The first grab spin track of a timeline, or null */
	static const FGSTN_GrabSpinTrack* Find(const FGSTN_CombatTimelineData& Data);

	/**
	 * The facing at TimelineFrame (frames since the move started, pauses excluded), degrees, for a move that started facing OriginYawDeg. Before the
	 * track: the origin; after it: the origin plus the whole turn. False when the frame is outside [StartFrame, EndFrame] (nothing to write)
	 */
	bool YawAt(float OriginYawDeg, int32 TimelineFrame, float& OutYawDeg) const;

	/** TurnEndFrame resolved: EndFrame when unset or out of (StartFrame, EndFrame] */
	int32 ResolveTurnEndFrame() const;

	/** The wall slam can land at TimelineFrame: a wall hit group is set and the frame is in the turns (never in the settle) */
	bool IsWallProbeFrame(int32 TimelineFrame) const;

	/** One message per problem: an empty or inverted range, no turn, a wall group past the 8 that hit once per group */
	void Validate(const FString& Label, TArray<FString>& OutErrors) const;
};
