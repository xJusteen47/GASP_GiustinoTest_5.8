// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GSTN_OverrideLayerSource.h"
#include "WallMovement/Animation/GSTN_WallAnimTypes.h"
#include "GSTN_WallAnimInstance.generated.h"

class UMoverComponent;

/**
 * C++ parent of the wall anim layer (wall-anim.md): the layer Blueprint that implements the wall layer interface derives from this and
 * reads WallAnim through property access. The main character graph does not know about walls: it asks its linked layers whether one
 * wants to override the pose (IGSTN_OverrideLayerSource, see UGSTN_CharacterAnimInstance), so the wall layer can be swapped per kit or
 * skin, or removed, without touching the main graph.
 *
 * Every frame on the game thread, before the graph runs, it gathers the Mover presentation state (UGSTN_WallAnimUtils::GatherInputs) and
 * steps WallAnim (UGSTN_WallAnimUtils::StepWallAnimState). Then it probes the wall under each hand and foot for the limb contact rig
 * (WallLimbContact, limb-contact-ik.md): the layer graph ends with a Control Rig node that pulls the limbs the clip brings near the wall
 * onto it. Presentation only: it reads Mover and the world, never writes to the simulation.
 */
UCLASS(Abstract, Blueprintable)
class GASP_GIUSTINO2_API UGSTN_WallAnimInstance : public UAnimInstance, public IGSTN_OverrideLayerSource
{
	GENERATED_BODY()

public:
	/** Thresholds and phase durations. Tune them on the layer Blueprint's class defaults */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wall|Animation")
	FGSTN_WallAnimSettings Settings;

	/** Blend time the main graph uses to go into and out of this layer (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wall|Animation", meta = (ClampMin = "0", ForceUnits = "s"))
	float OverrideBlendTime = 0.15f;

	/** What the layer graph reads: phase, loop state, speeds on the wall */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wall|Animation")
	FGSTN_WallAnimState WallAnim;

	/** Probe bones and per-mode weights of the limb contact rig */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wall|Animation|Limb Contact")
	FGSTN_WallLimbContactSettings LimbContactSettings;

	/** What the limb contact Control Rig node reads: one surface per limb (component space) and the hand and foot weights */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wall|Animation|Limb Contact")
	FGSTN_WallLimbContactState WallLimbContact;

	//~ Begin IGSTN_OverrideLayerSource
	virtual bool IsOverrideLayerActive() const override { return WallAnim.bIsActive; }
	virtual float GetOverrideBlendTime() const override { return OverrideBlendTime; }
	//~ End IGSTN_OverrideLayerSource

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	/** Found on the owning actor at initialization (and again if lost, e.g. in the editor preview before a pawn exists) */
	TWeakObjectPtr<const UMoverComponent> MoverComponent;

	void FindMoverComponent();
};
