// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "GSTN_AnimNode_StanceWidth.generated.h"

/**
 * Stance width layer (owner 2026-11-21, ambito: locomotion clips that opt in; first user: run start w2 -> variant 22).
 * Widens the feet sideways without re-authoring the clip. Each foot is moved along the character's lateral axis (the root bone's frame, so
 * it follows the root yaw of the turnaround starts) and the leg is re-solved by a two-bone IK (thigh, calf, foot) that keeps the knee plane
 * and the foot rotation; ball and toes follow. Heights are untouched, so planted feet stay planted while the weight is constant.
 *
 *   outward move = (Offset + soft floor to MinHalfWidth) x weight
 *   Offset       gstn.Loco.StanceWidth (cm per foot, outward)
 *   MinHalfWidth gstn.Loco.StanceMinHalf (cm from the root's midline; a foot closer than this, or crossing, is pushed out to it)
 *   weight       per foot: the anim curve LeftCurve / RightCurve of the pose (blended by the blend stack like any curve, 0 when the playing
 *                clip has no such curve), or gstn.Loco.StanceWidth.Force when larger (try the layer on any clip).
 *
 * The IK foot bones (ik_foot_l / _r) get the same move, so a later reader of them stays in step. Place it at the END of the main AnimGraph
 * (after GASP's leg IK, which would otherwise snap the feet back). With every weight 0 the pose passes through untouched.
 */
USTRUCT(BlueprintInternalUseOnly)
struct GSTNANIMATIONCORE_API FGSTN_AnimNode_StanceWidth : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Links")
	FPoseLink Source;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName LeftFoot = TEXT("foot_l");

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName RightFoot = TEXT("foot_r");

	/** Same move as the FK foot; None or missing = skipped */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName LeftIKFoot = TEXT("ik_foot_l");

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName RightIKFoot = TEXT("ik_foot_r");

	/** Bone whose frame gives the lateral axis (the root carries the clip's yaw) */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName AxisBone = TEXT("root");

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName LeftCurve = TEXT("GSTN_StanceWidth_L");

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName RightCurve = TEXT("GSTN_StanceWidth_R");

	/** Softness of the MinHalfWidth floor (cm): 0 = hard clamp */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float FloorSoftness = 2.f;

	// FAnimNode_Base
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

	/** Outward move (cm) of a foot whose outward lateral distance from the midline is Lateral; pure so tests can call it */
	static double OutwardMove(double Lateral, double Offset, double MinHalfWidth, double Softness);

private:
	struct FLeg
	{
		FCompactPoseBoneIndex Foot = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Knee = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex Hip = FCompactPoseBoneIndex(INDEX_NONE);
		FCompactPoseBoneIndex IKFoot = FCompactPoseBoneIndex(INDEX_NONE);
		double Side = 1.0;   // +1: outward is +SideAxis
		double LastMove = 0.0;
		float LastWeight = 0.f;
	};

	FLeg Legs[2];
	FCompactPoseBoneIndex Axis = FCompactPoseBoneIndex(INDEX_NONE);
	FVector SideAxis = FVector(1.0, 0.0, 0.0);   // in the axis bone's frame, horizontal, from the right foot to the left foot
	bool bValid = false;

	// Read on the update (cvars)
	float Offset = 0.f;
	float MinHalfWidth = 0.f;
	float Force = 0.f;
};
