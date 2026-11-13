// Automation tests of the hitstop clock and the freeze layered move (combat-m4-hits 8, M4 H7). No world: plain data and bit archives.

#include "GSTN_CombatNPTags.h"
#include "Input/GSTN_InputMatcher.h"
#include "Misc/AutomationTest.h"
#include "Movement/GSTN_LayeredMove_Freeze.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Simulation/GSTN_CombatActionTypes.h"
#include "Simulation/GSTN_CombatSimTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitstopPausedFramesCountTest, "GSTNCombat.NP.Hits.Hitstop.PausedFramesCount", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitstopPausedFramesCountTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatSyncCore Core;
	Core.HitstopStartFrame = 10;
	Core.HitstopEndFrame = 18;

	int32 Counted = 0;
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Counted += Core.AdvanceHitstopClock(Frame) ? 1 : 0;
		if (Frame == 9)
		{
			TestEqual(TEXT("nothing paused before the window"), Core.PausedFramesTotal, 0);
		}
		if (Frame == 17)
		{
			TestEqual(TEXT("every window frame counted once"), Core.PausedFramesTotal, 8);
		}
	}
	TestEqual(TEXT("the window length and no more"), Core.PausedFramesTotal, 8);
	TestEqual(TEXT("the frames that reported hitstop"), Counted, 8);

	// The combat clock stands still through the window and runs on at the same rate after it
	Core.PausedFramesTotal = 0;
	int32 ClockBefore = 0;
	int32 ClockInside = 0;
	int32 ClockAfter = 0;
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Core.AdvanceHitstopClock(Frame);
		const int32 Clock = FGSTN_CombatClock::FromFrame(Frame, Core.PausedFramesTotal);
		ClockBefore = Frame == 10 ? Clock : ClockBefore;
		ClockInside = Frame == 17 ? Clock : ClockInside;
		ClockAfter = Frame == 18 ? Clock : ClockAfter;
	}
	TestEqual(TEXT("the clock stands still inside the window"), ClockInside, ClockBefore);
	TestEqual(TEXT("the clock runs again on the first frame after"), ClockAfter, ClockInside + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitstopBufferAgeTest, "GSTNCombat.NP.Hits.Hitstop.BufferAgeExcludesHitstop", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitstopBufferAgeTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatSyncCore Core;
	Core.HitstopStartFrame = 10;
	Core.HitstopEndFrame = 18;

	// The tick hands the window of the state to the matcher as the paused window: a press made before the window ages by the combat
	// clock, i.e. by the frames outside the window
	FGSTN_MatchParams Params;
	Params.PausedStart = Core.HitstopStartFrame;
	Params.PausedEnd = Core.HitstopEndFrame;

	int32 PressClock = 0;
	for (int32 Frame = 0; Frame <= 25; ++Frame)
	{
		Core.AdvanceHitstopClock(Frame);
		if (Frame == 8)
		{
			PressClock = FGSTN_CombatClock::FromFrame(Frame, Core.PausedFramesTotal);
		}
	}
	Params.Now = 25;
	const int32 NowClock = FGSTN_CombatClock::FromFrame(25, Core.PausedFramesTotal);
	TestEqual(TEXT("age of a press before the window excludes its frames"), FGSTN_InputMatcher::GetAge(8, Params), 17 - 8);
	TestEqual(TEXT("it is the combat clock difference"), FGSTN_InputMatcher::GetAge(8, Params), NowClock - PressClock);

	// A press during the window is recorded and ages only from its end
	Params.Now = 17;
	TestEqual(TEXT("a press made in the window has not aged inside it"), FGSTN_InputMatcher::GetAge(12, Params), 0);
	Params.Now = 22;
	TestEqual(TEXT("it ages by the frames after the window"), FGSTN_InputMatcher::GetAge(12, Params), 22 - 18);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_FreezeRestoresVelocityTest, "GSTNCombat.NP.Hits.Hitstop.FreezeRestoresVelocity", GSTN_NP_TEST_FLAGS)
bool FGSTN_FreezeRestoresVelocityTest::RunTest(const FString& Parameters)
{
	const FVector Incoming(412.5, -80.0, 621.0);
	FGSTN_LayeredMove_Freeze Freeze;
	Freeze.Setup(Incoming, 133.f, false);

	TestEqual(TEXT("overrides everything"), Freeze.MixMode, EMoveMixMode::OverrideAll);
	TestEqual(TEXT("duration"), Freeze.DurationMs, 133.f);
	TestEqual(TEXT("finishes by setting a velocity"), Freeze.FinishVelocitySettings.FinishVelocityMode, ELayeredMoveFinishVelocityMode::SetVelocity);
	TestTrue(TEXT("the incoming velocity is what it sets"), Freeze.FinishVelocitySettings.SetVelocity.Equals(Incoming));
	TestTrue(TEXT("it carries the frozen tag"), Freeze.HasGameplayTag(GSTNCombatNPTags::Mover_Frozen.GetTag(), true));
	TestTrue(TEXT("and matches its parent when not exact"), Freeze.HasGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("Combat.Mover")), false));

	// It survives the wire and a clone (a rollback copies the move group)
	FBitWriter Writer(1024, true);
	Freeze.NetSerialize(Writer);
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_LayeredMove_Freeze Loaded;
	Loaded.NetSerialize(Reader);
	TestTrue(TEXT("incoming velocity after the wire"), Loaded.IncomingVelocity.Equals(Incoming, 0.01));
	TestFalse(TEXT("flag after the wire"), Loaded.bZeroVelocityOnFinish);
	TestEqual(TEXT("finish mode after the wire"), Loaded.FinishVelocitySettings.FinishVelocityMode, ELayeredMoveFinishVelocityMode::SetVelocity);
	TestTrue(TEXT("finish velocity after the wire"), Loaded.FinishVelocitySettings.SetVelocity.Equals(Incoming, 0.01));

	const TUniquePtr<FLayeredMoveBase> Clone(Freeze.Clone());
	TestTrue(TEXT("the clone is a freeze"), Clone->GetScriptStruct() == FGSTN_LayeredMove_Freeze::StaticStruct());
	TestTrue(TEXT("the clone keeps the finish velocity"), Clone->FinishVelocitySettings.SetVelocity.Equals(Incoming));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_FreezeZeroOnFinishFlagTest, "GSTNCombat.NP.Hits.Hitstop.FreezeZeroOnFinishFlag", GSTN_NP_TEST_FLAGS)
bool FGSTN_FreezeZeroOnFinishFlagTest::RunTest(const FString& Parameters)
{
	const FVector Incoming(300.0, 0.0, -150.0);
	FGSTN_LayeredMove_Freeze Freeze;
	Freeze.Setup(Incoming, 100.f, true);

	TestEqual(TEXT("sets a velocity on finish"), Freeze.FinishVelocitySettings.FinishVelocityMode, ELayeredMoveFinishVelocityMode::SetVelocity);
	TestTrue(TEXT("and it is zero"), Freeze.FinishVelocitySettings.SetVelocity.IsZero());
	TestTrue(TEXT("the incoming velocity is still known"), Freeze.IncomingVelocity.Equals(Incoming));

	FBitWriter Writer(1024, true);
	Freeze.NetSerialize(Writer);
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_LayeredMove_Freeze Loaded;
	Loaded.NetSerialize(Reader);
	TestTrue(TEXT("flag after the wire"), Loaded.bZeroVelocityOnFinish);
	TestTrue(TEXT("zero finish velocity after the wire"), Loaded.FinishVelocitySettings.SetVelocity.IsZero());

	// The held velocity is zero while it lasts, whatever the flag
	FProposedMove Proposed;
	FMoverTickStartData StartData;
	FMoverTimeStep TimeStep;
	Proposed.LinearVelocity = Incoming;
	TestTrue(TEXT("a move is generated"), Freeze.GenerateMove(StartData, TimeStep, nullptr, nullptr, Proposed));
	TestTrue(TEXT("with zero velocity"), Proposed.LinearVelocity.IsZero());
	TestEqual(TEXT("overriding everything"), Proposed.MixMode, EMoveMixMode::OverrideAll);
	return true;
}

#endif
