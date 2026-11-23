// Camera pivot queries for custom cameras (owner 2026-11-20). See docs/architecture/camera-anchor.md

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_CameraAnchorLibrary.generated.h"

class AActor;

/**
 * Where a camera should look at a character, without the jitter that capsule size changes (crouch, dair tuck, knockdown, dropkick) and the
 * 62.5 Hz simulation steps put on the actor location. Use these instead of GetActorLocation in any custom camera.
 *
 * WHEN to call: from the camera's own update, i.e. inside the Player Camera Manager update (BlueprintUpdateCamera, a camera modifier,
 * a camera component's GetCameraView) or from a tick in TG_PostPhysics or earlier. A tick in TG_PostUpdateWork runs AFTER the view of the frame
 * was built: whatever it moves shows one frame late, which is exactly the jitter fixed on 2026-11-20.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_CameraAnchorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The character's camera pivot this frame, undamped: its actor root as if its capsule were the standing one. Horizontal from the drawn
	 * (smoothed) mesh; height from the capsule bottom + standing half height (EGSTN_CameraVerticalSource::CapsuleBottom), so no capsule change
	 * on the ground (crouch, landing, knockdown) moves it at all. The height steps at the 62.5 Hz simulation rate on slopes, stairs and jumps:
	 * damp it (or use GetDampedCameraPivot). Same height as the standing actor
	 * location (capsule centre). Returns false and the actor location when the character has no mesh to read
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Camera", meta = (DefaultToSelf = "Character", ReturnDisplayName = "Found Mesh"))
	static bool GetCameraPivot(const AActor* Character, FVector& Location);

	/**
	 * The same pivot with the project camera's damping (DA_CameraAnchor_Default: vertical spring, air hold, landing catch up), as the
	 * built-in camera follows it. Needs the camera anchor running on the character (it runs on the pawn a local player views, gstn.Camera.Anchor 1);
	 * otherwise returns the undamped pivot and Damped = false. Look ahead and aim offsets are not included: those belong to each camera
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Camera", meta = (DefaultToSelf = "Character"))
	static FVector GetDampedCameraPivot(const AActor* Character, bool& bDamped);
};
