// Automation tests of the spin and drill plugin: the spin's data and math (plan D4).

#include "Data/GSTN_SpinDrillTracks.h"
#include "Data/GSTN_SpinTimelines.h"
#include "Jump/GSTN_JumpTypes.h"
#include "Math/GSTN_DrillMath.h"
#include "Misc/AutomationTest.h"
#include "Timeline/GSTN_TimelineValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNSpinDrillSpinTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	int32 CountErrors(const FGSTN_CombatTimelineData& Data, FString& OutFirst)
	{
		TArray<FGSTN_TimelineMessage> Messages;
		FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidation::MakeContext(Data, 60.f), Messages);
		int32 Errors = 0;
		for (const FGSTN_TimelineMessage& Message : Messages)
		{
			if (Message.bError)
			{
				OutFirst = OutFirst.IsEmpty() ? Message.Text : OutFirst;
				++Errors;
			}
		}
		return Errors;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SpinDrillSpinTimelinesTest, "GSTNSpinDrill.Core.Spin.DefaultTimelines", GSTNSpinDrillSpinTest::Flags)
bool FGSTN_SpinDrillSpinTimelinesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNSpinDrillSpinTest;

	for (const bool bAir : { false, true })
	{
		for (const EGSTN_SpinSense Sense : { EGSTN_SpinSense::Clockwise, EGSTN_SpinSense::CounterClockwise })
		{
			const FGSTN_SpinTimelineParams Params = bAir ? FGSTN_SpinTimelines::AirParams() : FGSTN_SpinTimelines::GroundParams();
			const FGSTN_CombatTimelineData Data = FGSTN_SpinTimelines::Make(Params, Sense, bAir);
			const FString Label = FString::Printf(TEXT("%s %s"), bAir ? TEXT("air") : TEXT("ground"), Sense == EGSTN_SpinSense::Clockwise ? TEXT("cw") : TEXT("ccw"));

			const FGSTN_SpinTrack* Spin = FGSTN_SpinTrack::Find(Data);
			if (!TestNotNull(*FString::Printf(TEXT("%s: has a spin track"), *Label), Spin))
			{
				continue;
			}
			TestEqual(*FString::Printf(TEXT("%s: the sense"), *Label), static_cast<int32>(Spin->Sense), static_cast<int32>(Sense));
			TestEqual(*FString::Printf(TEXT("%s: the air flag"), *Label), Spin->bAirSpin, bAir);
			TestTrue(*FString::Printf(TEXT("%s: the track covers the whole spin"), *Label), Spin->StartFrame == 0 && Spin->EndFrame == Data.TotalFrames);
			TestEqual(*FString::Printf(TEXT("%s: the frames of the params"), *Label), Data.TotalFrames, Params.TotalFrames);

			bool bRing = false;
			bool bJumpWindow = false;
			for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Data.Tracks)
			{
				const FGSTN_TimelineTrack* Base = Track.GetPtr<FGSTN_TimelineTrack>();
				bRing |= Base && Base->GetKind() == EGSTN_TimelineTrackKind::Hitbox && Base->StartFrame == Params.RingStartFrame && Base->EndFrame == Params.RingEndFrame;
				bJumpWindow |= Base && Base->GetKind() == EGSTN_TimelineTrackKind::CancelWindow && Base->StartFrame == 0;
				TestFalse(*FString::Printf(TEXT("%s: no tags track (no movement lock)"), *Label), Base && Base->GetKind() == EGSTN_TimelineTrackKind::Tags);
			}
			TestTrue(*FString::Printf(TEXT("%s: the hitbox ring"), *Label), bRing);
			TestTrue(*FString::Printf(TEXT("%s: the jump cancel window"), *Label), bJumpWindow);

			FString First;
			const int32 Errors = CountErrors(Data, First);
			TestEqual(*FString::Printf(TEXT("%s: the timeline validates (%s)"), *Label, *First), Errors, 0);
		}
	}

	FGSTN_CombatTimelineData Plain;
	Plain.TotalFrames = 10;
	TestNull(TEXT("a timeline without a spin track has none"), FGSTN_SpinTrack::Find(Plain));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SpinDrillHopTest, "GSTNSpinDrill.Core.Spin.HopAndRefresh", GSTNSpinDrillSpinTest::Flags)
bool FGSTN_SpinDrillHopTest::RunTest(const FString& Parameters)
{
	constexpr double Tolerance = 1e-3;

	// The hop under ordinary gravity: falling at 800 becomes rising at the hop speed, the run is kept
	const FVector Falling(300.0, -100.0, -800.0);
	const FVector Cancelled = UGSTN_DrillMath::ComputeHopVelocity(Falling, FVector::UpVector, 450.f, true);
	TestTrue(TEXT("cancel fall: the run is kept"), FVector(Cancelled.X, Cancelled.Y, 0.0).Equals(FVector(300.0, -100.0, 0.0), Tolerance));
	TestEqual(TEXT("cancel fall: rising at the hop speed"), Cancelled.Z, 450.0, Tolerance);
	const FVector Added = UGSTN_DrillMath::ComputeHopVelocity(Falling, FVector::UpVector, 450.f, false);
	TestEqual(TEXT("additive: the fall only slows"), Added.Z, -350.0, Tolerance);
	const FVector Rising(0.0, 0.0, 900.0);
	TestEqual(TEXT("cancel fall never slows a faster rise"), UGSTN_DrillMath::ComputeHopVelocity(Rising, FVector::UpVector, 450.f, true).Z, 900.0, Tolerance);

	// Any gravity: up along +X (a wall-walking gravity), the hop happens along it and the rest is kept
	const FVector WallUp(1.0, 0.0, 0.0);
	const FVector Sideways = UGSTN_DrillMath::ComputeHopVelocity(FVector(-200.0, 50.0, 70.0), WallUp, 450.f, true);
	TestTrue(TEXT("tilted gravity: hop along its up"), Sideways.Equals(FVector(450.0, 50.0, 70.0), Tolerance));
	TestTrue(TEXT("a zero up changes nothing"), UGSTN_DrillMath::ComputeHopVelocity(Falling, FVector::ZeroVector, 450.f, true).Equals(Falling, Tolerance));

	// Refresh from the jump state: grounded refreshes every frame, a launch only on its first frame
	const uint8 None = static_cast<uint8>(EGSTN_JumpPhase::None);
	const uint8 Rising8 = static_cast<uint8>(EGSTN_JumpPhase::Rising);
	const uint8 Falling8 = static_cast<uint8>(EGSTN_JumpPhase::Falling);
	const uint8 Launched = static_cast<uint8>(EGSTN_JumpPhase::Launched);
	const int32 Land = static_cast<int32>(EGSTN_AirSpinRefresh::Land);
	const int32 Trampoline = static_cast<int32>(EGSTN_AirSpinRefresh::Trampoline);
	TestEqual(TEXT("grounded: land"), UGSTN_DrillMath::RefreshEventsFromJumpPhase(Falling8, None), Land);
	TestEqual(TEXT("still grounded: land again"), UGSTN_DrillMath::RefreshEventsFromJumpPhase(None, None), Land);
	TestEqual(TEXT("rising from a jump: nothing"), UGSTN_DrillMath::RefreshEventsFromJumpPhase(None, Rising8), 0);
	TestEqual(TEXT("falling: nothing"), UGSTN_DrillMath::RefreshEventsFromJumpPhase(Rising8, Falling8), 0);
	TestEqual(TEXT("launched now: trampoline"), UGSTN_DrillMath::RefreshEventsFromJumpPhase(Falling8, Launched), Trampoline);
	TestEqual(TEXT("still launched: nothing (an air spin used on the way up stays spent)"), UGSTN_DrillMath::RefreshEventsFromJumpPhase(Launched, Launched), 0);

	// Getting hit never refreshes by default (owner 2026-11-01), only when a tuning asks for it
	const int32 DefaultRefresh = FGSTN_AirSpinTuning().RefreshOn;
	TestFalse(TEXT("the default tuning does not refresh on a hit"), UGSTN_DrillMath::ShouldRefreshAirSpin(DefaultRefresh, static_cast<int32>(EGSTN_AirSpinRefresh::Hit)));
	TestTrue(TEXT("the default tuning refreshes on landing"), UGSTN_DrillMath::ShouldRefreshAirSpin(DefaultRefresh, Land));
	TestTrue(TEXT("and on a trampoline"), UGSTN_DrillMath::ShouldRefreshAirSpin(DefaultRefresh, Trampoline));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
