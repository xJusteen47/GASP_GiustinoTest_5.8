// Charge Hold (aerial charge prototype, owner 2026-11-25): the clock rule, the held-since-start rule, the damage scale and the direction lock of
// FGSTN_TimelineTrack_ChargeHold, pure.

#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputSample.h"
#include "Tests/GSTN_CombatTestUtils.h"
#include "Timeline/GSTN_ChargeHold.h"
#include "Timeline/GSTN_StartupSteer.h"
#include "Timeline/GSTN_TimelineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNChargeHoldTests
{
	constexpr int32 HoldFrame = 5;
	constexpr int32 Button = 2;

	/** An aerial: 30 frames, one hitbox [10, 13), hold on frame 5 for at most 60 ticks */
	FGSTN_CombatTimelineData MakeAerial(float FullScale = 1.f, bool bLock = true)
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 30;
		Data.FirstActionableFrame = 26;
		Data.MaxSteerDegrees = 15.f;
		Data.Steer.Source = EGSTN_SteerSource::MoveStick;
		FGSTN_TimelineTrack_Hitbox Hitbox;
		Hitbox.StartFrame = 10;
		Hitbox.EndFrame = 13;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Hitbox));
		return UGSTN_TimelineUtils::SetChargeHoldTrack(Data, HoldFrame, FGameplayTag(), TEXT("Combat.Attack"), 60, FullScale, bLock);
	}

	/** A history with Button pressed on PressFrame and released on ReleaseFrame (INDEX_NONE = still held), sampled up to LastFrame */
	FGSTN_InputHistory MakeHistory(int32 PressFrame, int32 ReleaseFrame, int32 LastFrame)
	{
		FGSTN_InputHistory History;
		for (int32 Frame = 0; Frame <= LastFrame; ++Frame)
		{
			FGSTN_InputSample Sample;
			Sample.Frame = Frame;
			const bool bDown = Frame >= PressFrame && (ReleaseFrame == INDEX_NONE || Frame < ReleaseFrame);
			Sample.Held = bDown ? (1u << Button) : 0u;
			Sample.Pressed = Frame == PressFrame ? (1u << Button) : 0u;
			Sample.Released = Frame == ReleaseFrame ? (1u << Button) : 0u;
			History.Append(Sample);
		}
		return History;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ChargeHoldClockTest, "GSTNCombat.Core.ChargeHold.Clock", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ChargeHoldClockTest::RunTest(const FString& Parameters)
{
	using namespace GSTNChargeHoldTests;
	const FGSTN_CombatTimelineData Data = MakeAerial();
	int32 Index = INDEX_NONE;
	const FGSTN_TimelineTrack_ChargeHold* Track = FGSTN_ChargeHold::FindTrack(Data, &Index);
	if (!TestNotNull(TEXT("the authoring helper adds the track"), Track))
	{
		return false;
	}
	TestEqual(TEXT("after the hitbox"), Index, 1);
	TestEqual(TEXT("a second call replaces it"), UGSTN_TimelineUtils::SetChargeHoldTrack(Data, 3, FGameplayTag(), NAME_None).Tracks.Num(), 2);
	TestEqual(TEXT("and it can be removed"), UGSTN_TimelineUtils::RemoveChargeHoldTrack(Data).Tracks.Num(), 1);
	TestNull(TEXT("a cap of 0 turns it off"), FGSTN_ChargeHold::FindTrack(UGSTN_TimelineUtils::SetChargeHoldTrack(Data, HoldFrame, FGameplayTag(), NAME_None, 0)));

	// The timeline reached the hold frame on tick 5; ticks 6.. stand on it while held
	TestFalse(TEXT("the tick that evaluates the hold frame plays it"), FGSTN_ChargeHold::ShouldStall(*Track, HoldFrame - 1, HoldFrame, true));
	TestTrue(TEXT("first stall"), FGSTN_ChargeHold::ShouldStall(*Track, HoldFrame, HoldFrame + 1, true));
	TestTrue(TEXT("last stall: the cap"), FGSTN_ChargeHold::ShouldStall(*Track, HoldFrame, HoldFrame + 60, true));
	TestFalse(TEXT("past the cap the move goes on by itself"), FGSTN_ChargeHold::ShouldStall(*Track, HoldFrame, HoldFrame + 61, true));
	TestFalse(TEXT("released: the startup plays on"), FGSTN_ChargeHold::ShouldStall(*Track, HoldFrame, HoldFrame + 3, false));
	TestFalse(TEXT("other frames never stall"), FGSTN_ChargeHold::ShouldStall(*Track, HoldFrame + 1, HoldFrame + 3, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ChargeHoldHeldTest, "GSTNCombat.Core.ChargeHold.HeldSinceStart", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ChargeHoldHeldTest::RunTest(const FString& Parameters)
{
	using namespace GSTNChargeHoldTests;
	constexpr int32 Start = 20;
	TestTrue(TEXT("pressed on the start, still held"), FGSTN_ChargeHold::IsHeldSinceStart(MakeHistory(Start, INDEX_NONE, Start + 8), Button, Start));
	TestTrue(TEXT("a buffered press counts"), FGSTN_ChargeHold::IsHeldSinceStart(MakeHistory(Start - 4, INDEX_NONE, Start + 8), Button, Start));
	TestFalse(TEXT("released"), FGSTN_ChargeHold::IsHeldSinceStart(MakeHistory(Start, Start + 3, Start + 8), Button, Start));
	// A tap, then a new press before the hold frame: not the same hold
	FGSTN_InputHistory Again = MakeHistory(Start, Start + 2, Start + 3);
	for (int32 Frame = Start + 4; Frame <= Start + 8; ++Frame)
	{
		FGSTN_InputSample Sample;
		Sample.Frame = Frame;
		Sample.Held = 1u << Button;
		Sample.Pressed = Frame == Start + 4 ? (1u << Button) : 0u;
		Again.Append(Sample);
	}
	TestFalse(TEXT("pressed again after the start"), FGSTN_ChargeHold::IsHeldSinceStart(Again, Button, Start));
	TestFalse(TEXT("no button"), FGSTN_ChargeHold::IsHeldSinceStart(MakeHistory(Start, INDEX_NONE, Start + 8), INDEX_NONE, Start));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ChargeHoldDamageTest, "GSTNCombat.Core.ChargeHold.DamageScale", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ChargeHoldDamageTest::RunTest(const FString& Parameters)
{
	using namespace GSTNChargeHoldTests;
	TestEqual(TEXT("off by default (1C): a pure pose hold"), FGSTN_ChargeHold::GetDamageScale(MakeAerial(), 60), 1.f);
	const FGSTN_CombatTimelineData Charged = MakeAerial(1.4f);
	TestEqual(TEXT("a tap"), FGSTN_ChargeHold::GetDamageScale(Charged, 0), 1.f);
	TestEqual(TEXT("half"), FGSTN_ChargeHold::GetDamageScale(Charged, 30), 1.2f, 1.e-4f);
	TestEqual(TEXT("full, Smash Ultimate's 1.4"), FGSTN_ChargeHold::GetDamageScale(Charged, 60), 1.4f, 1.e-4f);
	TestEqual(TEXT("never past full"), FGSTN_ChargeHold::GetDamageScale(Charged, 255), 1.4f, 1.e-4f);
	FGSTN_CombatTimelineData Plain;
	TestEqual(TEXT("no track"), FGSTN_ChargeHold::GetDamageScale(Plain, 40), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ChargeHoldDirectionTest, "GSTNCombat.Core.ChargeHold.Direction", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ChargeHoldDirectionTest::RunTest(const FString& Parameters)
{
	using namespace GSTNChargeHoldTests;
	FGSTN_CombatTimelineData Locked = MakeAerial();
	Locked.Reverse.Frames = 8;
	TestEqual(TEXT("2A: the reverse window ends at the hold frame"), FGSTN_StartupSteer::GetReverseEndFrame(Locked), HoldFrame);
	FGSTN_CombatTimelineData Free = MakeAerial(1.f, false);
	Free.Reverse.Frames = 8;
	TestEqual(TEXT("2B: its own window"), FGSTN_StartupSteer::GetReverseEndFrame(Free), 9);

	// While the move stands on the hold frame the yaw holds whatever the stick says; after the release the steering cone is back (the
	// angle tolerance at the confirm)
	FGSTN_SteerFrameInput In;
	In.StartYawDeg = 0.f;
	In.CurrentYawDeg = 2.f;
	In.Stick = FVector2D(1.0, 0.0);
	In.bSourceActive = true;
	float Yaw = 0.f;
	bool bPushed = false;
	In.TimelineFrame = HoldFrame;
	TestTrue(TEXT("held on the hold frame"), FGSTN_StartupSteer::ResolveFrame(Locked, In, Yaw, bPushed) == EGSTN_SteerPhase::Before);
	TestEqual(TEXT("the yaw does not move"), Yaw, 2.f, 1.e-3f);
	In.TimelineFrame = HoldFrame + 1;
	TestTrue(TEXT("steers after the release"), FGSTN_StartupSteer::ResolveFrame(Locked, In, Yaw, bPushed) == EGSTN_SteerPhase::Steering);
	TestEqual(TEXT("inside the move's cone"), Yaw, 15.f, 1.e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ChargeHoldValidateTest, "GSTNCombat.Core.ChargeHold.Validate", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ChargeHoldValidateTest::RunTest(const FString& Parameters)
{
	using namespace GSTNChargeHoldTests;
	TArray<FString> Errors;
	FGSTN_ChargeHold::Validate(MakeAerial(), TEXT("ok"), Errors);
	TestEqual(TEXT("a hold in the startup with a layout function is fine"), Errors.Num(), 0);
	Errors.Reset();
	FGSTN_ChargeHold::Validate(UGSTN_TimelineUtils::SetChargeHoldTrack(MakeAerial(), 11, FGameplayTag(), NAME_None), TEXT("late"), Errors);
	TestEqual(TEXT("past the first active frame and no button"), Errors.Num(), 2);
	return true;
}

#endif
