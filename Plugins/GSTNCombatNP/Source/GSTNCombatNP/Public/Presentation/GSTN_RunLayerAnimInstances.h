// Combat presentation (GAS-NPP fork): the run skid and run start anim layers (polish M4). Never read by the simulation.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GSTN_ClipTimeSource.h"
#include "GSTN_OverrideLayerSource.h"
#include "Presentation/GSTN_RunPresentationComponent.h"
#include "GSTN_RunLayerAnimInstances.generated.h"

class UAnimSequenceBase;
class UMoverComponent;

/** Pure timing of the skid clip: the clip time for a frame of the skid (the tests call it) */
struct GSTNCOMBATNP_API FGSTN_SkidClipTime
{
	/**
	 * Enter and Active stretch the clip's [0, StopTime] over the skid's frames from the start to the rest (RestFrame), so the feet stop when the
	 * body does whatever the speed (distance matching by time); Exit plays from StopTime at ExitPlayRate, one sim frame (FrameSeconds) per live
	 * frame. Clamped to [0, Length]. StopTime < 0 = StopFraction of the clip
	 */
	static float Compute(EGSTN_RunSkidPhase Phase, int32 Frame, int32 RestFrame, float Length, float StopTime, float StopFraction, float ExitPlayRate,
		float FrameSeconds);
};

/** Pure run start trigger: started from (nearly) still on the ground, it plays its clip once (the tests call it) */
struct GSTNCOMBATNP_API FGSTN_RunStartClipState
{
	bool bActive = false;
	float Elapsed = 0.f;
	float PreviousSpeed = 0.f;

	/**
	 * One presentation frame: starts when the ground speed crosses StartFromSpeed upwards on the ground, runs for ClipLength / PlayRate, stops early
	 * when the character leaves the ground, slows back under StartFromSpeed or skids. ClipLength <= 0 (no clip) never starts
	 */
	void Step(float DeltaSeconds, float Speed, bool bOnGround, bool bSkidding, float StartFromSpeed, float ClipLength, float PlayRate);
};

/**
 * Parent of the run skid layer Blueprint (ABP_GSTN_SkidLayer, Tools/GAS/setup_skid_anim_layer.py): takes the override slot Skid while the run
 * skid is on (UGSTN_RunPresentationComponent's foot slide, phases Enter/Active/Exit) and shows SkidClip at FGSTN_SkidClipTime through the
 * GSTN Clip At Time node. With a clip set the placeholder lean of the run presentation is turned off. No clip = never active (the base pose).
 */
UCLASS(Blueprintable)
class GSTNCOMBATNP_API UGSTN_SkidAnimInstance : public UAnimInstance, public IGSTN_OverrideLayerSource, public IGSTN_ClipTimeSource
{
	GENERATED_BODY()

public:
	/** The skid clip (owner 2026-11-17: SS_180_Turn2, root fixed by the owner; set by Tools/GAS/setup_skid_anim_layer.py). Root motion is not used */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skid|Animation")
	TObjectPtr<UAnimSequenceBase> SkidClip;

	/** Clip time (s) where the body is at rest; the Enter/Active frames stretch [0, StopTime]. < 0 = StopFraction of the clip */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skid|Animation", meta = (ForceUnits = "s"))
	float StopTime = -1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skid|Animation", meta = (ClampMin = "0.05", ClampMax = "1"))
	float StopFraction = 0.6f;

	/** Rate of the clip after the stop (the recovery) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skid|Animation", meta = (ClampMin = "0.1"))
	float ExitPlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skid|Animation", meta = (ClampMin = "0", ForceUnits = "s"))
	float OverrideBlendTime = 0.08f;

	/** The foot slide this frame and the clip time it gives (debug, Blueprint) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Skid|Animation")
	FGSTN_FootSlideState Slide;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Skid|Animation")
	float ClipTime = 0.f;

	//~ Begin IGSTN_OverrideLayerSource
	virtual bool IsOverrideLayerActive() const override { return bActive; }
	virtual float GetOverrideBlendTime() const override { return OverrideBlendTime; }
	virtual int32 GetOverrideSlot() const override { return GSTNOverrideSlot::Skid; }
	//~ End IGSTN_OverrideLayerSource

	//~ Begin IGSTN_ClipTimeSource
	virtual bool GetClipAtTime(UAnimSequenceBase*& OutClip, float& OutTime) const override;
	//~ End IGSTN_ClipTimeSource

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	TWeakObjectPtr<UGSTN_RunPresentationComponent> Presentation;
	bool bActive = false;
};

/**
 * Parent of the run start layer Blueprint (ABP_GSTN_RunStartLayer): takes the override slot RunStart for one play of RunStartClip when the
 * character starts running from still (FGSTN_RunStartClipState). The clip is DATA and empty by default: which clip is still the owner's call
 * (run-start-skid-import-piano.md 3), so until one is set the layer never takes the pose. Priority below the skid and the wall (slot order).
 */
UCLASS(Blueprintable)
class GSTNCOMBATNP_API UGSTN_RunStartAnimInstance : public UAnimInstance, public IGSTN_OverrideLayerSource, public IGSTN_ClipTimeSource
{
	GENERATED_BODY()

public:
	/** The run start clip (none yet: the layer stays off). Root motion is not used */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run Start|Animation")
	TObjectPtr<UAnimSequenceBase> RunStartClip;

	/** Ground speed (cm/s) under which the character counts as still: crossing it upwards starts the clip, falling back under it stops it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run Start|Animation", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float StartFromSpeed = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run Start|Animation", meta = (ClampMin = "0.1"))
	float PlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Run Start|Animation", meta = (ClampMin = "0", ForceUnits = "s"))
	float OverrideBlendTime = 0.1f;

	//~ Begin IGSTN_OverrideLayerSource
	virtual bool IsOverrideLayerActive() const override { return State.bActive && RunStartClip != nullptr; }
	virtual float GetOverrideBlendTime() const override { return OverrideBlendTime; }
	virtual int32 GetOverrideSlot() const override { return GSTNOverrideSlot::RunStart; }
	//~ End IGSTN_OverrideLayerSource

	//~ Begin IGSTN_ClipTimeSource
	virtual bool GetClipAtTime(UAnimSequenceBase*& OutClip, float& OutTime) const override;
	//~ End IGSTN_ClipTimeSource

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	TWeakObjectPtr<const UMoverComponent> Mover;
	TWeakObjectPtr<const UGSTN_RunPresentationComponent> Presentation;
	FGSTN_RunStartClipState State;
};
