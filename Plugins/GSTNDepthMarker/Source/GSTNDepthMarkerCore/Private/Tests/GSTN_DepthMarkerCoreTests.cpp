// Pure automation tests of the depth marker target selection (depth-marker.md §7, GSTNDepthMarker.Core.*). No world.
// Every test runs for several Up vectors (flipped, sideways, diagonal) so a hidden world-Z assumption fails.

#include "GSTN_DepthMarkerMath.h"
#include "GSTN_DepthMarkerTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNDepthMarkerCoreTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	const TArray<FVector>& Ups()
	{
		static const TArray<FVector> Values = {
			FVector(0, 0, 1), FVector(0, 0, -1), FVector(1, 0, 0), FVector(0, 1, 0),
			FVector(1, 1, 1).GetSafeNormal(), FVector(0.3, -0.5, 0.8).GetSafeNormal(), FVector(-0.7, 0.2, -0.4).GetSafeNormal()
		};
		return Values;
	}

	/** One scenario in a frame of reference where Up is "up" and Origin is a point of the ground plane */
	struct FCtx
	{
		FVector Up;
		FVector Side;
		FVector Origin = FVector(123.0, -45.0, 67.0);
		FGSTN_DepthSelectorParams P;
		FGSTN_DepthFadeParams Fade;
		FGSTN_DepthSelectionState St;
		FGameplayTagQuery NoQuery;
		FGSTN_DepthFootprintHit NoFootprint;
		FGSTN_DepthReferenceOverride NoOverride;

		explicit FCtx(const FVector& InUp)
			: Up(InUp)
			, Side(FVector::CrossProduct(InUp, FMath::Abs(InUp.Z) < 0.9 ? FVector::UpVector : FVector::RightVector).GetSafeNormal())
		{
		}

		FVector At(double Along, double Lateral = 0.0) const { return Origin + Up * Along + Side * Lateral; }

		FGSTN_DepthSubjectState Subject(double Along, double Lateral, bool bAirborne, bool bGrounded) const
		{
			FGSTN_DepthSubjectState S;
			S.Feet = At(Along, Lateral);
			S.Up = Up;
			S.bAirborne = bAirborne;
			S.bGrounded = bGrounded;
			return S;
		}

		/** A hit straight below the subject, Height away along Down */
		FGSTN_DepthCandidate Cand(const FGSTN_DepthSubjectState& S, double Height, int64 Key) const
		{
			FGSTN_DepthCandidate C;
			C.Height = static_cast<float>(Height);
			C.Point = S.Feet - Up * Height;
			C.Normal = Up;
			C.SurfaceKey = Key;
			return C;
		}

		FGSTN_DepthResult Eval(const FGSTN_DepthSubjectState& S, const TArray<FGSTN_DepthCandidate>& Cands, double Time, bool bProbed = true)
		{
			return FGSTN_DepthMarkerMath::Evaluate(St, S, Cands, NoFootprint, NoOverride, P, NoQuery, bProbed, Time);
		}

		/** Stand on the plane long enough to store the grounded point */
		void StandOnPlane(double Lateral = 0.0)
		{
			const FGSTN_DepthSubjectState S = Subject(0.0, Lateral, false, true);
			Eval(S, {}, 0.0, false);
			Eval(S, {}, 0.2, false);
		}
	};
}

#define GSTN_DM_TEST(Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNDepthMarkerCoreTest_##Name, "GSTNDepthMarker.Core." #Name, GSTNDepthMarkerCoreTest::Flags) \
	bool FGSTNDepthMarkerCoreTest_##Name::RunTest(const FString& Parameters)

#define GSTN_DM_FOR_EACH_UP \
	for (const FVector& TestUp : GSTNDepthMarkerCoreTest::Ups()) \
		for (GSTNDepthMarkerCoreTest::FCtx C(TestUp), *Once = &C; Once; Once = nullptr)

#define CHECK(Cond) TestTrue(*FString::Printf(TEXT("%s [up %s]"), TEXT(#Cond), *C.Up.ToString()), (Cond))

using namespace GSTNDepthMarkerCoreTest;

GSTN_DM_TEST(FirstSurfaceBelowIgnoresAbove)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FGSTN_DepthSubjectState S = C.Subject(500, 0, true, false);
		TArray<FGSTN_DepthCandidate> In = { C.Cand(S, -50, 1), C.Cand(S, 200, 2) };
		TArray<FGSTN_DepthCandidate> Out;
		FGSTN_DepthMarkerMath::FilterCandidates(In, C.Up, C.P, C.NoQuery, Out);
		CHECK(Out.Num() == 1 && Out[0].SurfaceKey == 2);
		FGSTN_DepthCandidate Chosen;
		CHECK(FGSTN_DepthMarkerMath::ChooseFirstBelow(Out, Chosen) && Chosen.SurfaceKey == 2);
	}
	return true;
}

GSTN_DM_TEST(StackedPlatformsPickNearest)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FGSTN_DepthSubjectState S = C.Subject(1000, 0, true, false);
		TArray<FGSTN_DepthCandidate> In = { C.Cand(S, 900, 3), C.Cand(S, 100, 1), C.Cand(S, 400, 2) };
		const FGSTN_DepthResult R = C.Eval(S, In, 0.0);
		CHECK(R.Below.Kind == EGSTN_DepthTargetKind::Surface && R.Below.SurfaceKey == 1);
		CHECK(FMath::IsNearlyEqual(R.Below.Height, 100.f, 0.01f));
		CHECK(R.Below.Point.Equals(S.Feet - C.Up * 100.0, 0.01));
	}
	return true;
}

GSTN_DM_TEST(StartPenetratingSkipped)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FGSTN_DepthSubjectState S = C.Subject(300, 0, true, false);
		FGSTN_DepthCandidate Inside = C.Cand(S, 0, 1);
		Inside.bStartPenetrating = true;
		TArray<FGSTN_DepthCandidate> In = { Inside, C.Cand(S, 80, 2) };
		const FGSTN_DepthResult R = C.Eval(S, In, 0.0);
		CHECK(R.Below.SurfaceKey == 2);
		// Non-blocking and excluded hits never target either
		FGSTN_DepthCandidate Overlap = C.Cand(S, 10, 3);
		Overlap.bBlocking = false;
		FGSTN_DepthCandidate Pawn = C.Cand(S, 20, 4);
		Pawn.bExcluded = true;
		TArray<FGSTN_DepthCandidate> Out;
		FGSTN_DepthMarkerMath::FilterCandidates({ Overlap, Pawn }, C.Up, C.P, C.NoQuery, Out);
		CHECK(Out.IsEmpty());
	}
	return true;
}

GSTN_DM_TEST(ThinPlatformJustPassedIsAboveFeet)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FGSTN_DepthSubjectState S = C.Subject(300, 0, true, false);
		TArray<FGSTN_DepthCandidate> Out;
		FGSTN_DepthMarkerMath::FilterCandidates({ C.Cand(S, -5, 1) }, C.Up, C.P, C.NoQuery, Out);
		CHECK(Out.IsEmpty());
		FGSTN_DepthMarkerMath::FilterCandidates({ C.Cand(S, -1, 1) }, C.Up, C.P, C.NoQuery, Out);
		CHECK(Out.Num() == 1);
		// The lower floor is chosen, not the platform just left behind
		const FGSTN_DepthResult R = C.Eval(S, { C.Cand(S, -5, 1), C.Cand(S, 300, 2) }, 0.0);
		CHECK(R.Below.SurfaceKey == 2);
	}
	return true;
}

GSTN_DM_TEST(UndersideRejected)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FGSTN_DepthSubjectState S = C.Subject(300, 0, true, false);
		FGSTN_DepthCandidate Under = C.Cand(S, 100, 1);
		Under.Normal = -C.Up;
		FGSTN_DepthCandidate Wall = C.Cand(S, 110, 2);
		Wall.Normal = C.Side;
		FGSTN_DepthCandidate Ramp = C.Cand(S, 120, 3);
		Ramp.Normal = (C.Up + C.Side).GetSafeNormal();
		TArray<FGSTN_DepthCandidate> Out;
		FGSTN_DepthMarkerMath::FilterCandidates({ Under, Wall, Ramp }, C.Up, C.P, C.NoQuery, Out);
		CHECK(Out.Num() == 1 && Out[0].SurfaceKey == 3);
	}
	return true;
}

GSTN_DM_TEST(CoplanarOverlapKeepsPrevious)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FGSTN_DepthSubjectState S = C.Subject(100, 0, true, false);
		C.Eval(S, { C.Cand(S, 100, 1) }, 0.0);
		// An overlapping mesh 1 cm away with the same normal must not take over
		const FGSTN_DepthResult R = C.Eval(S, { C.Cand(S, 101, 2) }, 1.0);
		CHECK(R.Below.SurfaceKey == 1);
		CHECK(R.Below.State == EGSTN_DepthMarkerState::Ground);
	}
	return true;
}

GSTN_DM_TEST(NearerSurfaceSwitchesAtOnce)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FGSTN_DepthSubjectState Far = C.Subject(300, 0, true, false);
		C.Eval(Far, { C.Cand(Far, 300, 1) }, 0.0);
		// The character moved over a higher platform: switch at once, even inside the dwell, with no blend
		const FGSTN_DepthSubjectState Near = C.Subject(300, 20, true, false);
		const FGSTN_DepthResult R = C.Eval(Near, { C.Cand(Near, 100, 2) }, 0.01);
		CHECK(R.Below.SurfaceKey == 2);
		CHECK(R.Below.BlendMs == 0.f);
	}
	return true;
}

GSTN_DM_TEST(EdgeHoldWithinForgiveness)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FGSTN_DepthSubjectState On = C.Subject(100, 0, true, false);
		C.Eval(On, { C.Cand(On, 100, 1) }, 0.0);

		// The centre ray now sees a far floor, the footprint still touches the old platform 10 cm away
		const FGSTN_DepthSubjectState Edge = C.Subject(100, 40, true, false);
		FGSTN_DepthFootprintHit Hit;
		Hit.bValid = true;
		Hit.SurfaceKey = 1;
		Hit.Point = C.At(0, 30);
		Hit.LateralCm = 10.f;
		const FGSTN_DepthResult R = FGSTN_DepthMarkerMath::Evaluate(C.St, Edge, { C.Cand(Edge, 600, 2) }, Hit, C.NoOverride, C.P, C.NoQuery, true, 1.0);
		CHECK(R.Below.SurfaceKey == 1);
		CHECK(R.Below.State == EGSTN_DepthMarkerState::EdgeHold);
		CHECK(FMath::IsNearlyEqual(R.Below.EdgeAlpha, 0.75f, 0.001f));
		CHECK(R.Below.Point.Equals(Hit.Point, 0.01));

		// No candidate at all (a gap) holds the same way
		const FGSTN_DepthResult Gap = FGSTN_DepthMarkerMath::Evaluate(C.St, Edge, {}, Hit, C.NoOverride, C.P, C.NoQuery, true, 1.1);
		CHECK(Gap.Below.State == EGSTN_DepthMarkerState::EdgeHold);
	}
	return true;
}

GSTN_DM_TEST(SwitchBeyondForgivenessWithBlend)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FGSTN_DepthSubjectState On = C.Subject(100, 0, true, false);
		C.Eval(On, { C.Cand(On, 100, 1) }, 0.0);

		const FGSTN_DepthSubjectState Away = C.Subject(100, 90, true, false);
		FGSTN_DepthFootprintHit Hit;
		Hit.bValid = true;
		Hit.SurfaceKey = 1;
		Hit.Point = C.At(0, 30);
		Hit.LateralCm = 60.f; // beyond EdgeForgivenessCm (40)
		const FGSTN_DepthResult R = FGSTN_DepthMarkerMath::Evaluate(C.St, Away, { C.Cand(Away, 600, 2) }, Hit, C.NoOverride, C.P, C.NoQuery, true, 1.0);
		CHECK(R.Below.SurfaceKey == 2);
		CHECK(R.Below.State == EGSTN_DepthMarkerState::Ground);
		CHECK(R.Below.BlendMs == C.P.SwitchBlendMs);
	}
	return true;
}

GSTN_DM_TEST(MinDwellPreventsPingPong)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FGSTN_DepthSubjectState S = C.Subject(700, 0, true, false);
		auto Key = [&](double Time, TArray<FGSTN_DepthCandidate> Cands) { return C.Eval(S, Cands, Time).Below.SurfaceKey; };
		const FGSTN_DepthCandidate Near = C.Cand(S, 100, 1);
		const FGSTN_DepthCandidate Far = C.Cand(S, 500, 2);

		CHECK(Key(0.0, { Near }) == 1);
		// A farther surface right after the first selection is held back by the dwell
		CHECK(Key(0.05, { Far }) == 1);
		CHECK(Key(0.20, { Far }) == 2);
		// ... and the nearer one coming back at once does not switch inside the dwell of that farther switch
		CHECK(Key(0.23, { Near }) == 2);
		CHECK(Key(0.35, { Near }) == 1);

		// Alternating every 30 ms never switches faster than the dwell
		int32 Switches = 0;
		int64 Last = Key(1.0, { Near });
		for (int32 i = 1; i <= 20; ++i)
		{
			const int64 K = Key(1.0 + 0.03 * i, { (i % 2) ? Far : Near });
			Switches += (K != Last) ? 1 : 0;
			Last = K;
		}
		CHECK(Switches <= 6);
	}
	return true;
}

GSTN_DM_TEST(PitUsesReferenceVolume)
{
	GSTN_DM_FOR_EACH_UP
	{
		FGSTN_DepthReferenceOverride Volume;
		Volume.bValid = true;
		Volume.Point = C.At(-30.0);
		Volume.Normal = C.Up;
		Volume.bHazard = true;

		// Off-stage, never stood anywhere: the volume alone gives the ring (above the plane: "no ground here" ring below)
		const FGSTN_DepthSubjectState S = C.Subject(40, 0, true, false);
		FGSTN_DepthResult R = FGSTN_DepthMarkerMath::Evaluate(C.St, S, {}, C.NoFootprint, Volume, C.P, C.NoQuery, true, 0.0);
		CHECK(R.Below.Kind == EGSTN_DepthTargetKind::ReferencePlane && R.Below.State == EGSTN_DepthMarkerState::NoGround);
		CHECK(R.Below.bHazard);
		CHECK(R.Below.Point.Equals(C.At(-30.0), 0.01));

		// Below the volume's plane: the ring above the entity
		const FGSTN_DepthSubjectState Low = C.Subject(-200, 0, true, false);
		R = FGSTN_DepthMarkerMath::Evaluate(C.St, Low, {}, C.NoFootprint, Volume, C.P, C.NoQuery, true, 0.1);
		CHECK(R.Above.State == EGSTN_DepthMarkerState::AboveReference);
		CHECK(R.Above.Point.Equals(C.At(-30.0), 0.01));
		CHECK(!R.Below.IsValid());
	}
	return true;
}

GSTN_DM_TEST(PitUsesLastGroundedPlane)
{
	GSTN_DM_FOR_EACH_UP
	{
		C.StandOnPlane();
		// Launched off the stage over a gap at the same level: the ring sits on the last platform's plane, under the entity
		const FGSTN_DepthSubjectState S = C.Subject(25, 400, true, false);
		const FGSTN_DepthResult R = C.Eval(S, {}, 0.5);
		CHECK(R.Below.State == EGSTN_DepthMarkerState::NoGround);
		CHECK(R.Below.Point.Equals(C.At(0.0, 400.0), 0.01));
		CHECK(FMath::IsNearlyEqual(R.Below.Height, 25.f, 0.01f));
		CHECK(!R.Above.IsValid());
		// Too brief a stay does not update the point: a fresh context that only touched the ground has no plane
		FCtx Fresh(C.Up);
		Fresh.Eval(Fresh.Subject(0, 0, false, true), {}, 0.0, false);
		Fresh.Eval(Fresh.Subject(0, 0, false, true), {}, 0.05, false);
		CHECK(!Fresh.St.bHasGrounded);
	}
	return true;
}

GSTN_DM_TEST(NoReferenceMeansNone)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FGSTN_DepthSubjectState S = C.Subject(-100, 0, true, false);
		const FGSTN_DepthResult R = C.Eval(S, {}, 0.0);
		CHECK(!R.Below.IsValid() && !R.Above.IsValid());
		// A moving base does not set the plane either
		FGSTN_DepthSubjectState OnLift = C.Subject(0, 0, false, true);
		OnLift.bBaseMoves = true;
		C.Eval(OnLift, {}, 0.0, false);
		C.Eval(OnLift, {}, 1.0, false);
		CHECK(!C.St.bHasGrounded);
	}
	return true;
}

GSTN_DM_TEST(RingAboveEntityWhenBelowPlane)
{
	GSTN_DM_FOR_EACH_UP
	{
		C.StandOnPlane();
		const FGSTN_DepthSubjectState S = C.Subject(-300, 80, true, false);
		const FGSTN_DepthResult R = C.Eval(S, {}, 0.5);
		CHECK(R.Above.Kind == EGSTN_DepthTargetKind::ReferencePlane && R.Above.State == EGSTN_DepthMarkerState::AboveReference);
		// On the plane, straight along Up from the feet, 300 cm up the line
		CHECK(R.Above.Point.Equals(C.At(0.0, 80.0), 0.01));
		CHECK(FMath::IsNearlyEqual(R.Above.Height, 300.f, 0.01f));
		CHECK(FVector::DotProduct(R.Above.Point - S.Feet, C.Up) > 0.0);
		// Too far from the plane: nothing
		const FGSTN_DepthSubjectState Far = C.Subject(-5000, 0, true, false);
		CHECK(!C.Eval(Far, {}, 0.6).Above.IsValid());
	}
	return true;
}

GSTN_DM_TEST(GravityFlipUsesNewUp)
{
	GSTN_DM_FOR_EACH_UP
	{
		C.StandOnPlane();
		const FGSTN_DepthSubjectState S = C.Subject(60, 0, true, false);
		C.Eval(S, { C.Cand(S, 60, 1) }, 0.3);
		CHECK(C.St.bHasPrevious);

		// Gravity flips mid-air: the old floor is now an underside, the plane is rebuilt from the point with the new Up
		FGSTN_DepthSubjectState Flipped = S;
		Flipped.Up = -C.Up;
		FGSTN_DepthCandidate OldFloor = C.Cand(S, 60, 1);
		OldFloor.Height = -60.f;
		const FGSTN_DepthResult R = FGSTN_DepthMarkerMath::Evaluate(C.St, Flipped, { OldFloor }, C.NoFootprint, C.NoOverride, C.P, C.NoQuery, true, 0.4);
		CHECK(R.bGravityReset);
		CHECK(!R.Below.IsValid() || R.Below.SurfaceKey != 1);
		// The feet are 60 cm "below" the plane in the new frame: the ring is above them (towards the new Up)
		CHECK(R.Above.State == EGSTN_DepthMarkerState::AboveReference);
		CHECK(R.Above.Normal.Equals(-C.Up, 0.001));
		CHECK(R.Above.Point.Equals(C.At(0.0), 0.01));
	}
	return true;
}

GSTN_DM_TEST(SidewaysGravityPlane)
{
	// Gravity along world X: nothing may be measured along world Z
	FCtx C(FVector(1, 0, 0));
	C.StandOnPlane(250.0);
	const FGSTN_DepthSubjectState S = C.Subject(35, 250.0, true, false);
	const FGSTN_DepthResult R = C.Eval(S, {}, 0.5);
	CHECK(R.Below.State == EGSTN_DepthMarkerState::NoGround);
	CHECK(FMath::IsNearlyEqual(static_cast<float>(R.Below.Point.X), static_cast<float>(C.Origin.X), 0.01f));
	CHECK(FMath::IsNearlyEqual(static_cast<float>(R.Below.Point.Z), static_cast<float>(S.Feet.Z), 0.01f));
	CHECK(FMath::IsNearlyEqual(R.Below.Height, 35.f, 0.01f));
	// The surface search is along X as well
	const FGSTN_DepthResult Surface = C.Eval(S, { C.Cand(S, 135, 7) }, 0.6);
	CHECK(Surface.Below.Point.Equals(S.Feet - FVector(135, 0, 0), 0.01));
	return true;
}

GSTN_DM_TEST(UpResetOnlyOnJump)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FVector Axis = C.Side;
		FGSTN_DepthSubjectState S = C.Subject(100, 0, true, false);
		C.Eval(S, { C.Cand(S, 100, 1) }, 0.0);
		CHECK(C.St.bHasPrevious);
		// Curved gravity: 2 degrees per frame for 20 frames never resets
		bool bAnyReset = false;
		for (int32 i = 1; i <= 20; ++i)
		{
			S.Up = C.Up.RotateAngleAxis(2.0 * i, Axis);
			bAnyReset |= C.Eval(S, { C.Cand(S, 100, 1) }, 0.01 * i).bGravityReset;
		}
		CHECK(!bAnyReset);
		CHECK(C.St.bHasPrevious);
		// A jump of 90 degrees resets and clears the selection
		S.Up = S.Up.RotateAngleAxis(90.0, Axis);
		const FGSTN_DepthResult R = C.Eval(S, {}, 0.5);
		CHECK(R.bGravityReset && R.bStateCleared);
		CHECK(!C.St.bHasPrevious);
	}
	return true;
}

GSTN_DM_TEST(SlopeTiltClamped)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FVector Steep = C.Up.RotateAngleAxis(70.0, C.Side);
		const FVector Clamped = FGSTN_DepthMarkerMath::ClampTilt(Steep, C.Up, 50.f);
		const double Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Clamped, C.Up)));
		CHECK(FMath::IsNearlyEqual(static_cast<float>(Angle), 50.f, 0.1f));
		// Same plane of tilt: the clamped normal stays between Up and the real normal
		CHECK(FVector::DotProduct(Clamped, Steep) > FVector::DotProduct(C.Up, Steep));
		const FVector Gentle = C.Up.RotateAngleAxis(30.0, C.Side);
		CHECK(FGSTN_DepthMarkerMath::ClampTilt(Gentle, C.Up, 50.f).Equals(Gentle, 0.001));
		CHECK(FGSTN_DepthMarkerMath::ClampTilt(C.Up, C.Up, 50.f).Equals(C.Up, 0.001));
		// Cosmetic stretch is capped at 2 and the steep flag follows MinNormalDot
		CHECK(FGSTN_DepthMarkerMath::SlopeRadiusScale(Steep, C.Up) <= 2.f);
		CHECK(FMath::IsNearlyEqual(FGSTN_DepthMarkerMath::SlopeRadiusScale(C.Up, C.Up), 1.f, 0.001f));

		const FGSTN_DepthSubjectState S = C.Subject(200, 0, true, false);
		FGSTN_DepthCandidate Ramp = C.Cand(S, 200, 1);
		Ramp.Normal = Steep;
		const FGSTN_DepthResult R = C.Eval(S, { Ramp }, 0.0);
		CHECK(R.Below.SurfaceKey == 1 && !R.Below.bSteep);
		const double TargetAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(R.Below.PresentNormal, C.Up)));
		CHECK(FMath::IsNearlyEqual(static_cast<float>(TargetAngle), 50.f, 0.1f));
	}
	return true;
}

GSTN_DM_TEST(MovingBaseLocalPlaneFollows)
{
	GSTN_DM_FOR_EACH_UP
	{
		const FGSTN_DepthSubjectState S = C.Subject(200, 0, true, false);
		FGSTN_DepthCandidate Lift = C.Cand(S, 200, 5);
		Lift.bMovingBase = true;
		Lift.ComponentTransform = FTransform(FQuat(C.Up, 0.7), C.At(0.0, 500.0));
		const FGSTN_DepthResult R = C.Eval(S, { Lift }, 0.0);
		CHECK(R.Below.SurfaceKey == 5);

		// The lift rises 50 cm and drifts sideways between traces: the point stays under the feet at the new height
		FTransform Moved = Lift.ComponentTransform;
		Moved.AddToTranslation(C.Up * 50.0 + C.Side * 20.0);
		const FGSTN_DepthTarget Follow = FGSTN_DepthMarkerMath::ReprojectTarget(R.Below, Moved, S.Feet, C.Up);
		CHECK(Follow.Point.Equals(R.Below.Point + C.Up * 50.0, 0.01));
		CHECK(FMath::IsNearlyEqual(Follow.Height, 150.f, 0.01f));
		// A lift that tilts moves the point along the column
		FTransform Tilted = Lift.ComponentTransform;
		Tilted.SetRotation(FQuat(C.Side, 0.3) * Lift.ComponentTransform.GetRotation());
		const FGSTN_DepthTarget TiltedTarget = FGSTN_DepthMarkerMath::ReprojectTarget(R.Below, Tilted, S.Feet, C.Up);
		CHECK(FVector::DotProduct(TiltedTarget.Normal, C.Up) < 0.9999);
		CHECK((TiltedTarget.Point - S.Feet).GetSafeNormal().Equals(-C.Up, 0.001));
	}
	return true;
}

GSTN_DM_TEST(FadeCurvesMonotonic)
{
	FGSTN_DepthFadeParams Fade;
	float PrevAlpha = 2.f, PrevRadius = 2.f;
	for (float Height = 0.f; Height <= 7000.f; Height += 50.f)
	{
		float Alpha = 0.f, Radius = 0.f;
		FGSTN_DepthMarkerMath::ComputeFade(Height, Fade, Alpha, Radius);
		TestTrue(TEXT("alpha never increases with height"), Alpha <= PrevAlpha + KINDA_SMALL_NUMBER);
		TestTrue(TEXT("radius never increases with height"), Radius <= PrevRadius + KINDA_SMALL_NUMBER);
		PrevAlpha = Alpha;
		PrevRadius = Radius;
	}
	float Alpha = 0.f, Radius = 0.f;
	FGSTN_DepthMarkerMath::ComputeFade(0.f, Fade, Alpha, Radius);
	TestEqual(TEXT("near alpha"), Alpha, 1.f);
	TestEqual(TEXT("near radius"), Radius, 1.f);
	FGSTN_DepthMarkerMath::ComputeFade(6000.f, Fade, Alpha, Radius);
	TestEqual(TEXT("far alpha"), Alpha, 0.25f);
	TestEqual(TEXT("far radius"), Radius, 0.6f);
	FGSTN_DepthMarkerMath::ComputeFade(60000.f, Fade, Alpha, Radius);
	TestEqual(TEXT("beyond the end stays faint, not absent"), Alpha, 0.25f);

	// Visibility hysteresis: shown from 60, hidden below 40, never while grounded
	FGSTN_DepthSelectionState St;
	TestFalse(TEXT("50 cm: not yet"), FGSTN_DepthMarkerMath::UpdateVisibility(St, true, 50.f, Fade));
	TestTrue(TEXT("60 cm: shown"), FGSTN_DepthMarkerMath::UpdateVisibility(St, true, 60.f, Fade));
	TestTrue(TEXT("50 cm: stays"), FGSTN_DepthMarkerMath::UpdateVisibility(St, true, 50.f, Fade));
	TestFalse(TEXT("39 cm: hidden"), FGSTN_DepthMarkerMath::UpdateVisibility(St, true, 39.f, Fade));
	TestFalse(TEXT("grounded: hidden"), FGSTN_DepthMarkerMath::UpdateVisibility(St, false, 500.f, Fade));
	return true;
}

GSTN_DM_TEST(AboveAndBelowWhenDroppingToLowerPlatform)
{
	GSTN_DM_FOR_EACH_UP
	{
		// Stood on a high platform (plane at Along 300), now dropping over a lower one 200 cm under the feet
		const FGSTN_DepthSubjectState Stand = C.Subject(300, 0, false, true);
		C.Eval(Stand, {}, 0.0, false);
		C.Eval(Stand, {}, 0.2, false);
		const FGSTN_DepthSubjectState S = C.Subject(200, 400, true, false);
		const FGSTN_DepthResult R = C.Eval(S, { C.Cand(S, 200, 2) }, 0.5);
		CHECK(R.Below.Kind == EGSTN_DepthTargetKind::Surface && R.Below.SurfaceKey == 2);
		CHECK(R.Above.State == EGSTN_DepthMarkerState::AboveReference);
		CHECK(R.Above.Point.Equals(C.At(300.0, 400.0), 0.01));
		CHECK(R.Below.Point.Equals(C.At(0.0, 400.0), 0.01));
	}
	return true;
}

GSTN_DM_TEST(AboveHysteresisAtLip)
{
	GSTN_DM_FOR_EACH_UP
	{
		C.StandOnPlane();
		auto Shown = [&](double Along, double Time) { return C.Eval(C.Subject(Along, 0, true, false), {}, Time).Above.IsValid(); };
		CHECK(!Shown(-40, 1.0));   // 40 deep: not yet (needs 50)
		CHECK(Shown(-60, 1.1));    // 60 deep: shown
		CHECK(Shown(-40, 1.2));    // between 30 and 50: stays
		CHECK(!Shown(-25, 1.3));   // above 30 deep: hidden
		CHECK(!Shown(-40, 1.4));   // does not come back until 50 again
		CHECK(Shown(-60, 1.5));
	}
	return true;
}

GSTN_DM_TEST(AboveClearedOnLanding)
{
	GSTN_DM_FOR_EACH_UP
	{
		C.StandOnPlane();
		CHECK(C.Eval(C.Subject(-200, 0, true, false), {}, 1.0).Above.IsValid());
		CHECK(C.St.bAboveShown);
		// Lands on a lower platform: the ring goes at once, and after the grounded delay the plane is the new platform
		const FGSTN_DepthSubjectState Landed = C.Subject(-200, 0, false, true);
		CHECK(!C.Eval(Landed, {}, 1.1, false).Above.IsValid());
		CHECK(!C.St.bAboveShown);
		C.Eval(Landed, {}, 1.4, false);
		CHECK(C.St.LastGroundedPoint.Equals(Landed.Feet, 0.01));
		// Airborne again just above the new platform: no ring
		CHECK(!C.Eval(C.Subject(-170, 0, true, false), {}, 1.5).Above.IsValid());
	}
	return true;
}

GSTN_DM_TEST(AboveFollowsGravityFlip)
{
	GSTN_DM_FOR_EACH_UP
	{
		C.StandOnPlane();
		// 100 cm below the plane along Up: the ring is above
		FGSTN_DepthSubjectState S = C.Subject(-100, 0, true, false);
		FGSTN_DepthResult R = C.Eval(S, {}, 1.0);
		CHECK(R.Above.IsValid());

		// Same place, gravity flipped: the feet are now 100 cm ABOVE the plane (new Up), so no above ring but the ring below
		S.Up = -C.Up;
		R = C.Eval(S, {}, 1.1);
		CHECK(R.bGravityReset);
		CHECK(!R.Above.IsValid());
		CHECK(R.Below.State == EGSTN_DepthMarkerState::NoGround);
		CHECK(R.Below.Point.Equals(C.At(0.0), 0.01));

		// Move to the other side of the plane: with the new Up the ring is above again, pointing along the new Up
		S.Feet = C.At(100.0);
		R = C.Eval(S, {}, 1.2);
		CHECK(R.Above.State == EGSTN_DepthMarkerState::AboveReference);
		CHECK(R.Above.Point.Equals(C.At(0.0), 0.01));
		CHECK(FVector::DotProduct(R.Above.Point - S.Feet, S.Up) > 0.0);
	}
	return true;
}

// ---- Approach marker (marker B) ----

namespace GSTNDepthMarkerCoreTest
{
	/** A walkable top whose sampled point lies Along over the ground plane and Lateral to the side */
	static FGSTN_DepthPlaneCandidate Plane(const FCtx& C, double Along, double Lateral, int64 Key, bool bExplicit = false, int32 Priority = 0)
	{
		FGSTN_DepthPlaneCandidate P;
		P.Point = C.At(Along, Lateral);
		P.Normal = C.Up;
		P.SurfaceKey = Key;
		P.bExplicit = bExplicit;
		P.Priority = Priority;
		return P;
	}

	/** The primary (below) target: ground key 1 at Along 0 under a character at Along 300 (every approach test stands there) */
	static FGSTN_DepthTarget Primary(FCtx& C, const FGSTN_DepthSubjectState& S)
	{
		return C.Eval(S, { C.Cand(S, 300.0, 1) }, 0.0).Below;
	}

	static bool Approach(FCtx& C, const FGSTN_DepthSubjectState& S, const FGSTN_DepthTarget& Prim, const TArray<FGSTN_DepthPlaneCandidate>& Probed,
		const TArray<FGSTN_DepthPlaneCandidate>& Explicit, double Time, FGSTN_DepthTarget& Out)
	{
		return FGSTN_DepthMarkerMath::EvaluateApproach(C.St, S, C.P, Prim, Probed, Explicit, Time, Out);
	}
}

GSTN_DM_TEST(ApproachMarkerOnHigherPlatformPlane)
{
	GSTN_DM_FOR_EACH_UP
	{
		FGSTN_DepthSubjectState S = C.Subject(300, 0, true, false);
		S.Velocity = C.Side * 300.0;
		const FGSTN_DepthTarget Prim = Primary(C, S);
		CHECK(Prim.Kind == EGSTN_DepthTargetKind::Surface && Prim.SurfaceKey == 1);

		// A platform 150 over the ground (150 below the feet), its nearest sampled point 250 cm to the side
		FGSTN_DepthTarget A;
		CHECK(Approach(C, S, Prim, { Plane(C, 150, 250, 2) }, {}, 0.0, A));
		CHECK(A.Kind == EGSTN_DepthTargetKind::ApproachPlane && A.State == EGSTN_DepthMarkerState::Approach && A.SurfaceKey == 2);
		// Marker B hangs in the air: the character's column projected onto that plane, beyond the platform's edge
		CHECK(A.Point.Equals(C.At(150, 0), 0.01));
		CHECK(FMath::IsNearlyEqual(A.Height, 150.f, 0.01f));
		// Both markers at once: the primary one is untouched
		CHECK(Prim.Point.Equals(C.At(0, 0), 0.01));
	}
	return true;
}

GSTN_DM_TEST(NoApproachWhenNotMovingToward)
{
	GSTN_DM_FOR_EACH_UP
	{
		FGSTN_DepthSubjectState S = C.Subject(300, 0, true, false);
		const FGSTN_DepthTarget Prim = Primary(C, S);
		FGSTN_DepthTarget A;

		// Standing still, 300 cm away: neither approaching nor close
		CHECK(!Approach(C, S, Prim, { Plane(C, 150, 300, 2) }, {}, 0.0, A));
		// Moving away
		S.Velocity = -C.Side * 300.0;
		CHECK(!Approach(C, S, Prim, { Plane(C, 150, 300, 2) }, {}, 0.1, A));
		// Moving towards but too far
		S.Velocity = C.Side * 300.0;
		CHECK(!Approach(C, S, Prim, { Plane(C, 150, 700, 2) }, {}, 0.2, A));
		// Moving towards a plane that is higher than the feet by more than the near-above tolerance
		CHECK(!Approach(C, S, Prim, { Plane(C, 400, 250, 2) }, {}, 0.3, A));
		// An underside (a ceiling seen from below) or a wall is no platform, however near
		S.Velocity = C.Side * 300.0;
		FGSTN_DepthPlaneCandidate Ceiling = Plane(C, 150, 100, 2);
		Ceiling.Normal = -C.Up;
		CHECK(!Approach(C, S, Prim, { Ceiling }, {}, 0.35, A));
		FGSTN_DepthPlaneCandidate Wall = Plane(C, 150, 100, 2);
		Wall.Normal = C.Side;
		CHECK(!Approach(C, S, Prim, { Wall }, {}, 0.36, A));
		// Standing still but within the min distance: shown
		S.Velocity = FVector::ZeroVector;
		CHECK(Approach(C, S, Prim, { Plane(C, 150, 100, 2) }, {}, 0.4, A));
		// Grounded: never
		S.bAirborne = false;
		S.bGrounded = true;
		CHECK(!Approach(C, S, Prim, { Plane(C, 150, 100, 2) }, {}, 0.5, A));
	}
	return true;
}

GSTN_DM_TEST(ApproachUsesVolumePlaneWhenPresent)
{
	GSTN_DM_FOR_EACH_UP
	{
		FGSTN_DepthSubjectState S = C.Subject(300, 0, true, false);
		S.Velocity = C.Side * 300.0;
		const FGSTN_DepthTarget Prim = Primary(C, S);
		FGSTN_DepthTarget A;

		// Probing alone finds the platform
		CHECK(Approach(C, S, Prim, { Plane(C, 150, 100, 2) }, {}, 0.0, A) && A.SurfaceKey == 2);
		C.St.ClearApproach();

		// A designer plane that overlaps the character wins over the probed platform, whatever the distances
		CHECK(Approach(C, S, Prim, { Plane(C, 150, 100, 2) }, { Plane(C, 100, 0, 9, true) }, 1.0, A));
		CHECK(A.SurfaceKey == 9 && A.Point.Equals(C.At(100, 0), 0.01) && FMath::IsNearlyEqual(A.EdgeAlpha, 1.f));
		C.St.ClearApproach();

		// Among designer planes the priority decides
		CHECK(Approach(C, S, Prim, {}, { Plane(C, 100, 0, 9, true, 1), Plane(C, 200, 0, 10, true, 5) }, 2.0, A));
		CHECK(A.SurfaceKey == 10 && A.Point.Equals(C.At(200, 0), 0.01));

		// It is the fallback too: no probe found anything, the declared plane still shows
		C.St.ClearApproach();
		CHECK(Approach(C, S, Prim, {}, { Plane(C, 100, 0, 9, true) }, 3.0, A) && A.SurfaceKey == 9);
	}
	return true;
}

GSTN_DM_TEST(ApproachHysteresisNoFlicker)
{
	GSTN_DM_FOR_EACH_UP
	{
		// One platform at a fixed place; the character moves, so the edge distance is the character's lateral position
		const double PlatformLateral = 1000.0;
		FGSTN_DepthSubjectState S = C.Subject(300, PlatformLateral - 480.0, true, false);
		S.Velocity = C.Side * 300.0;
		const FGSTN_DepthTarget Prim = Primary(C, S);
		const FGSTN_DepthPlaneCandidate Platform = Plane(C, 150, PlatformLateral, 2);
		FGSTN_DepthTarget A;
		CHECK(Approach(C, S, Prim, { Platform }, {}, 0.0, A));

		// Edge distance jittering around the show distance (500): it never drops out once shown
		int32 Drops = 0;
		for (int32 Frame = 1; Frame <= 20; ++Frame)
		{
			S.Feet = C.At(300, PlatformLateral - ((Frame % 2) ? 510.0 : 490.0));
			if (!Approach(C, S, Prim, { Platform }, {}, Frame * 0.016, A) || A.SurfaceKey != 2)
			{
				++Drops;
			}
		}
		CHECK(Drops == 0);

		// Between the show and hide distances, no longer moving towards it: still shown (hysteresis)
		S.Velocity = FVector::ZeroVector;
		S.Feet = C.At(300, PlatformLateral - 540.0);
		CHECK(Approach(C, S, Prim, { Platform }, {}, 1.0, A));

		// Beyond the hide distance: gone at once, even though a probe still sees the platform
		S.Feet = C.At(300, PlatformLateral - 650.0);
		CHECK(!Approach(C, S, Prim, { Platform }, {}, 1.1, A));
		CHECK(!C.St.bApproachShown);

		// Two platforms almost equally near: the shown one is not swapped back and forth while the nearest alternates
		C.St.ClearApproach();
		S.Velocity = FVector::ZeroVector;
		int64 First = 0;
		int32 Swaps = 0;
		for (int32 Frame = 0; Frame < 12; ++Frame)
		{
			S.Feet = C.At(300, (Frame % 2) ? 5.0 : -5.0);
			const TArray<FGSTN_DepthPlaneCandidate> Both = { Plane(C, 150, 100, 2), Plane(C, 150, -100, 3) };
			if (Approach(C, S, Prim, Both, {}, 2.0 + Frame * 0.016, A))
			{
				if (!First)
				{
					First = A.SurfaceKey;
				}
				Swaps += A.SurfaceKey != First;
			}
		}
		CHECK(First != 0 && Swaps == 0);

		// Probes are staggered: a frame without any candidate holds the plane, a late one does not
		FGSTN_DepthSubjectState T = C.Subject(300, 0, true, false);
		C.St.ClearApproach();
		CHECK(Approach(C, T, Prim, { Plane(C, 150, 100, 2) }, {}, 10.0, A));
		CHECK(Approach(C, T, Prim, {}, {}, 10.1, A) && A.SurfaceKey == 2);
		CHECK(!Approach(C, T, Prim, {}, {}, 10.5, A));
	}
	return true;
}

GSTN_DM_TEST(ApproachNeverDuplicatesPrimaryPlane)
{
	GSTN_DM_FOR_EACH_UP
	{
		FGSTN_DepthSubjectState S = C.Subject(300, 0, true, false);
		S.Velocity = C.Side * 300.0;
		const FGSTN_DepthTarget Prim = Primary(C, S);
		FGSTN_DepthTarget A;

		// The primary's own surface, seen from another probe point
		CHECK(!Approach(C, S, Prim, { Plane(C, 0, 200, 1) }, {}, 0.0, A));
		// Another component coplanar with it
		CHECK(!Approach(C, S, Prim, { Plane(C, 1, 200, 7) }, {}, 0.1, A));
		// A designer plane on the primary's surface
		CHECK(!Approach(C, S, Prim, {}, { Plane(C, 0, 0, 1, true) }, 0.2, A));
		// A designer plane coplanar with the primary
		CHECK(!Approach(C, S, Prim, {}, { Plane(C, 0, 0, 5, true) }, 0.3, A));
		// A step below the minimum rise adds nothing; a real step does
		CHECK(!Approach(C, S, Prim, { Plane(C, 20, 200, 8) }, {}, 0.4, A));
		CHECK(Approach(C, S, Prim, { Plane(C, 60, 200, 8) }, {}, 0.5, A) && A.SurfaceKey == 8);
		// The result is never the primary's plane
		CHECK(A.SurfaceKey != Prim.SurfaceKey && !FMath::IsNearlyEqual(A.Height, Prim.Height, 1.f));

		// The primary's surface sampled on both sides of the column is rejected on both
		C.St.ClearApproach();
		CHECK(!Approach(C, S, Prim, { Plane(C, 0, 300, 1), Plane(C, 5, -300, 1) }, {}, 0.6, A));
	}
	return true;
}

GSTN_DM_TEST(ApproachProbePatternFollowsUp)
{
	GSTN_DM_FOR_EACH_UP
	{
		FGSTN_DepthSubjectState S = C.Subject(300, 0, true, false);
		const int32 Ring = C.P.ApproachRingProbes;
		CHECK(FGSTN_DepthMarkerMath::NumApproachProbeSlots(C.P) == Ring + C.P.ApproachVelocityProbes);

		// A slow character: only the ring
		int32 Valid = 0;
		for (int32 Slot = 0; Slot < FGSTN_DepthMarkerMath::NumApproachProbeSlots(C.P); ++Slot)
		{
			FVector Start;
			FVector End;
			Valid += FGSTN_DepthMarkerMath::ApproachProbeSegment(S, C.P, Slot, Start, End) ? 1 : 0;
		}
		CHECK(Valid == Ring);

		// Moving: the velocity probes lead the way, along the horizontal velocity
		S.Velocity = C.Side * 400.0 + C.Up * 200.0;
		FVector Start;
		FVector End;
		CHECK(FGSTN_DepthMarkerMath::ApproachProbeSegment(S, C.P, Ring + 1, Start, End));
		CHECK((Start - (S.Feet + C.Up * C.P.ApproachNearAboveFeetCm)).Equals(C.Side * C.P.ApproachDistanceCm, 0.01));
		for (int32 Slot = 0; Slot < FGSTN_DepthMarkerMath::NumApproachProbeSlots(C.P); ++Slot)
		{
			CHECK(FGSTN_DepthMarkerMath::ApproachProbeSegment(S, C.P, Slot, Start, End));
			// Every probe starts level with the feet (plus the near-above tolerance) and runs straight along gravity
			CHECK(FMath::Abs(FVector::DotProduct(Start - S.Feet, C.Up) - C.P.ApproachNearAboveFeetCm) < 0.01);
			CHECK((End - Start).GetSafeNormal().Equals(-C.Up, 1.e-4));
		}
		CHECK(!FGSTN_DepthMarkerMath::ApproachProbeSegment(S, C.P, FGSTN_DepthMarkerMath::NumApproachProbeSlots(C.P), Start, End));

		// Designer plane overlap test on an oriented box
		const FTransform Box(FQuat::FindBetweenNormals(FVector::UpVector, C.Up), C.Origin);
		CHECK(FGSTN_DepthMarkerMath::PointInOrientedBox(C.At(10, 20), Box, FVector(100, 100, 50)));
		CHECK(!FGSTN_DepthMarkerMath::PointInOrientedBox(C.At(80, 0), Box, FVector(100, 100, 50)));
	}
	return true;
}

#undef CHECK

#endif // WITH_DEV_AUTOMATION_TESTS
