// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GSTN_CharacterAnimInstance.generated.h"

/**
 * C++ parent for the character's main anim graph (reparent the GASP Mover ABP to it; it adds nothing else, so the GASP graph keeps
 * working as it is). It knows no feature: every frame it asks the linked anim layers that implement IGSTN_OverrideLayerSource whether
 * one wants to take over the pose, and exposes the answer for one "Blend Poses by bool" in front of a Linked Anim Layer node.
 * See the plugin README and docs/architecture/anim-layers.md.
 */
UCLASS(Blueprintable)
class GSTNANIMATIONCORE_API UGSTN_CharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** A linked layer wants to replace the main pose this frame */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Override Layer")
	bool bOverrideLayerActive = false;

	/** Blend time of the winning layer, or of the one that just stopped (so the blend out uses the same time) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Override Layer")
	float OverrideBlendTime = 0.15f;

	/** Class of the winning layer (debug and Blueprint branching); null when none is active */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Override Layer")
	TObjectPtr<UClass> ActiveOverrideLayerClass;

	// Blend-out foot lock (read by FGSTN_AnimNode_BlendOutFootLock, written here on the game thread before the parallel update).
	// The window opens when the last montage stops driving the pose (its blend-out starts, or it is cut) and stays open while the capsule
	// is still, no new montage plays and FootLockMaxHold has not passed. The first frame of a montage is never touched (the "pop" stays).

	/** Plant the feet: a montage is blending back to the game pose and the capsule is still */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Foot Lock")
	bool bFootLockRequest = false;

	/** A montage drives the pose (playing or blending in): any held foot is let go at once */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Foot Lock")
	bool bFootLockInstantRelease = true;

	/** The capsule moves faster than FootLockStillSpeed: held feet blend back instead of stepping */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Foot Lock")
	bool bFootLockMoving = false;

	/** Largest montage weight this frame (the node waits for 0 before the feet step to the game pose) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Foot Lock")
	float FootLockMontageWeight = 0.f;

	/** Debug: phases the node evaluated last, left * 10 + right (0 free, 1 locked, 2 stepping, 3 releasing) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Foot Lock")
	int32 FootLockDebugPhase = 0;

	/** The capsule counts as still below this speed (cm/s) */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Foot Lock", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float FootLockStillSpeed = 10.f;

	/** Longest time the feet stay planted after a montage stops driving the pose; then they step */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Foot Lock", meta = (ClampMin = "0", ForceUnits = "s"))
	float FootLockMaxHold = 1.2f;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void UpdateFootLockRequest(float DeltaSeconds);

	bool bFootLockWindowOpen = false;
	float FootLockWindowTime = 0.f;
	FVector FootLockLastActorLocation = FVector::ZeroVector;
	bool bFootLockHasLastLocation = false;
};
