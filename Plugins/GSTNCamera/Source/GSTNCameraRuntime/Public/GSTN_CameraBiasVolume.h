// Soft framing volume (owner decision 2026-11-21): the camera is free by default; a designer can steer it towards a direction or a part of
// the level, or keep it inside a range, WITHOUT a hard lock. It blends in and out smoothly, the player's stick always wins, and the pull
// comes back only after the stick has rested. Replaces the "Parallel" mode option of the research plan. Hard locks stay in AGSTN_CameraSection.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_CameraTypes.h"
#include "GameFramework/Volume.h"
#include "GSTN_CameraBiasVolume.generated.h"

class USceneComponent;

UENUM(BlueprintType)
enum class EGSTN_CameraBiasYawSource : uint8
{
	/** The volume's forward arrow plus YawOffsetDeg (a corridor, a "look this way" stretch) */
	VolumeForward,
	/** From the player towards the focus point or focus actor (a door, a boss, a landmark) */
	TowardFocus
};

USTRUCT(BlueprintType)
struct GSTNCAMERARUNTIME_API FGSTN_CameraBiasSpec
{
	GENERATED_BODY()

	/** Turn it on/off at runtime with SetBiasEnabled (it blends, never pops) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing")
	bool bEnabled = true;

	/** Higher priority volumes are served first; each takes its weight of what is left */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing")
	int32 Priority = 0;

	/** Overall strength 0..1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ClampMin = "0", ClampMax = "1"))
	float Weight = 1.f;

	/** Inside the volume the weight rises from 0 at the edge to full at this depth (smooth), so walking in never pops */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ClampMin = "0", ForceUnits = "cm"))
	float FalloffCm = 250.f;

	// ---- What it does ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Direction")
	bool bBiasYaw = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Direction", meta = (EditCondition = "bBiasYaw"))
	EGSTN_CameraBiasYawSource YawSource = EGSTN_CameraBiasYawSource::VolumeForward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Direction", meta = (EditCondition = "bBiasYaw", ForceUnits = "deg"))
	float YawOffsetDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Direction")
	bool bBiasPitch = false;

	/** Negative looks down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Direction", meta = (EditCondition = "bBiasPitch", ClampMin = "-89", ClampMax = "89", ForceUnits = "deg"))
	float PitchDeg = -25.f;

	/** Arm multiplier inside the volume (1 = the player's distance unchanged) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|View", meta = (ClampMin = "0.1"))
	float DistanceScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|View", meta = (ForceUnits = "deg"))
	float FovOffsetDeg = 0.f;

	/** Move the pivot this fraction of the way to the focus, so player and focus share the frame (0 = none) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|View", meta = (ClampMin = "0", ClampMax = "1"))
	float FramePivotFraction = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|View", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxPivotShiftCm = 300.f;

	/** Optional: an actor to focus (moving target: smoothed by FocusDampingSeconds). Null = the Focus component of this volume */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Focus")
	TSoftObjectPtr<AActor> FocusActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Focus", meta = (ClampMin = "0", ForceUnits = "s"))
	float FocusDampingSeconds = 0.3f;

	// ---- The player ----

	/** Soft: stick free, the pull pauses on input. Firm: stick free within FirmRangeDeg (wider at the edge), the pull returns soon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Player")
	EGSTN_CameraBiasStrength Strength = EGSTN_CameraBiasStrength::Soft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Player", meta = (EditCondition = "Strength == EGSTN_CameraBiasStrength::Firm", ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float FirmRangeDeg = 35.f;

	/** After look input the pull waits this long (Soft 1.5 s, Firm 0.4 s are good starts) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Player", meta = (ClampMin = "0", ForceUnits = "s"))
	float ResumeDelaySeconds = 1.5f;

	/** Time constant of the pull */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Player", meta = (ClampMin = "0", ForceUnits = "s"))
	float PullSeconds = 0.6f;

	/** Never turns faster than this (anti-nausea, the Bananza lesson) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Player", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float MaxPullDegPerSecond = 45.f;

	/** Never steer during a jump (design.md §3.4, Gemini review) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Player")
	bool bSuppressInAir = true;

	/** Only pull while the character moves */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Player")
	bool bOnlyWhileMoving = false;

	/** A framing heading further than this from the current camera heading does not pull (a focus behind the player never spins the view). 180 = always */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Player", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxEngageAngleDeg = 100.f;

	// ---- Timing ----

	/** Enable/disable blends (walking in and out is spatial: FalloffCm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Timing", meta = (ClampMin = "0", ForceUnits = "s"))
	float BlendInSeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing|Timing", meta = (ClampMin = "0", ForceUnits = "s"))
	float BlendOutSeconds = 0.8f;
};

/**
 * A soft framing volume. Local presentation only (never replicated, nothing on a dedicated server); every local camera evaluates it on
 * its own presented pivot. Outside AGSTN_CameraSection volumes only: a section is the designer's hard framing and wins.
 */
UCLASS(Blueprintable, meta = (DisplayName = "GSTN Camera Framing Volume"))
class GSTNCAMERARUNTIME_API AGSTN_CameraBiasVolume : public AVolume
{
	GENERATED_BODY()

public:
	AGSTN_CameraBiasVolume();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Framing", meta = (ShowOnlyInnerProperties))
	FGSTN_CameraBiasSpec Spec;

	/** Half extents of the box used when the volume has no brush (made in code or a test) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Framing", meta = (ClampMin = "0", ForceUnits = "cm"))
	FVector FallbackExtent = FVector(500.0, 500.0, 300.0);

	UFUNCTION(BlueprintCallable, Category = "Camera|Framing")
	void SetBiasEnabled(bool bEnabled) { Spec.bEnabled = bEnabled; }

	/** Spatial weight 0..1 of a point: 0 outside, smooth up to full FalloffCm inside (the brush bounds box gives the depth) */
	UFUNCTION(BlueprintPure, Category = "Camera|Framing")
	float GetSpatialWeight(const FVector& Point) const;

	/** The focus in world space: the focus actor, else the Focus component */
	UFUNCTION(BlueprintPure, Category = "Camera|Framing")
	FVector GetFocusLocation() const;

	/** Heading of the actor forward about Up, in the frame */
	float GetBaseHeading(const FGSTN_CameraFrame& Frame) const;

	USceneComponent* GetFocusComponent() const { return Focus; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Where TowardFocus and FramePivotFraction aim when no focus actor is set; move it in the viewport */
	UPROPERTY(VisibleAnywhere, Category = "Camera|Framing")
	TObjectPtr<USceneComponent> Focus;
};
