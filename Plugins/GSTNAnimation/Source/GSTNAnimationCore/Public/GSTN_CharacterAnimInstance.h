// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GSTN_CharacterAnimInstance.generated.h"

class UGSTN_StanceDefinition;

/**
 * C++ parent for the character's main anim graph (reparent the GASP Mover ABP to it; it adds nothing else, so the GASP graph keeps
 * working as it is). It knows no feature: every frame it asks the linked anim layers that implement IGSTN_OverrideLayerSource whether
 * one wants to take over the pose, and exposes the winner's slot for one "Blend Poses by int" whose pose N is the Linked Anim Layer of
 * slot N (GSTNOverrideSlot; the setup script Tools/GAS/setup_anim_layers.py builds it).
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

	/** Slot of the winning layer (GSTNOverrideSlot), 0 = the base pose: drives the seam's Blend Poses by int */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Override Layer")
	int32 ActiveOverrideSlot = 0;

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

	// Upper-body stance offset (read by FGSTN_AnimNode_StanceOffset, written here on the game thread). The chosen stance lives only at
	// rest: off while a montage drives the pose or the capsule moves, back after a move per the stance's ReturnTiming / ReturnDelay.

	/** Stance used until SetStance is called (null = none: the stance node passes the pose through) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Stance")
	TObjectPtr<UGSTN_StanceDefinition> DefaultStance;

	/** The capsule counts as still below this speed (cm/s) for the stance */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Stance", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float StanceStillSpeed = 10.f;

	/** Chooses the upper-body stance (a kit's or a skin's); null = none. Presentation only */
	UFUNCTION(BlueprintCallable, Category = "Animation|Stance")
	void SetStance(UGSTN_StanceDefinition* NewStance);

	UFUNCTION(BlueprintPure, Category = "Animation|Stance")
	UGSTN_StanceDefinition* GetStance() const { return ActiveStance; }

	/** The stance in use */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Stance")
	TObjectPtr<UGSTN_StanceDefinition> ActiveStance;

	/** Seconds since the stance started coming back; below 0 = not shown (waiting for the return trigger or the delay) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Stance")
	float StanceReturnTime = -1.f;

	/** 1 at rest, falls to 0 over the stance's FadeOutTime while the capsule moves */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Stance")
	float StanceRestAlpha = 1.f;

	/** 1 - the largest montage weight: the stance yields to a montage as it blends in */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Stance")
	float StanceMontageScale = 1.f;

	/** Changes when the stance asset is switched (the node blends from its last output) */
	UPROPERTY(Transient)
	int32 StanceSerial = 0;

	/** Debug for captures: 0 no stance, 1 hidden (montage or waiting for the trigger), 2 delay, 3 returning or shown, 4 moving */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Stance")
	int32 StanceDebugState = 0;

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void UpdateFootLockRequest(float DeltaSeconds);
	void UpdateStance(float DeltaSeconds);

	bool bStanceWaitingTrigger = false;
	bool bStanceWasMontage = false;
	float StanceDelayLeft = -1.f;
	FString StanceTestApplied;
	float CapsuleSpeed = 0.f;          // this frame, from UpdateFootLockRequest
	bool bMontageDrivingPose = false;  // this frame, from UpdateFootLockRequest

	bool bFootLockWindowOpen = false;
	float FootLockWindowTime = 0.f;
	FVector FootLockLastActorLocation = FVector::ZeroVector;
	bool bFootLockHasLastLocation = false;
};
