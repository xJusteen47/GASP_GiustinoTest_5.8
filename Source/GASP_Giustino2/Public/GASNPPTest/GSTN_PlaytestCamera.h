// Playtest camera distance (owner 2026-11-17: "camera much farther", 2026-11-18: "a bit closer") and tilt-shift look (owner 2026-11-18). The
// play view is GASP's CameraAsset_SandboxCharacter (GameplayCameras); its rigs are GASP content, kept untouched so a GASP update still applies.
// Both are a camera modifier on top of whatever the view target computes, tuned as data in the camera settings (FGSTN_CameraAnchorSettings
// Framing and TiltShift, DA_CameraAnchor_Default); console variables override them for A/B (-1 = the data).

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_PlaytestCamera.generated.h"

class UGSTN_CameraAnchorSettingsAsset;
class UMaterialInstanceDynamic;
struct FGSTN_CameraTiltShiftSettings;

/**
 * Pushes the final view back along its own forward by the camera settings ExtraDistance and up by ExtraHeight (console overrides), sweeping
 * from the unmodified view so a wall stops it short, then adds the tilt-shift look. Presentation only: never read by the simulation
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_PlaytestCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	/** The pushed back location from the view, its forward and the extra distance and height (no collision). Pure, for the tests */
	static FVector PushBack(const FVector& ViewLocation, const FVector& ViewForward, float ExtraDistance, float ExtraHeight);

	/** Where a world point lands on screen vertically, as a UV (0 top, 1 bottom, may leave 0..1). Pure, for the tests and the screen band */
	static float ScreenV(const FMinimalViewInfo& View, const FVector& WorldPoint);

	/**
	 * Telephoto framing: the camera distance scale that keeps a point the same size on screen when the horizontal FOV goes from FromFOV to
	 * ToFOV (tan(From/2) / tan(To/2)), and the inverse (the FOV that keeps the size at a given distance scale). Pure, for the tests
	 */
	static float TelephotoDistanceScale(float FromFOV, float ToFOV);
	static float FOVForDistanceScale(float FromFOV, float DistanceScale);

	/** The f-stop the DOF tilt shift uses: Fstop / Intensity, never under 0.7. Pure, for the tests */
	static float TiltShiftFstop(const FGSTN_CameraTiltShiftSettings& TiltShift);

protected:
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

private:
	void ApplyTiltShift(const FGSTN_CameraTiltShiftSettings& TiltShift, const FMinimalViewInfo& POV, const FVector& Focus);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BandMaterial;
	bool bBandMaterialTried = false;
};

/**
 * Adds the playtest camera modifier to every local player's camera manager in a game world, once (camera managers come and go with players),
 * and a UGSTN_CameraAnchorComponent to every locally viewed pawn that has none (GSTN_CameraAnchor.h: the camera stops shaking with the capsule)
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_PlaytestCameraSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UGSTN_CameraAnchorSettingsAsset> AnchorSettings;
	bool bAnchorSettingsLoaded = false;
};
