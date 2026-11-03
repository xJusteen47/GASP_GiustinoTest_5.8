// Defence, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_DefenceRules.h"
#include "GSTN_DefenceViews.generated.h"

/**
 * Plain views the defence math reads. They are built by the network module from the sync state and the snapshot (and by the tests
 * by hand), so the math never reads a world, an actor or a component. Values are doubles and integers: every machine that builds
 * the same view gets the same bits out of the same function. No view holds a tag; the caller resolves tags to flags first.
 *
 * A frame field that is INDEX_NONE means "never happened".
 */

/** Per-frame time of the fixed tick (62.5 Hz) for the functions that integrate a speed */
namespace GSTNDefenceTiming
{
	constexpr double FrameSeconds = 0.016;
}

/** The shield's three rates, read from the attribute set at the end of F-1 (or from the rules at load) */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_ShieldRates
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	double Drain = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	double Regen = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	double Max = 50.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	int32 RegenDelayFrames = 0;

	static FGSTN_ShieldRates FromRules(const FGSTN_ShieldRules& Rules);
};

/**
 * The stamped state the shield value is derived from, analytically: a value at a frame plus whether the shield is held. The value
 * is never stored per frame (m6 spec 5.3); it is re-stamped at each event (press, release, damage, rate change).
 */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_ShieldView
{
	GENERATED_BODY()

	/** The value at StampFrame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	double Value = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	int32 StampFrame = 0;

	/** The shield button is held and the bubble is raised or raising: the value drains */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	bool bHeld = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	int32 PressFrame = INDEX_NONE;

	/** The frame the shield was released; INDEX_NONE while held or never raised */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	int32 ReleaseFrame = INDEX_NONE;
};

/** The sphere the snapshot extension contributes: world centre and radius. The radius is the rules' RadiusCm at every shield value */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_ShieldBubble
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	FVector Centre = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	double Radius = 0.0;
};

/** Which parry source produced a decision */
UENUM(BlueprintType)
enum class EGSTN_ParrySource : uint8
{
	None,
	/** The shield's perfect shield */
	Shield,
	/** A move's parry window track */
	Move
};

/** The one parry decision both sources go through (m6 spec 5.4) */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_ParryDecision
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry")
	EGSTN_ParrySource Source = EGSTN_ParrySource::None;

	/** Index into UGSTN_DefenceRules::ParryProfiles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry")
	int32 ProfileIndex = 0;

	bool IsParry() const { return Source != EGSTN_ParrySource::None; }
};

/**
 * Everything ResolveParry needs about one hit at the hit's stamped frame, taken from the victim's snapshot and the hit data. Flags,
 * not tags: the caller resolves Combat.Immunity.Parry and the hit type once (see UGSTN_DefenceMath::HasParryImmunity and
 * IsGrabLikeHitType).
 */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_ParryContext
{
	GENERATED_BODY()

	/** The hit's stamped frame: the decision never depends on the frame the hit is resolved at */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	int32 HitFrame = 0;

	/** FGSTN_HitData::bParryable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bHitParryable = true;

	/** The hit or its ability carries Combat.Immunity.Parry */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bHitImmune = false;

	/** The victim carries Combat.Immunity.Parry */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bVictimImmune = false;

	/** The hit type is Grab, Throw or Pummel: the link step runs first and the parry skips them */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bGrabLikeHit = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bProjectile = false;

	/** The hit landed on the shield bubble (the shield source needs it) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bHitOnBubble = false;

	// ---- Source Shield ----

	/** The shield is available (mode and kit) and the victim has a bubble at the hit frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	bool bShieldAvailable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	int32 ShieldPressFrame = INDEX_NONE;

	/** INDEX_NONE while the shield is still held */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield")
	int32 ShieldReleaseFrame = INDEX_NONE;

	// ---- Source Move ----

	/** Stamped when the parry window track starts; INDEX_NONE = no window */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	int32 MoveStartFrame = INDEX_NONE;

	/** Exclusive */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	int32 MoveEndFrame = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	int32 MoveProfileIndex = 0;

	/** The hit's hurtbox is one the window covers (FGSTN_ParryWindowTrack::Affects, resolved by the caller) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	bool bMoveCoversHurtbox = false;
};

/** The tech state machine's stamps (m6 spec 6). Frames only, so a re-simulation from the input history reproduces them */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_TechState
{
	GENERATED_BODY()

	/** The last press that counted (not locked out); INDEX_NONE = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
	int32 LastTechPressFrame = INDEX_NONE;

	/** Presses before this frame are ignored (anti-mash) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
	int32 TechLockoutEndFrame = 0;

	/** The newest contact already evaluated */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
	int32 LastHandledContact = INDEX_NONE;
};

/** The result of EvaluateTech: the new stamps and, when a contact was evaluated, whether it was teched */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_TechEvaluation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
	FGSTN_TechState State;

	/** A new contact was evaluated by this call */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
	bool bContactEvaluated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
	bool bSuccess = false;

	/** The press that made the tech; INDEX_NONE on failure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
	int32 PressFrame = INDEX_NONE;
};

/** One frame of the SDI drift */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_SDIStep
{
	GENERATED_BODY()

	/** The drift velocity for this frame, cm/s on the plane; zero when nothing drifts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDI")
	FVector DriftVelocity = FVector::ZeroVector;

	/** The distance this frame adds to the hit's total, cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDI")
	double DistanceAdded = 0.0;

	/** SDI is not allowed for this victim or hit (armor, throw, stagger in place), as opposed to a stick that does not move it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDI")
	bool bBlocked = false;
};
