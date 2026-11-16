#include "GSTN_CameraMode.h"

#include "GSTN_CameraMath.h"
#include "GSTN_CameraSection.h"
#include "GSTN_CameraTags.h"

namespace GSTNCameraModePrivate
{
	/** Exponential smoothing of a pivot, independent of the frame rate. The first call snaps */
	static void Damp(FVector& Smoothed, bool& bHas, const FVector& Target, float Seconds, float DeltaSeconds)
	{
		if (!bHas || Seconds <= UE_SMALL_NUMBER)
		{
			Smoothed = Target;
			bHas = true;
			return;
		}
		const double Alpha = 1.0 - FMath::Exp(-DeltaSeconds / Seconds);
		Smoothed += (Target - Smoothed) * Alpha;
	}
}

using namespace GSTNCameraModePrivate;

FGSTN_CameraView UGSTN_CameraMode::ComputeView_Implementation(const FGSTN_CameraModeContext& Context)
{
	FGSTN_CameraView View;
	View.Pivot = Context.Subject.Pivot;
	View.HeadingDeg = Context.Orbit.YawDeg;
	View.PitchDeg = Context.Orbit.PitchDeg;
	return View;
}

// ---- Follow ----

void UGSTN_CameraMode_Follow::ResetState()
{
	bHasPivot = false;
	AutoAlignOffsetDeg = 0.f;
}

FGSTN_CameraView UGSTN_CameraMode_Follow::ComputeView_Implementation(const FGSTN_CameraModeContext& Context)
{
	FVector Pivot = Context.Subject.Pivot;
	float PitchExtra = 0.f;
	float ArmExtra = 0.f;
	AdjustFollow(Context, Pivot, PitchExtra, ArmExtra);
	Damp(SmoothedPivot, bHasPivot, Pivot, Tuning.PivotDampingSeconds, Context.DeltaSeconds);

	if (Tuning.bAutoAlignToMovement)
	{
		const FVector Flat = FGSTN_CameraMath::ProjectOnUpPlane(Context.Subject.Velocity, Context.Frame.Up);
		if (!Flat.IsNearlyZero() && Context.Subject.Velocity.Size() > 150.0)
		{
			const float Wanted = FGSTN_CameraMath::ShortestAngleDelta(Context.Orbit.YawDeg, FGSTN_CameraMath::DirectionToHeading(Context.Frame, Flat));
			AutoAlignOffsetDeg = FGSTN_CameraMath::StepAngleToward(AutoAlignOffsetDeg, Wanted, 45.f * Context.DeltaSeconds);
		}
	}

	FGSTN_CameraView View;
	View.Pivot = SmoothedPivot;
	View.HeadingDeg = Context.Orbit.YawDeg + AutoAlignOffsetDeg;
	View.PitchDeg = FMath::Clamp(Context.Orbit.PitchDeg + PitchExtra, -89.f, 89.f);
	View.ArmCm = Tuning.DistanceCm * Context.Orbit.DistanceScale + ArmExtra;
	View.FovDeg = Tuning.FovDeg;
	return View;
}

// ---- Free ----

void UGSTN_CameraMode_Free::ResetState()
{
	Super::ResetState();
	bHasGround = false;
	LookAhead = FVector::ZeroVector;
}

void UGSTN_CameraMode_Free::AdjustFollow(const FGSTN_CameraModeContext& Context, FVector& InOutPivot, float& OutPitchExtraDeg, float& OutArmExtraCm)
{
	const FVector Up = Context.Frame.Up;
	const double Height = FVector::DotProduct(InOutPivot, Up);
	if (!bHasGround)
	{
		GroundHeight = static_cast<float>(Height);
		bHasGround = true;
	}
	else
	{
		GroundHeight = FGSTN_CameraMath::StepGroundHeight(GroundHeight, static_cast<float>(Height), Context.Subject.bGrounded, FreeTuning.JumpDeadZoneUpCm, FreeTuning.FallFollowCm);
	}
	// The pivot height follows the last ground, not the jump arc
	InOutPivot -= Up * (Height - GroundHeight);

	const FVector Horizontal = Context.Subject.Velocity - Up * FVector::DotProduct(Context.Subject.Velocity, Up);
	const float Speed = static_cast<float>(Horizontal.Size());
	const float Factor = FMath::Clamp(Speed / FMath::Max(1.f, FreeTuning.LookAheadFullSpeed), 0.f, 1.f);
	const FVector Wanted = Speed > UE_SMALL_NUMBER ? Horizontal / Speed * FreeTuning.LookAheadCm * Factor * (Context.Subject.bGrounded ? 1.0 : 0.5) : FVector::ZeroVector;
	const double Alpha = 1.0 - FMath::Exp(-Context.DeltaSeconds / FMath::Max(0.01f, FreeTuning.LookAheadSeconds));
	LookAhead += (Wanted - LookAhead) * Alpha;
	InOutPivot += LookAhead;

	if (FreeTuning.bPitchDownWhenFalling && !Context.Subject.bGrounded)
	{
		const float Below = FMath::Max(0.f, GroundHeight - static_cast<float>(Height));
		OutPitchExtraDeg = -FreeTuning.FallPitchExtraDeg * FMath::Clamp(Below / 300.f, 0.f, 1.f);
	}
	OutArmExtraCm = FreeTuning.SpeedZoomOutCm * Factor;
}

// ---- Shoulder ----

FGSTN_CameraView UGSTN_CameraMode_Shoulder::ComputeView_Implementation(const FGSTN_CameraModeContext& Context)
{
	Damp(SmoothedPivot, bHasPivot, Context.Subject.Pivot, 0.04f, Context.DeltaSeconds);
	FGSTN_CameraView View;
	View.Pivot = SmoothedPivot;
	View.HeadingDeg = Context.Orbit.YawDeg;
	View.PitchDeg = Context.Orbit.PitchDeg;
	View.ArmCm = Tuning.ArmCm;
	// The side is a blend value, so a swap slides the camera across instead of snapping; the aim stays correct because the converged aim
	// is computed from the resulting pose (the point under the view centre), whichever side the camera is on
	const EGSTN_ShoulderSide Side = Context.bHasShoulderSideOverride ? Context.ShoulderSideOverride : Tuning.ShoulderSide;
	if (!bHasSide)
	{
		SideBlend = Side == EGSTN_ShoulderSide::Right ? 1.f : -1.f;
		bHasSide = true;
	}
	SideBlend = FGSTN_CameraMath::StepShoulderBlend(SideBlend, Side, Tuning.ShoulderSwapBlendTime, Context.DeltaSeconds);
	View.SideOffsetCm = SideBlend * Tuning.ShoulderOffsetCm;
	View.UpOffsetCm = Tuning.UpOffsetCm;
	View.FovDeg = Tuning.FovDeg;
	return View;
}

// ---- Section ----

FGSTN_CameraView UGSTN_CameraMode_Section::ComputeView_Implementation(const FGSTN_CameraModeContext& Context)
{
	Damp(SmoothedPivot, bHasPivot, Context.Subject.Pivot, PivotDampingSeconds, Context.DeltaSeconds);
	const AGSTN_CameraSection* Section = Context.Section;
	if (!Section)
	{
		FGSTN_CameraView Fallback;
		Fallback.Pivot = SmoothedPivot;
		Fallback.HeadingDeg = Context.Orbit.YawDeg;
		Fallback.PitchDeg = Context.Orbit.PitchDeg;
		return Fallback;
	}

	const FGSTN_CameraSectionSpec& Spec = Section->Spec;
	const FTransform SectionTransform = Section->GetActorTransform();
	const FVector Focus = SmoothedPivot + SectionTransform.TransformVectorNoScale(Spec.FocalOffset);

	if (Spec.Path != EGSTN_CameraSectionPath::Orbit)
	{
		FVector CameraLocation = SectionTransform.GetLocation();
		if (Spec.Path == EGSTN_CameraSectionPath::Rail)
		{
			TArray<FVector> Points;
			Section->GetRailPoints(Points);
			float Along = 0.f;
			if (!FGSTN_CameraMath::ProjectOnPolyline(Points, SmoothedPivot, CameraLocation, Along))
			{
				// A rail without spline points degrades to a fixed point at the actor
				CameraLocation = SectionTransform.GetLocation();
			}
		}
		CameraLocation += SectionTransform.TransformVectorNoScale(Spec.CameraOffset);
		return FGSTN_CameraMath::ViewLookingAt(Context.Frame, CameraLocation, Focus, Spec.FovDeg);
	}

	FGSTN_CameraView View;
	View.Pivot = Focus;
	View.HeadingDeg = Context.Orbit.YawDeg;
	View.PitchDeg = Context.Orbit.PitchDeg;
	View.ArmCm = Spec.DistanceCm * Context.Orbit.DistanceScale;
	View.FovDeg = Spec.FovDeg;
	return View;
}

// ---- Duel ----

FGSTN_CameraView UGSTN_CameraMode_Duel::ComputeView_Implementation(const FGSTN_CameraModeContext& Context)
{
	if (!Context.bHasOpponent)
	{
		// Alone: a plain view around the subject, so a missing opponent never leaves the camera without a frame
		Damp(SmoothedPivot, bHasPivot, Context.Subject.Pivot, 0.2f, Context.DeltaSeconds);
		FGSTN_CameraView Alone;
		Alone.Pivot = SmoothedPivot;
		Alone.HeadingDeg = DuelState.bInitialised ? DuelState.HeadingDeg : Context.Orbit.YawDeg;
		Alone.PitchDeg = Tuning.PitchDeg;
		Alone.ArmCm = Tuning.DuelMinArmCm;
		Alone.FovDeg = Tuning.FovDeg;
		return Alone;
	}

	const FVector A = Context.Subject.Pivot;
	const FVector B = Context.Opponent.Pivot;
	const FVector Up = Context.Frame.Up;
	// The axis follows the line between the fighters, the side of the line is held through a cross-over and the yaw rate is limited (CAM.8)
	const bool bHaveHeading = FGSTN_CameraMath::StepDuelCamera(DuelState, Context.Frame, A, B, Tuning.bLocalPlayerOnLeft, Context.bBothGrounded,
		Tuning.DuelMinSeparation, Tuning.DuelSideSwapDelay, Tuning.DuelSideSwapMaxDelay, Tuning.DuelMaxYawRateDegPerSec, Context.DeltaSeconds);
	const float HeadingDeg = bHaveHeading ? DuelState.HeadingDeg : Context.Orbit.YawDeg;

	const FVector Delta = B - A;
	const double Vertical = FMath::Abs(FVector::DotProduct(Delta, Up));
	const double Horizontal = (Delta - Up * FVector::DotProduct(Delta, Up)).Size();
	Damp(SmoothedPivot, bHasPivot, (A + B) * 0.5, 0.1f, Context.DeltaSeconds);

	FGSTN_CameraView View;
	View.Pivot = SmoothedPivot;
	View.HeadingDeg = HeadingDeg;
	View.PitchDeg = Tuning.PitchDeg;
	View.FovDeg = Tuning.FovDeg;
	View.ArmCm = FGSTN_CameraMath::DuelArm(static_cast<float>(Horizontal), static_cast<float>(Vertical), Tuning.DuelMarginCm, Tuning.FovDeg, Context.AspectRatio, Tuning.DuelMinArmCm, Tuning.DuelMaxArmCm);
	return View;
}

// ---- Arena ----

FGSTN_CameraView UGSTN_CameraMode_Arena::ComputeView_Implementation(const FGSTN_CameraModeContext& Context)
{
	// Every fighter, the local pawn first (it weighs more in the centroid)
	TArray<FVector> Fighters;
	Fighters.Reserve(Context.Opponents.Num() + 1);
	Fighters.Add(Context.Subject.Pivot);
	for (const FGSTN_CameraSubjectState& Opponent : Context.Opponents)
	{
		Fighters.Add(Opponent.Pivot);
	}
	const FBox Bounds = Context.bHasArenaBounds ? Context.ArenaBounds : FBox(ForceInit);
	// The yaw depends on the arena only: no fighter position enters it
	const float HeadingDeg = Context.ArenaBaseHeadingDeg + Tuning.YawDeg;

	const FGSTN_ArenaFraming Wanted = FGSTN_CameraMath::ArenaFrameAll(Fighters, Context.Frame, HeadingDeg, Tuning, Context.AspectRatio, Bounds);
	Damp(SmoothedFocus, bHasFocus, Wanted.Focus, Tuning.FocusDampingSeconds, Context.DeltaSeconds);
	// The distance that fits is searched around the damped focus (the one the camera really looks at), then damped itself
	const FGSTN_ArenaFraming Fit = FGSTN_CameraMath::ArenaFrameAll(Fighters, Context.Frame, HeadingDeg, Tuning, Context.AspectRatio, Bounds, &SmoothedFocus);
	if (!bHasArm)
	{
		SmoothedArm = Fit.ArmCm;
		bHasArm = true;
	}
	else
	{
		SmoothedArm = FGSTN_CameraMath::StepArenaZoom(SmoothedArm, Fit.ArmCm, Tuning.ZoomInSeconds, Tuning.ZoomOutSeconds, Context.DeltaSeconds);
	}

	FGSTN_CameraView View;
	View.Pivot = SmoothedFocus;
	View.HeadingDeg = HeadingDeg;
	View.PitchDeg = FGSTN_CameraMath::ArenaPitchForArm(Tuning, SmoothedArm);
	View.ArmCm = SmoothedArm;
	View.FovDeg = Tuning.FovDeg;
	return View;
}

// ---- Definition and rule set ----

bool UGSTN_CameraModeDefinition::IsValidDefinition() const
{
	return Mode && ModeTag.IsValid() && ModeTag.MatchesTag(GSTNCameraTags::Mode_Follow.GetTag().RequestDirectParent());
}

UGSTN_CameraRuleSet* UGSTN_CameraRuleSet::MakeDefault(UObject* Outer, const FGameplayTag& DefaultModeTag, float ExitDelaySeconds, float MinDwellSeconds)
{
	UGSTN_CameraRuleSet* Set = NewObject<UGSTN_CameraRuleSet>(Outer);
	Set->MinDwellSeconds = MinDwellSeconds;

	FGameplayTagQueryExpression AimTags;
	AimTags.AllTagsMatch().AddTag(GSTNCameraTags::Request_Aim);
	FGameplayTagQueryExpression BlockedTags;
	BlockedTags.NoTagsMatch().AddTag(GSTNCameraTags::State_ShoulderBlocked);
	FGameplayTagQueryExpression AimExpr;
	AimExpr.AllExprMatch().AddExpr(AimTags).AddExpr(BlockedTags);

	FGSTN_CameraModeRule& Aim = Set->Rules.AddDefaulted_GetRef();
	Aim.Query = FGameplayTagQuery::BuildQuery(AimExpr);
	Aim.ModeTag = GSTNCameraTags::Mode_Shoulder;
	Aim.Priority = 100;
	Aim.ExitDelaySeconds = ExitDelaySeconds;

	FGameplayTagQueryExpression SectionExpr;
	SectionExpr.AllTagsMatch().AddTag(GSTNCameraTags::State_InSection);
	// Group framing of the registered opponents (CAM.7): one is a duel, two or more an arena fight. Above the section, below the aim
	FGameplayTagQueryExpression OneExpr;
	OneExpr.AllTagsMatch().AddTag(GSTNCameraTags::State_OneOpponent);
	FGSTN_CameraModeRule& Duel = Set->Rules.AddDefaulted_GetRef();
	Duel.Query = FGameplayTagQuery::BuildQuery(OneExpr);
	Duel.ModeTag = GSTNCameraTags::Mode_Duel;
	Duel.Priority = 90;
	Duel.ExitDelaySeconds = ExitDelaySeconds;

	FGameplayTagQueryExpression ManyExpr;
	ManyExpr.AllTagsMatch().AddTag(GSTNCameraTags::State_ManyOpponents);
	FGSTN_CameraModeRule& Arena = Set->Rules.AddDefaulted_GetRef();
	Arena.Query = FGameplayTagQuery::BuildQuery(ManyExpr);
	Arena.ModeTag = GSTNCameraTags::Mode_Arena;
	Arena.Priority = 90;
	Arena.ExitDelaySeconds = ExitDelaySeconds;

	FGSTN_CameraModeRule& InSection = Set->Rules.AddDefaulted_GetRef();
	InSection.Query = FGameplayTagQuery::BuildQuery(SectionExpr);
	InSection.bModeFromSection = true;
	InSection.Priority = 50;
	InSection.ExitDelaySeconds = ExitDelaySeconds;

	FGSTN_CameraModeRule& Default = Set->Rules.AddDefaulted_GetRef();
	Default.ModeTag = DefaultModeTag.IsValid() ? DefaultModeTag : GSTNCameraTags::Mode_Follow.GetTag();
	Default.Priority = 0;
	Default.ExitDelaySeconds = ExitDelaySeconds;
	return Set;
}
