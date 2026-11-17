// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_BodyCapsule.generated.h"

/** What stays in place when the movement capsule changes to a Body Capsule track */
UENUM(BlueprintType)
enum class EGSTN_BodyCapsuleAnchor : uint8
{
	/** The character stays where it is on screen: the capsule (the actor) moves onto the body, the mesh is offset back by the same amount */
	Body,
	/**
	 * The capsule stays (its bottom keeps its height), the mesh moves under it. For a part whose clip already moved the body relative to its
	 * root, e.g. the dropkick's land clip, whose pelvis starts ~half a metre under the loop's: the mesh root goes back to the ground, the clip's
	 * own drop is cancelled instead of shown
	 */
	Capsule
};

/**
 * Body Capsule (owner 2026-11-15, dropkick: "the capsule should be at the centre of the torso, it looks like it is on the head"). While the timeline
 * evaluates a frame of [StartFrame, EndFrame), the character's movement capsule (Mover's collision: what lands, what is blocked by walls, what the
 * depth marker reads) takes this shape and sits on this point of the body instead of the standing capsule's. Outside every track the capsule is the
 * pawn's own (its archetype). Per move data, the general per-move capsule mechanism (ECB-like shortening: a crouching attack, a lying knockdown, an
 * air move with the legs tucked): several tracks per timeline, the first track holding the frame wins.
 *
 * Hurtboxes and hitboxes do not use it (they follow the bones). Simulation: FGSTN_BodyCapsuleModifier (GSTNCombatNP) applies it from Mover's tick,
 * rollback-safe (the applied shape is Mover sync state). A held frame (Hold Until Landed, hitstop) keeps its track's shape.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Body Capsule"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_BodyCapsule : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** Capsule half height (centre to the tip of a cap). Unreal raises it to the radius when smaller */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Capsule", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float HalfHeight = 40.f;

	/** Capsule radius, 0 = the pawn's own */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Capsule", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float Radius = 0.f;

	/**
	 * Where the capsule centre sits on the character, from the mesh root (the point between the feet of the clip), in the capsule's frame:
	 * X forward, Y right, Z up. The standing capsule is (0, 0, its half height). Usually the pelvis/torso of the clip's pose on these frames
	 * (Tools/GAS/setup_dropkick_body_capsule.py measures it from the clips)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Capsule", meta = (ForceUnits = "cm"))
	FVector Offset = FVector(0.0, 0.0, 40.0);

	/** What stays in place when the capsule changes to this track */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Capsule")
	EGSTN_BodyCapsuleAnchor Anchor = EGSTN_BodyCapsuleAnchor::Body;

	/**
	 * Body anchor only: the most the capsule centre moves per frame on its way to this track (0 = in one frame). The mesh is offset back by the
	 * same amount, so the body never moves on screen either way; a few frames only keep the step small under render interpolation
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Capsule", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxShiftPerFrame = 20.f;
};

/** A movement capsule: size and where its centre is on the character (see FGSTN_TimelineTrack_BodyCapsule::Offset) */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_BodyCapsuleShape
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Body Capsule")
	float HalfHeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Body Capsule")
	float Radius = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Body Capsule")
	FVector Offset = FVector::ZeroVector;

	/** The pawn's own capsule */
	static FGSTN_BodyCapsuleShape Standing(float InHalfHeight, float InRadius)
	{
		FGSTN_BodyCapsuleShape Shape;
		Shape.HalfHeight = InHalfHeight;
		Shape.Radius = InRadius;
		Shape.Offset = FVector(0.0, 0.0, InHalfHeight);
		return Shape;
	}

	/** Equal within a hundredth of a centimetre (shapes cross the wire quantized) */
	bool NearlyEquals(const FGSTN_BodyCapsuleShape& Other) const;
};

/** What the capsule asks for on one frame */
struct FGSTN_BodyCapsuleTarget
{
	FGSTN_BodyCapsuleShape Shape;
	EGSTN_BodyCapsuleAnchor Anchor = EGSTN_BodyCapsuleAnchor::Body;
	/** 0 = no limit */
	float MaxShiftPerFrame = 0.f;
};

/** One frame's change of the capsule: the new applied shape and how far the capsule centre (the actor) moves, in the capsule's frame */
struct FGSTN_BodyCapsuleStep
{
	FGSTN_BodyCapsuleShape Applied;
	FVector ActorShiftLocal = FVector::ZeroVector;
};

/** The rules of the Body Capsule track, pure functions for the movement modifier and the tests */
struct GSTNCOMBATCORE_API FGSTN_BodyCapsule
{
	/** The first Body Capsule track of Data holding Frame, null when none does (also for INDEX_NONE) */
	static const FGSTN_TimelineTrack_BodyCapsule* FindTrackAt(const FGSTN_CombatTimelineData& Data, int32 Frame);

	/**
	 * The capsule of the frame: the track's when Data has one holding TimelineFrame (Radius 0 = Standing's), else Standing with the Body anchor
	 * (the way back to the pawn's capsule keeps the body still, as crouch does) and no step limit. Data null = Standing
	 */
	static FGSTN_BodyCapsuleTarget ResolveTarget(const FGSTN_CombatTimelineData* Data, int32 TimelineFrame, const FGSTN_BodyCapsuleShape& Standing);

	/**
	 * As above, with the capsule of the state outside every track (the ground crouch, FGSTN_GroundCrouchKitExtension): Idle, when not null, takes
	 * the place of Standing as the shape to go to when no track holds the frame, with the Body anchor (the mesh stays put, the bottom stays on the
	 * feet) and no step limit. A track holding the frame still wins
	 */
	static FGSTN_BodyCapsuleTarget ResolveTarget(const FGSTN_CombatTimelineData* Data, int32 TimelineFrame, const FGSTN_BodyCapsuleShape& Standing, const FGSTN_BodyCapsuleShape* Idle);

	/**
	 * One frame's step from Applied toward Target. Body anchor: the offset moves at most MaxShiftPerFrame (size changes at once), the actor moves
	 * by the offset change, so the mesh root (actor - offset) stays put. Capsule anchor: at once; the actor moves only by the half height change,
	 * so the capsule's bottom stays (a grounded capsule stays on the ground), the mesh moves with the new offset
	 */
	static FGSTN_BodyCapsuleStep Step(const FGSTN_BodyCapsuleShape& Applied, const FGSTN_BodyCapsuleTarget& Target);

	/**
	 * The standing capsule coming back while the pawn stands on the floor grows FROM THE FLOOR (Capsule anchor: bottom kept, the mesh root goes
	 * under the actor's standing offset). With the Body anchor the mesh root stayed where the move's last track had it, below an aerial's tucked
	 * feet, so after a landing the pawn stood sunk by that gap for good (dair V9 27 cm, V10 52 cm under the floor; dair-v10 review r1, 2026-11-17).
	 * In the air the Body anchor stays (the body does not move on screen). A target that is not the standing shape is returned as it is
	 */
	static FGSTN_BodyCapsuleTarget GroundedRestore(const FGSTN_BodyCapsuleTarget& Target, const FGSTN_BodyCapsuleShape& Standing, bool bGrounded);
};
