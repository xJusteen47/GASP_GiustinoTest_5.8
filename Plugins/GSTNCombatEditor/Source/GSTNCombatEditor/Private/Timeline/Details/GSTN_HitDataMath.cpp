// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Details/GSTN_HitDataMath.h"

#include "Hits/GSTN_HitMathUtils.h"
#include "Hits/GSTN_KnockbackModel.h"
#include "Input/GSTN_CombatSettings.h"
#include "Rules/GSTN_CombatRules.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

float GSTNHitDataDial::AngleFromPoint(const FVector2D& Pivot, const FVector2D& Point, float SnapDegrees)
{
	// Screen y grows down: the elevation is measured with up as positive
	const double Forward = Point.X - Pivot.X;
	const double Up = Pivot.Y - Point.Y;
	double Degrees = 0.0;
	if (Forward <= 0.0)
	{
		// Behind the attacker the needle has nowhere to go but straight up or straight down
		Degrees = Up >= 0.0 ? 90.0 : -90.0;
	}
	else
	{
		Degrees = FMath::RadiansToDegrees(FMath::Atan2(Up, Forward));
	}
	if (SnapDegrees > 0.f)
	{
		Degrees = FMath::RoundToDouble(Degrees / SnapDegrees) * SnapDegrees;
	}
	return static_cast<float>(FMath::Clamp(Degrees, static_cast<double>(MinAngle), static_cast<double>(MaxAngle)));
}

FVector2D GSTNHitDataDial::PointOnArc(const FVector2D& Pivot, double Radius, float AngleDeg)
{
	const double Radians = FMath::DegreesToRadians(static_cast<double>(AngleDeg));
	return FVector2D(Pivot.X + Radius * FMath::Cos(Radians), Pivot.Y - Radius * FMath::Sin(Radians));
}

FVector2D GSTNHitDataDial::PlaneComponents(float AngleDeg)
{
	const double Radians = FMath::DegreesToRadians(static_cast<double>(FMath::Clamp(AngleDeg, MinAngle, MaxAngle)));
	return FVector2D(FMath::Cos(Radians), FMath::Sin(Radians));
}

float GSTNHitDataDial::GetSnapDegrees(bool bShift, bool bCtrl)
{
	return bCtrl ? 15.f : (bShift ? 5.f : 1.f);
}

FText GSTNHitDataText::DescribeYawSource(EGSTN_YawSource YawSource)
{
	switch (YawSource)
	{
	case EGSTN_YawSource::HitboxToVictim: return LOCTEXT("YawHitboxToVictim", "Sends the victim away from the centre of the hitbox: a spin or a swing that hits all around.");
	case EGSTN_YawSource::HitboxVelocity: return LOCTEXT("YawHitboxVelocity", "Sends the victim the way the hitbox is moving: a sweeping blade carries them along.");
	default: return LOCTEXT("YawAttackerFacing", "Sends the victim the way the attacker faces when the hit lands (the default).");
	}
}

FText GSTNHitDataText::DescribeSpecialAngle(EGSTN_SpecialAngle SpecialAngle)
{
	switch (SpecialAngle)
	{
	case EGSTN_SpecialAngle::Sakurai3D: return LOCTEXT("SpecialSakurai", "The angle above is replaced: low on a grounded victim when the knockback is weak, higher otherwise (the rules set both angles).");
	case EGSTN_SpecialAngle::AutoLink: return LOCTEXT("SpecialAutoLink", "The victim is sent the way the hitbox moves, so a combo links on its own. The angle above is the fallback when the hitbox is still.");
	default: return LOCTEXT("SpecialNone", "No special angle: the angle above is used as it is.");
	}
}

FText GSTNHitDataText::DescribeAngle(float AngleDeg)
{
	const FVector2D Plane = GSTNHitDataDial::PlaneComponents(AngleDeg);
	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 2;
	Options.MaximumFractionalDigits = 2;
	const FText Where = FMath::IsNearlyZero(AngleDeg, 0.05f) ? LOCTEXT("AngleLevel", "level with the plane")
		: (AngleDeg > 0.f ? LOCTEXT("AngleAbove", "above the plane") : LOCTEXT("AngleBelow", "below the plane"));
	return FText::Format(LOCTEXT("AngleCaption", "{0} deg, {1}: forward {2}, up {3}"),
		FText::AsNumber(FMath::RoundToInt(AngleDeg)), Where, FText::AsNumber(Plane.X, &Options), FText::AsNumber(Plane.Y, &Options));
}

FText GSTNHitDataText::Summarize(const FGSTN_HitData& Hit)
{
	FNumberFormattingOptions Options;
	Options.MaximumFractionalDigits = 1;
	const FText Knockback = Hit.FixedKnockback >= 0.f
		? FText::Format(LOCTEXT("SummaryFixed", "knockback {0} (fixed)"), FText::AsNumber(Hit.FixedKnockback, &Options))
		: FText::Format(LOCTEXT("SummaryBracket", "knockback {0} + {1}/dmg"), FText::AsNumber(Hit.BaseKnockback, &Options), FText::AsNumber(Hit.KnockbackGrowth, &Options));
	return FText::Format(LOCTEXT("HitSummary", "Damage {0}, {1}, {2} deg"), FText::AsNumber(Hit.Damage, &Options), Knockback, FText::AsNumber(FMath::RoundToInt(Hit.AngleDeg)));
}

const UGSTN_CombatRules* GSTNHitDataPreview::GetActiveRules()
{
	return UGSTN_CombatSettings::Get().ResolveDefaultRules();
}

FText GSTNHitDataPreview::DescribeReaction(EGSTN_HitReaction Reaction)
{
	switch (Reaction)
	{
	case EGSTN_HitReaction::PushBack: return LOCTEXT("TierPushBack", "Push back");
	case EGSTN_HitReaction::Launch: return LOCTEXT("TierLaunch", "Launch");
	default: return LOCTEXT("TierStagger", "Stagger in place");
	}
}

FGSTN_KnockbackPreviewVictim& GSTNHitDataPreview::GetSessionVictim()
{
	static FGSTN_KnockbackPreviewVictim Victim;
	return Victim;
}

FGSTN_KnockbackPreview GSTNHitDataPreview::Compute(const FGSTN_HitData& Hit, const UGSTN_CombatRules* Rules, const FGSTN_KnockbackPreviewVictim& Victim)
{
	FGSTN_KnockbackPreview Preview;
	const UGSTN_KnockbackModel* Model = Rules ? Rules->KnockbackModel.Get() : nullptr;
	if (Model == nullptr)
	{
		Preview.Line = Rules ? LOCTEXT("PreviewNoModel", "The default combat rules have no knockback model: nothing to preview.")
			: LOCTEXT("PreviewNoRules", "No default combat rules are set (Project Settings, GSTN Combat): nothing to preview.");
		return Preview;
	}
	Preview.bHasModel = true;

	// The input of UGSTN_HitStep_ComputeKnockback, for a victim straight ahead of an attacker who faces it, on flat ground, hit on a hurtbox with no scales
	const FVector Forward = FVector::ForwardVector;
	FGSTN_KnockbackInput Input;
	Input.Hit = Hit;
	Input.Up = FVector::UpVector;
	Input.Weight = Victim.Weight;
	Input.DamagePercent = Victim.DamagePercent;
	Input.HealthFraction = 1.f;
	if (Rules->StaggerThreshold > 0.f)
	{
		// The rules' global default of the hitstun condition: the steps copy it into the modifiers before the model runs
		Input.Reaction.StaggerThreshold.Value = Rules->StaggerThreshold;
	}

	// Sakurai3D picks the angle from K, and K does not depend on the direction: read it with no direction first, as the step does
	double Knockback = 0.0;
	if (Hit.SpecialAngle == EGSTN_SpecialAngle::Sakurai3D)
	{
		Input.LaunchDirection = FVector::ZeroVector;
		Knockback = Model->Compute(Input).Knockback;
	}
	Input.LaunchDirection = UGSTN_HitMathUtils::LaunchDirection(Hit.YawSource, Hit.AngleDeg, Hit.SpecialAngle, Input.Up, Forward, Forward, FVector::ZeroVector,
		Victim.bGrounded, static_cast<float>(Knockback), Rules->Sakurai);
	Preview.LaunchDirection = Input.LaunchDirection;
	Preview.EffectiveAngleDeg = static_cast<float>(FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Input.LaunchDirection.Z, -1.0, 1.0))));

	Preview.Result = Model->Compute(Input);
	const FGSTN_KnockbackResult& Result = Preview.Result;

	FNumberFormattingOptions OneDecimal;
	OneDecimal.MaximumFractionalDigits = 1;
	const FText Victim_ = FText::Format(LOCTEXT("PreviewVictim", "victim at {0} %, weight {1}"), FText::AsNumber(Victim.DamagePercent, &OneDecimal), FText::AsNumber(Victim.Weight, &OneDecimal));
	const FText Stagger = Result.bStaggered
		? FText::Format(LOCTEXT("PreviewStaggers", "staggers (force {0}), hitstun {1} f"), FText::AsNumber(Result.StaggerForce, &OneDecimal), FText::AsNumber(Result.HitstunFrames))
		: FText::Format(LOCTEXT("PreviewNoStagger", "does not stagger (force {0}), no hitstun"), FText::AsNumber(Result.StaggerForce, &OneDecimal));
	const FText Tier = Result.bStaggered
		? (Result.bTumble ? FText::Format(LOCTEXT("PreviewTumble", "{0} (tumble)"), DescribeReaction(Result.Reaction)) : DescribeReaction(Result.Reaction))
		: LOCTEXT("PreviewNoTier", "no reaction");
	Preview.Line = FText::Format(LOCTEXT("PreviewLine", "{0}: knockback {1} cm/s, {2}; {3}; hitstop {4} f (attacker {5} f). Attacker faces the victim; direction {6} deg above the plane."),
		Victim_, FText::AsNumber(Result.Knockback, &OneDecimal), Tier, Stagger, FText::AsNumber(Result.HitstopFrames), FText::AsNumber(Result.AttackerHitstopFrames),
		FText::AsNumber(FMath::RoundToInt(Preview.EffectiveAngleDeg)));
	return Preview;
}

#undef LOCTEXT_NAMESPACE
