// Control Rig units of the animation plugin (presentation only).

#pragma once

#include "CoreMinimal.h"
#include "Rigs/RigHierarchyCache.h"
#include "Units/RigUnit.h"
#include "GSTN_LimbContactTypes.h"
#include "GSTN_RigUnit_PreviewLimbSurface.generated.h"

/**
 * Preview wall of a limb contact rig (limb-contact-ik.md "Preview wall"): in the rig editor (or an anim Blueprint editor) there is no game
 * thread probe, so the surfaces coming from the anim instance are empty. When enabled, this node replaces Surface with a flat wall placed
 * by a control (location = a point on the wall, X axis = the side the character is on) and Weight with 1, so the IK can be checked by
 * moving the control. With bOnlyInEditorPreview (default) it passes Surface and Weight through in any world that is not an editor
 * preview world, so a rig saved with the preview on still behaves normally in game and PIE.
 */
USTRUCT(meta = (DisplayName = "GSTN Preview Limb Surface", Category = "GSTN|Limb Contact", Keywords = "Wall,Surface,Preview,Debug", NodeColor = "0.1 0.6 0.6"))
struct GSTNANIMATIONRIG_API FGSTN_RigUnit_PreviewLimbSurface : public FRigUnit
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	/** Use the preview wall instead of the incoming surface */
	UPROPERTY(meta = (Input))
	bool bEnabled = true;

	/** Only in editor preview worlds (rig editor, anim Blueprint editor): never in game or PIE */
	UPROPERTY(meta = (Input))
	bool bOnlyInEditorPreview = true;

	/** Control that places the wall: its X axis is the wall normal */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Wall = FRigElementKey(NAME_None, ERigElementType::Control);

	/** Surface from the anim instance (used when the preview is off) */
	UPROPERTY(meta = (Input))
	FGSTN_LimbSurface Surface;

	/** Weight from the anim instance (used when the preview is off) */
	UPROPERTY(meta = (Input))
	float Weight = 0.f;

	UPROPERTY(meta = (Output))
	FGSTN_LimbSurface ResultSurface;

	UPROPERTY(meta = (Output))
	float ResultWeight = 0.f;

	/** True when the preview wall is the one used this frame */
	UPROPERTY(meta = (Output))
	bool bPreviewActive = false;

	UPROPERTY()
	FCachedRigElement CachedWall;
};
