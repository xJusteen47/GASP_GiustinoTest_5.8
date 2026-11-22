// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_CoveredLanding.generated.h"

/**
 * Covered landing (2026-11-17, dair V10 light landing r3): a fighter that touches the floor while a montage drives the pose (an aerial's
 * landing lag, a dropkick's land clip) still makes the base locomotion pick its own landing reaction (GASP: the land crouch). The montage
 * covers it while it plays; when the montage blends out, the crouch shows for 12+ frames (pelvis 84 -> 64). The fix keeps the montage's last
 * full pose over the base layer until that landing reaction is over, then blends to the live pose. Settings, on UGSTN_CharacterAnimInstance
 */
USTRUCT(BlueprintType)
struct GSTNANIMATIONCORE_API FGSTN_CoveredLandingSettings
{
	GENERATED_BODY()

	/** Off: the base layer's landing shows after the montage as before */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Covered Landing")
	bool bEnabled = true;

	/** A fall at least this fast (cm/s, along the actor's up) counts as airborne for the touchdown */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Covered Landing", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float FallSpeed = 150.f;

	/** The touchdown is the first frame after a fall whose downward speed is under this (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Covered Landing", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float StoppedSpeed = 30.f;

	/** How long after the touchdown the base layer's landing reaction lasts: the held pose stays at full weight until then (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Covered Landing", meta = (ClampMin = "0", ForceUnits = "s"))
	float HoldUntil = 0.42f;

	/** Then the live pose comes back over this long (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Covered Landing", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float BlendOut = 0.15f;

	/** Moving along the floor faster than this (cm/s) lets the live pose back at once (walking away from the landing) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Covered Landing", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MoveSpeed = 60.f;
};

/** What the covered landing asks of the node this frame */
struct FGSTN_CoveredLandingOutput
{
	/** Copy the evaluated pose as the one to hold (the montage drives it at full weight, after a covered touchdown) */
	bool bCapture = false;
	/** Weight of the held pose over the live one (0 = live) */
	float HoldAlpha = 0.f;
};

/** The covered landing's rules, pure (game thread, the anim instance's update; tests) */
struct GSTNANIMATIONCORE_API FGSTN_CoveredLandingLogic
{
	/**
	 * One frame. UpSpeed / FloorSpeed: the actor's speed along its up and across it (cm/s, from its own motion: this module knows no movement
	 * component); bMontageDriving: a montage plays or blends in (not stopping); MontageWeight: the largest montage weight
	 */
	FGSTN_CoveredLandingOutput Step(const FGSTN_CoveredLandingSettings& Settings, float DeltaSeconds, float UpSpeed, float FloorSpeed,
		bool bMontageDriving, float MontageWeight);

	void Reset() { *this = FGSTN_CoveredLandingLogic(); }

	/** Seconds since the covered touchdown, -1 when no window is open (for captures and tests) */
	float GetWindowTime() const { return WindowTime; }

private:
	bool bFalling = false;
	float WindowTime = -1.f;
	/** The montage that covered the touchdown stopped driving: from here a new montage ends the window */
	bool bCoverStopped = false;
	/** A full-weight pose was captured in this window */
	bool bCaptured = false;
	/** Fades the hold out early (moved, left the ground) */
	float EarlyFade = -1.f;
};
