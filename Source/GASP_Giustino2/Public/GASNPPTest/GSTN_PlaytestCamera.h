// Playtest camera distance (owner 2026-11-17: "camera much farther"). The play view is GASP's CameraAsset_SandboxCharacter (GameplayCameras);
// its rigs are GASP content, kept untouched so a GASP update still applies. The extra distance is a camera modifier on top of whatever the
// view target computes, driven by console variables (DefaultEngine.ini [ConsoleVariables] sets the playtest value; 0 = off).

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_PlaytestCamera.generated.h"

/**
 * Pushes the final view back along its own forward by gstn.Camera.PlaytestExtraDistance and up by gstn.Camera.PlaytestExtraHeight, sweeping
 * from the unmodified view so a wall stops it short. Presentation only: never read by the simulation
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_PlaytestCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	/** The pushed back location from the view, its forward and the extra distance and height (no collision). Pure, for the tests */
	static FVector PushBack(const FVector& ViewLocation, const FVector& ViewForward, float ExtraDistance, float ExtraHeight);

protected:
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
};

/** Adds the playtest camera modifier to every local player's camera manager in a game world, once (camera managers come and go with players) */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_PlaytestCameraSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
};
