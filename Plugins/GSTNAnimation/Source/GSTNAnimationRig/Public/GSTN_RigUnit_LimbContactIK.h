// Control Rig units of the animation plugin (presentation only).

#pragma once

#include "CoreMinimal.h"
#include "Rigs/RigHierarchyCache.h"
#include "Units/RigUnit.h"
#include "GSTN_LimbContactTypes.h"
#include "GSTN_RigUnit_LimbContactIK.generated.h"

/**
 * Pulls one two-bone limb (shoulder-elbow-hand, thigh-knee-foot) onto a surface when the animation brings its effector near it
 * (limb-contact-ik.md). One node per limb, chained in the Forwards Solve of a rig such as CR_GSTN_WallLimbContact.
 *
 * Per frame:
 *   read the animated Root, Joint, Effector (global = component space)
 *   contact = UGSTN_LimbContactUtils::ComputeContact(effector, root, chain length, Surface, Settings)
 *   Alpha   = StepContactAlpha(previous Alpha, contact.TargetAlpha * Weight)      (smoothed over time, kept in the node)
 *   goal    = lerp(animated effector, contact.TargetLocation, Alpha)
 *   two-bone IK of Root/Joint towards goal, bend plane from the animated Joint (the clip keeps its elbow and knee direction)
 *   effector rotation turned by Settings.RotationAlpha * Alpha so that Settings.ContactAxis points into the surface
 *
 * Visual only: it changes the evaluated pose, which nothing in the simulation reads.
 */
USTRUCT(meta = (DisplayName = "GSTN Limb Contact IK", Category = "GSTN|Limb Contact", Keywords = "Wall,Surface,Hand,Foot,Contact,TwoBone,IK", NodeColor = "0.1 0.6 0.6"))
struct GSTNANIMATIONRIG_API FGSTN_RigUnit_LimbContactIK : public FRigUnitMutable
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	/** First bone of the chain: upper arm or thigh. The IK rotates it; it is also where the game thread probes from */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Root = FRigElementKey(NAME_None, ERigElementType::Bone);

	/** Middle bone: lower arm or calf */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Joint = FRigElementKey(NAME_None, ERigElementType::Bone);

	/** End bone: hand or foot. Its children (fingers, toes) follow it */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Effector = FRigElementKey(NAME_None, ERigElementType::Bone);

	/** Surface under this limb, in global (component) space. Invalid = release the limb */
	UPROPERTY(meta = (Input))
	FGSTN_LimbSurface Surface;

	UPROPERTY(meta = (Input))
	FGSTN_LimbContactSettings Settings;

	/** Overall weight of this limb, [0, 1]: the feature's fade (0 off the wall) times any per-limb choice */
	UPROPERTY(meta = (Input, ClampMin = "0", ClampMax = "1"))
	float Weight = 1.f;

	/** Contact weight applied this frame, after smoothing */
	UPROPERTY(meta = (Output))
	float Alpha = 0.f;

	/** Where the effector goes at full weight (global space). For debug drawing */
	UPROPERTY(meta = (Output))
	FVector Target = FVector::ZeroVector;

	UPROPERTY()
	FCachedRigElement CachedRoot;

	UPROPERTY()
	FCachedRigElement CachedJoint;

	UPROPERTY()
	FCachedRigElement CachedEffector;

	/** Smoothed contact weight carried between frames (work state, not a pin) */
	UPROPERTY()
	float SmoothedAlpha = 0.f;
};
