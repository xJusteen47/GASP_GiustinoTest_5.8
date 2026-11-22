// The victim's half-turn frame (rule-10 review cp5-hit-cue, 2026-11-15): a back hit turns the victim 180 degrees in one simulation tick
// (FGSTN_CombatVictim::ResolveHitFacing), and the fixed tick smoothing drew the frame between the two ticks with the orientation blended, a
// victim side-on for one frame. The Mover fork snaps an orientation change above Mover.Interpolation.OrientationSnapDegrees (60) instead.

#include "Misc/AutomationTest.h"
#include "MoverDataModelTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_FacingSnapInterpolateTest, "GSTNCombat.NP.Hits.Facing.InterpolateSnap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_FacingSnapInterpolateTest::RunTest(const FString& Parameters)
{
	const auto Make = [](float Yaw)
	{
		FMoverDefaultSyncState State;
		State.SetTransforms_WorldSpace(FVector(100.f, 0.f, 0.f), FRotator(0.f, Yaw, 0.f), FVector::ZeroVector, FVector::ZeroVector);
		return State;
	};

	// A back-hit turn: facing away, then facing the attacker on the next tick. Half-way between them the newer facing is drawn
	{
		const FMoverDefaultSyncState From = Make(0.f);
		const FMoverDefaultSyncState To = Make(180.f);
		FMoverDefaultSyncState Out = From;
		Out.Interpolate(From, To, 0.5f);
		TestTrue(TEXT("a 180 degree turn in one tick snaps to the newer facing"),
			Out.GetOrientation_WorldSpace().Quaternion().AngularDistance(To.GetOrientation_WorldSpace().Quaternion()) < KINDA_SMALL_NUMBER);
	}

	// A normal turn (10 degrees in a tick) still blends
	{
		const FMoverDefaultSyncState From = Make(0.f);
		const FMoverDefaultSyncState To = Make(10.f);
		FMoverDefaultSyncState Out = From;
		Out.Interpolate(From, To, 0.5f);
		TestEqual(TEXT("a small turn is blended"), static_cast<float>(Out.GetOrientation_WorldSpace().Yaw), 5.f, 0.01f);
	}

	// Location still blends when the facing snaps (only the orientation is a snap)
	{
		FMoverDefaultSyncState From = Make(0.f);
		FMoverDefaultSyncState To = Make(180.f);
		To.SetTransforms_WorldSpace(FVector(110.f, 0.f, 0.f), FRotator(0.f, 180.f, 0.f), FVector::ZeroVector, FVector::ZeroVector);
		FMoverDefaultSyncState Out = From;
		Out.Interpolate(From, To, 0.5f);
		TestEqual(TEXT("location blended"), static_cast<float>(Out.GetLocation_WorldSpace().X), 105.f, 0.01f);
	}
	return true;
}

#endif
