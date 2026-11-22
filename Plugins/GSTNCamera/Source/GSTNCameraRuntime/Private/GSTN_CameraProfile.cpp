#include "GSTN_CameraProfile.h"

namespace GSTNCameraProfilePrivate
{
	static void AddLinearKey(FRuntimeFloatCurve& Curve, float Time, float Value)
	{
		FRichCurve* Rich = Curve.GetRichCurve();
		const FKeyHandle Handle = Rich->AddKey(Time, Value);
		Rich->SetKeyInterpMode(Handle, RCIM_Linear);
	}

	static float Eval(const FRuntimeFloatCurve& Curve, float Time, float Neutral)
	{
		const FRichCurve* Rich = Curve.GetRichCurveConst();
		if (Curve.ExternalCurve)
		{
			return Curve.ExternalCurve->GetFloatValue(Time);
		}
		return (Rich && Rich->GetNumKeys() > 0) ? Rich->Eval(Time, Neutral) : Neutral;
	}
}

float FGSTN_CameraPitchCurves::EvalDistanceScale(float PitchDeg) const
{
	return FMath::Max(0.05f, GSTNCameraProfilePrivate::Eval(DistanceScaleByPitch, PitchDeg, 1.f));
}

float FGSTN_CameraPitchCurves::EvalFovOffset(float PitchDeg) const
{
	return GSTNCameraProfilePrivate::Eval(FovOffsetByPitch, PitchDeg, 0.f);
}

UGSTN_CameraProfile::UGSTN_CameraProfile()
{
	ShoulderOcclusion.Policy = EGSTN_CameraOcclusionPolicy::PullIn;

	FollowBlend.BlendSeconds = 0.6f;
	ShoulderBlend.BlendSeconds = 0.25f;
	ShoulderBlend.Function = EGSTN_CameraBlendFunction::EaseOut;
	DuelBlend.BlendSeconds = 0.5f;
	ArenaBlend.BlendSeconds = 0.5f;

	// Free camera starting values (design.md §4, [A] until measured on video in P1b): Astro Bot pitch ~18 down, wide horizontal FOV
	FreeView.DistanceCm = 650.f;
	FreeView.PitchDeg = -18.f;
	FreeView.FovDeg = 85.f;
	FreeView.PivotDampingSeconds = 0.15f;
	FreeView.MinPitchDeg = -70.f;
	FreeView.MaxPitchDeg = 20.f;

	// Nesky: from high a bit farther, from below closer (neutral at the default pitch)
	GSTNCameraProfilePrivate::AddLinearKey(FreePitchCurves.DistanceScaleByPitch, -70.f, 1.15f);
	GSTNCameraProfilePrivate::AddLinearKey(FreePitchCurves.DistanceScaleByPitch, -18.f, 1.f);
	GSTNCameraProfilePrivate::AddLinearKey(FreePitchCurves.DistanceScaleByPitch, 20.f, 0.75f);
}
