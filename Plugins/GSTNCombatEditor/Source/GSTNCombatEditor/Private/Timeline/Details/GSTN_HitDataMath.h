// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_ReactionTypes.h"

class UGSTN_CombatRules;
class UGSTN_KnockbackModel;

// The pure part of the hit data details row (combat-m5-editor 5.1, plan E12): the angle dial's geometry, the sentences that describe the enums,
// and the knockback preview. No Slate and no property handle in this header, so the automation tests run all of it without a window.

namespace GSTNHitDataDial
{
	/** The elevation range of FGSTN_HitData::AngleDeg */
	constexpr float MinAngle = -90.f;
	constexpr float MaxAngle = 90.f;

	/**
	 * The elevation a point on the dial stands for. The dial is a side view: the pivot is the attacker, the horizontal to the right is the movement
	 * plane in the direction of the launch, up is up. Screen space (y grows downwards). A point behind the pivot (left of it) is straight up or down,
	 * whichever side it is on; the result is rounded to SnapDegrees (0 = not snapped) and kept inside [MinAngle, MaxAngle]
	 */
	float AngleFromPoint(const FVector2D& Pivot, const FVector2D& Point, float SnapDegrees);

	/** The point of the arc of the given radius at the elevation: the inverse of AngleFromPoint, for drawing the needle */
	FVector2D PointOnArc(const FVector2D& Pivot, double Radius, float AngleDeg);

	/** The launch direction in the attacker's own frame, from the elevation alone: x forward along the movement plane, z up. Unit length */
	FVector2D PlaneComponents(float AngleDeg);

	/** Dragging snaps to whole degrees, with Shift to 5 and with Ctrl to 15 (the diagonals and the right angles land on a snap) */
	float GetSnapDegrees(bool bShift, bool bCtrl);
}

namespace GSTNHitDataText
{
	/** A sentence for each yaw source, for the row under the property */
	FText DescribeYawSource(EGSTN_YawSource YawSource);

	/** A sentence for each special angle */
	FText DescribeSpecialAngle(EGSTN_SpecialAngle SpecialAngle);

	/** "45 deg above the plane: forward 0.71, up 0.71" for the dial's caption */
	FText DescribeAngle(float AngleDeg);

	/** The hit's summary on one line, for the collapsed row: damage, knockback (fixed or base + growth) and the angle */
	FText Summarize(const FGSTN_HitData& Hit);
}

/** The victim the knockback preview is computed against: the few numbers the models read */
struct FGSTN_KnockbackPreviewVictim
{
	/** The percent it has taken before this hit (the percent models read it) */
	float DamagePercent = 50.f;

	/** Its weight, in the units of the rules' WeightReference */
	float Weight = 100.f;

	/** On the ground: Sakurai3D picks its low angle for a grounded victim */
	bool bGrounded = true;
};

/** What the preview found for one hit on the test victim */
struct FGSTN_KnockbackPreview
{
	/** False when the rules have no knockback model (the ComputeKnockback step writes nothing then) */
	bool bHasModel = false;

	/** What the model decided, exactly as the pipeline would get it for this victim */
	FGSTN_KnockbackResult Result;

	/** The unit launch direction the pipeline would feed the model (attacker facing +X, victim straight ahead, victim's up +Z) */
	FVector LaunchDirection = FVector::ZeroVector;

	/** The elevation actually used: AngleDeg, or what the special angle replaced it with */
	float EffectiveAngleDeg = 0.f;

	/** The one line the row shows */
	FText Line;
};

namespace GSTNHitDataPreview
{
	/**
	 * One hit on a test victim through the knockback model of the rules, with the same input the ComputeKnockback step builds (UGSTN_HitStep_ComputeKnockback)
	 * and the same call (UGSTN_KnockbackModel::Compute): there is no second copy of the formula, so the preview is the model's answer or nothing. Assumptions the
	 * line states: the attacker faces the victim, the hit lands on a hurtbox with no scales, and the stagger threshold is the rules' global one. Null Rules: no model.
	 */
	FGSTN_KnockbackPreview Compute(const FGSTN_HitData& Hit, const UGSTN_CombatRules* Rules, const FGSTN_KnockbackPreviewVictim& Victim);

	/** The rules the editor previews against: the project's default rules (UGSTN_CombatSettings), null when none is set */
	const UGSTN_CombatRules* GetActiveRules();

	/** The tier as a word ("Stagger in place", "Push back", "Launch") */
	FText DescribeReaction(EGSTN_HitReaction Reaction);

	/** The victim of the preview, kept for the session so every hit data row on screen uses the same one */
	FGSTN_KnockbackPreviewVictim& GetSessionVictim();
}
