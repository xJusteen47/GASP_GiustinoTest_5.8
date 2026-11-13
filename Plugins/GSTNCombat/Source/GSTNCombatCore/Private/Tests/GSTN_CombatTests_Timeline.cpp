#include "Tests/GSTN_CombatTestUtils.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelinePlayer.h"
#include "Timeline/GSTN_TimelineUtils.h"
#include "Timeline/GSTN_TimelineValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNTimelineTest
{
	using FTrackInstance = TInstancedStruct<FGSTN_TimelineTrack>;

	template <typename T>
	FTrackInstance MakeTrack(int32 Start, int32 End)
	{
		T Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		return FTrackInstance::Make<T>(Track);
	}

	FTrackInstance MakeSwitch(int32 SwapFrame)
	{
		FGSTN_TimelineTrack_WeaponSwitch Track;
		Track.StartFrame = SwapFrame;
		Track.EndFrame = SwapFrame + 1;
		Track.SwapFrame = SwapFrame;
		return FTrackInstance::Make<FGSTN_TimelineTrack_WeaponSwitch>(Track);
	}

	/**
	 * 30 frames, actionable from 12, a two frame override window and one track of each kind that runs:
	 * tags [0,12), cancel window [8,14), movement [4,10), event at 5, weapon switch at 6, hitbox [3,6)
	 */
	FGSTN_CombatTimelineData MakeData()
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 30;
		Data.FirstActionableFrame = 12;
		Data.OverrideFrames = 2;
		Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_Tags>(0, 12));
		Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_CancelWindow>(8, 14));
		Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_Movement>(4, 10));
		Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_Event>(5, 6));
		Data.Tracks.Add(MakeSwitch(6));
		FGSTN_TimelineTrack_Hitbox Hitbox;
		Hitbox.StartFrame = 3;
		Hitbox.EndFrame = 6;
		Hitbox.Part.GroupName = TEXT("Hands");
		Data.Tracks.Add(FTrackInstance::Make<FGSTN_TimelineTrack_Hitbox>(Hitbox));
		return Data;
	}

	/** A recoil of 8 frames with a movement on [2,5) */
	void AddRicochet(FGSTN_CombatTimelineData& Data)
	{
		Data.bHasRicochetResponse = true;
		Data.RicochetResponse.RecoilFrames = 8;
		Data.RicochetResponse.bHasMovement = true;
		Data.RicochetResponse.Movement.StartFrame = 2;
		Data.RicochetResponse.Movement.EndFrame = 5;
		Data.RicochetResponse.CueTag = GSTNCombatButtonTags::Button_Attack.GetTag();
	}

	FGSTN_TimelineEvalParams At(int32 Frame, int32 Previous)
	{
		FGSTN_TimelineEvalParams Params;
		Params.Frame = Frame;
		Params.PreviousFrame = Previous;
		return Params;
	}

	bool HasError(const TArray<FGSTN_TimelineMessage>& Messages)
	{
		return Messages.ContainsByPredicate([](const FGSTN_TimelineMessage& Message) { return Message.bError; });
	}

	bool HasMessage(const TArray<FGSTN_TimelineMessage>& Messages, bool bError, const TCHAR* Fragment)
	{
		return Messages.ContainsByPredicate([&](const FGSTN_TimelineMessage& Message) { return Message.bError == bError && Message.Text.Contains(Fragment); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TimelineWindowsAtFrameTest, "GSTNCombat.Core.Timeline.WindowsAtFrame", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TimelineWindowsAtFrameTest::RunTest(const FString& Parameters)
{
	using namespace GSTNTimelineTest;
	const FGSTN_CombatTimelineData Data = MakeData();
	FGSTN_TimelineOutput Output;

	FGSTN_TimelinePlayer::Evaluate(Data, At(0, INDEX_NONE), Output);
	TestEqual(TEXT("tag window open at 0"), Output.TagWindows.Num(), 1);
	TestEqual(TEXT("no cancel window at 0"), Output.CancelWindows.Num(), 0);
	TestTrue(TEXT("override window open at 0"), Output.bInOverrideWindow);
	TestTrue(TEXT("busy before the first actionable frame"), Output.bBusy);
	TestEqual(TEXT("commit frame is the end of the override window"), Output.DeferredCommitFrame, 2);

	FGSTN_TimelinePlayer::Evaluate(Data, At(2, 1), Output);
	TestFalse(TEXT("override window closed at 2 (half open)"), Output.bInOverrideWindow);

	FGSTN_TimelinePlayer::Evaluate(Data, At(7, 6), Output);
	TestEqual(TEXT("movement running at 7"), Output.ActiveMovement.Num(), 1);
	TestEqual(TEXT("no cancel window at 7"), Output.CancelWindows.Num(), 0);

	FGSTN_TimelinePlayer::Evaluate(Data, At(8, 7), Output);
	TestEqual(TEXT("cancel window opens at its start frame"), Output.CancelWindows.Num(), 1);

	FGSTN_TimelinePlayer::Evaluate(Data, At(12, 11), Output);
	TestEqual(TEXT("tag window closed at 12"), Output.TagWindows.Num(), 0);
	TestEqual(TEXT("cancel window still open at 12"), Output.CancelWindows.Num(), 1);
	TestFalse(TEXT("actionable at 12"), Output.bBusy);
	TestEqual(TEXT("movement over at 12"), Output.ActiveMovement.Num(), 0);

	FGSTN_TimelinePlayer::Evaluate(Data, At(14, 13), Output);
	TestEqual(TEXT("cancel window closed at its end frame"), Output.CancelWindows.Num(), 0);

	FGSTN_TimelinePlayer::Evaluate(Data, At(29, 28), Output);
	TestFalse(TEXT("not finished at the last frame"), Output.bFinished);
	FGSTN_TimelinePlayer::Evaluate(Data, At(30, 29), Output);
	TestTrue(TEXT("finished at TotalFrames"), Output.bFinished);

	// The paused-frame offset moves the evaluated frame back: hitstop does not advance the timeline
	FGSTN_TimelineEvalParams Paused = At(20, 7);
	Paused.PausedFrames = 12;
	FGSTN_TimelinePlayer::Evaluate(Data, Paused, Output);
	TestEqual(TEXT("evaluated frame = frame - paused frames"), Output.Frame, 8);
	TestEqual(TEXT("cancel window follows the paused clock"), Output.CancelWindows.Num(), 1);

	// A default override length applies when the timeline says "tier default"
	FGSTN_CombatTimelineData Tier = Data;
	Tier.OverrideFrames = -1;
	FGSTN_TimelineEvalParams Defaulted = At(0, INDEX_NONE);
	Defaulted.DefaultOverrideFrames = 3;
	FGSTN_TimelinePlayer::Evaluate(Tier, Defaulted, Output);
	TestEqual(TEXT("tier default used"), Output.DeferredCommitFrame, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TimelineEventsOnceTest, "GSTNCombat.Core.Timeline.EventsOnce", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TimelineEventsOnceTest::RunTest(const FString& Parameters)
{
	using namespace GSTNTimelineTest;
	const FGSTN_CombatTimelineData Data = MakeData();
	FGSTN_TimelineOutput Output;

	int32 Events = 0, Swaps = 0, Starts = 0, Ends = 0, Commits = 0;
	int32 Previous = INDEX_NONE;
	for (int32 Frame = 0; Frame <= 35; ++Frame)
	{
		FGSTN_TimelinePlayer::Evaluate(Data, At(Frame, Previous), Output);
		Events += Output.Events.Num();
		Swaps += Output.WeaponSwitches.Num();
		Starts += Output.MovementStarts.Num();
		Ends += Output.MovementEnds.Num();
		Commits += Output.bCommitThisFrame ? 1 : 0;
		if (Output.Events.Num() > 0)
		{
			TestEqual(TEXT("event fires on its frame"), Frame, 5);
		}
		Previous = Output.Frame;
	}
	TestEqual(TEXT("event fired once"), Events, 1);
	TestEqual(TEXT("weapon swap fired once"), Swaps, 1);
	TestEqual(TEXT("movement start once"), Starts, 1);
	TestEqual(TEXT("movement end once"), Ends, 1);
	TestEqual(TEXT("commit once"), Commits, 1);

	// Hitstop: the frame counter runs on but the timeline frame is held; keys on the held frame fire once, not every frame
	Events = 0;
	Previous = INDEX_NONE;
	for (int32 Frame = 0; Frame <= 14; ++Frame)
	{
		FGSTN_TimelineEvalParams Params = At(Frame, Previous);
		Params.PausedFrames = FMath::Clamp(Frame - 5, 0, 6);
		FGSTN_TimelinePlayer::Evaluate(Data, Params, Output);
		Events += Output.Events.Num();
		Previous = Output.Frame;
	}
	TestEqual(TEXT("event fired once across a pause on its frame"), Events, 1);

	// A skipped frame still delivers the key it jumped over
	FGSTN_TimelinePlayer::Evaluate(Data, At(7, 3), Output);
	TestEqual(TEXT("key inside (previous, current] fires"), Output.Events.Num(), 1);
	FGSTN_TimelinePlayer::Evaluate(Data, At(7, 5), Output);
	TestEqual(TEXT("key at the previous frame does not fire again"), Output.Events.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TimelineNoAllocationTest, "GSTNCombat.Core.Timeline.NoAllocation", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TimelineNoAllocationTest::RunTest(const FString& Parameters)
{
	using namespace GSTNTimelineTest;

	FGSTN_CombatTimelineData Data = MakeData();
	AddRicochet(Data);
	Data.Tail.bHasTail = true;

	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);

	// No malloc counter is available to an Automation test, so the check is capacity and pointer stability of every list:
	// an allocation (or a reallocation) of any of them would change its Max() or its data pointer. Reserve sizes them once
	// and Evaluate must only reset the lengths.
	struct FSnapshot
	{
		int32 Max[7];
		const void* Data[7];
	};
	auto Take = [&Output]()
	{
		FSnapshot Snapshot;
		Snapshot.Max[0] = Output.CancelWindows.Max();    Snapshot.Data[0] = Output.CancelWindows.GetData();
		Snapshot.Max[1] = Output.TagWindows.Max();       Snapshot.Data[1] = Output.TagWindows.GetData();
		Snapshot.Max[2] = Output.ActiveMovement.Max();   Snapshot.Data[2] = Output.ActiveMovement.GetData();
		Snapshot.Max[3] = Output.MovementStarts.Max();   Snapshot.Data[3] = Output.MovementStarts.GetData();
		Snapshot.Max[4] = Output.MovementEnds.Max();     Snapshot.Data[4] = Output.MovementEnds.GetData();
		Snapshot.Max[5] = Output.Events.Max();           Snapshot.Data[5] = Output.Events.GetData();
		Snapshot.Max[6] = Output.WeaponSwitches.Max();   Snapshot.Data[6] = Output.WeaponSwitches.GetData();
		return Snapshot;
	};
	auto Same = [](const FSnapshot& A, const FSnapshot& B)
	{
		for (int32 Index = 0; Index < 7; ++Index)
		{
			if (A.Max[Index] != B.Max[Index] || A.Data[Index] != B.Data[Index])
			{
				return false;
			}
		}
		return true;
	};

	const FSnapshot Before = Take();
	TestTrue(TEXT("lists reserved"), Before.Max[5] >= 1 && Before.Max[2] >= 1);

	bool bStable = true;
	int32 Previous = INDEX_NONE;
	for (int32 Frame = 0; Frame <= 40; ++Frame)
	{
		FGSTN_TimelinePlayer::Evaluate(Data, At(Frame, Previous), Output);
		Previous = Output.Frame;
		bStable &= Same(Before, Take());
	}
	int32 PreviousRecoil = INDEX_NONE;
	for (int32 Frame = 0; Frame <= 10; ++Frame)
	{
		FGSTN_TimelineEvalParams Params = At(20, 19);
		Params.bRicochet = true;
		Params.RecoilFrame = Frame;
		Params.PreviousRecoilFrame = PreviousRecoil;
		FGSTN_TimelinePlayer::Evaluate(Data, Params, Output);
		PreviousRecoil = Frame;
		bStable &= Same(Before, Take());
	}
	TestTrue(TEXT("capacities and data pointers unchanged over 40 frames and a recoil"), bStable);

	// Evaluate alone reserves on first use, then stays put
	FGSTN_TimelineOutput Lazy;
	FGSTN_TimelinePlayer::Evaluate(Data, At(0, INDEX_NONE), Lazy);
	const void* LazyEvents = Lazy.Events.GetData();
	const int32 LazyMax = Lazy.Events.Max();
	for (int32 Frame = 1; Frame <= 40; ++Frame)
	{
		FGSTN_TimelinePlayer::Evaluate(Data, At(Frame, Frame - 1), Lazy);
	}
	TestTrue(TEXT("lazily reserved output is stable after its first evaluation"), Lazy.Events.GetData() == LazyEvents && Lazy.Events.Max() == LazyMax);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TimelineValidationTest, "GSTNCombat.Core.Timeline.Validation", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TimelineValidationTest::RunTest(const FString& Parameters)
{
	using namespace GSTNTimelineTest;
	TArray<FGSTN_TimelineMessage> Messages;
	const FGSTN_TimelineValidationContext NoContext;

	FGSTN_CombatTimelineData Data = MakeData();
	FGSTN_TimelineValidation::Validate(Data, NoContext, Messages);
	TestEqual(TEXT("the sample timeline is clean"), Messages.Num(), 0);

	// First actionable frame past the end
	FGSTN_CombatTimelineData Late = Data;
	Late.FirstActionableFrame = 40;
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Late, NoContext, Messages);
	TestTrue(TEXT("FirstActionableFrame > TotalFrames is an error"), HasMessage(Messages, true, TEXT("FirstActionableFrame")));

	// Deferred commit: an event inside the override window
	FGSTN_CombatTimelineData Early = Data;
	Early.OverrideFrames = 6;
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Early, NoContext, Messages);
	TestTrue(TEXT("event, movement, swap and hitbox before the commit are errors"), HasMessage(Messages, true, TEXT("deferred commit")));
	TestEqual(TEXT("one error per acting track before the commit (hitbox 3, movement 4, event 5, swap 6 is not before 6)"), Messages.FilterByPredicate([](const FGSTN_TimelineMessage& M) { return M.bError; }).Num(), 3);

	// The tier default applies when the timeline leaves OverrideFrames at -1
	FGSTN_CombatTimelineData Tier = Data;
	Tier.OverrideFrames = -1;
	FGSTN_TimelineValidationContext Wide;
	Wide.DefaultOverrideFrames = 5;
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Tier, Wide, Messages);
	TestTrue(TEXT("tier default of 5 puts the hitbox (3) and the movement (4) before the commit"), HasError(Messages));

	// Override window longer than the timeline
	FGSTN_CombatTimelineData Long = Data;
	Long.OverrideFrames = 99;
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Long, NoContext, Messages);
	TestTrue(TEXT("override window longer than the timeline is an error"), HasMessage(Messages, true, TEXT("override window")));

	// Montage length against the frames: only when the length is known
	FGSTN_TimelineValidationContext Montage;
	Montage.TickRate = 60.f;
	Montage.MontageLengthSeconds = 0.5f; // 30 frames
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Data, Montage, Messages);
	TestEqual(TEXT("montage that matches: no message"), Messages.Num(), 0);
	Montage.MontageLengthSeconds = 1.0f; // 60 frames against 30
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Data, Montage, Messages);
	TestTrue(TEXT("montage too long: warning, not error"), HasMessage(Messages, false, TEXT("montage")) && !HasError(Messages));
	FGSTN_CombatTimelineData Fast = Data;
	Fast.MontagePlayRate = 2.f; // 60 frames of montage at twice the speed = 30
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Fast, Montage, Messages);
	TestEqual(TEXT("play rate is taken into account"), Messages.Num(), 0);
	Montage.MontageLengthSeconds = -1.f;
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Data, Montage, Messages);
	TestEqual(TEXT("unknown length: not checked"), Messages.Num(), 0);

	// Part selectors: never empty, ids checked only against a list the caller gives
	FGSTN_CombatTimelineData Hurt = Data;
	Hurt.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_HurtboxModifier>(2, 10));
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Hurt, NoContext, Messages);
	TestTrue(TEXT("a hurtbox modifier addressing nothing (the whole body) is an error"), HasMessage(Messages, true, TEXT("addresses no part")));

	FGSTN_TimelineTrack_HurtboxModifier Feet;
	Feet.StartFrame = 2;
	Feet.EndFrame = 10;
	Feet.Part.GroupName = TEXT("Feet");
	FGSTN_CombatTimelineData Dodge = Data;
	Dodge.Tracks.Add(FTrackInstance::Make<FGSTN_TimelineTrack_HurtboxModifier>(Feet));
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Dodge, NoContext, Messages);
	TestEqual(TEXT("a named group with no part list given is accepted"), Messages.Num(), 0);
	const TSet<FName> Groups = { TEXT("Arms") };
	FGSTN_TimelineValidationContext Known;
	Known.KnownGroupNames = &Groups;
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Dodge, Known, Messages);
	TestTrue(TEXT("a group missing from the given list is reported"), HasMessage(Messages, true, TEXT("unknown group")));

	// Ranges
	FGSTN_CombatTimelineData Backwards = Data;
	Backwards.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_Tags>(10, 10));
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Backwards, NoContext, Messages);
	TestTrue(TEXT("empty range is an error"), HasMessage(Messages, true, TEXT("empty range")));

	// Tail start past the end is reported (and clamped, see TailStartClampedToLength)
	FGSTN_CombatTimelineData Tail = Data;
	Tail.Tail.bHasTail = true;
	Tail.Tail.StartFrame = 99;
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Tail, NoContext, Messages);
	TestTrue(TEXT("tail start past the end is a warning"), HasMessage(Messages, false, TEXT("Tail start")));

	// Ricochet response without frames
	FGSTN_CombatTimelineData Recoil = Data;
	Recoil.bHasRicochetResponse = true;
	Messages.Reset();
	FGSTN_TimelineValidation::Validate(Recoil, NoContext, Messages);
	TestTrue(TEXT("enabled ricochet response without frames is a warning"), HasMessage(Messages, false, TEXT("ricochet")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TimelineTailCancelFlagAfterStartTest, "GSTNCombat.Core.Timeline.TailCancelFlagAfterStart", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TimelineTailCancelFlagAfterStartTest::RunTest(const FString& Parameters)
{
	using namespace GSTNTimelineTest;
	FGSTN_CombatTimelineData Data = MakeData();
	Data.Tail.bHasTail = true; // start defaults to FirstActionableFrame (12)
	Data.Tail.BlendOutFrames = 6;

	FGSTN_TimelineOutput Output;
	FGSTN_TimelinePlayer::Evaluate(Data, At(12, 11), Output);
	TestTrue(TEXT("flag on the tail start frame"), Output.bTailCancel);
	TestEqual(TEXT("tail start defaults to FirstActionableFrame"), Output.TailStartFrame, 12);
	TestEqual(TEXT("blend-out frames are reported"), Output.TailBlendOutFrames, 6);
	FGSTN_TimelinePlayer::Evaluate(Data, At(29, 28), Output);
	TestTrue(TEXT("flag on the last frame"), Output.bTailCancel);
	FGSTN_TimelinePlayer::Evaluate(Data, At(30, 29), Output);
	TestFalse(TEXT("the timeline is over at TotalFrames"), Output.bTailCancel);
	TestTrue(TEXT("IsInTail agrees"), FGSTN_TimelinePlayer::IsInTail(Data, 20) && UGSTN_TimelineUtils::IsInTail(Data, 20));

	// An explicit start later than the first actionable frame
	Data.Tail.StartFrame = 20;
	FGSTN_TimelinePlayer::Evaluate(Data, At(19, 18), Output);
	TestFalse(TEXT("explicit start: not yet at 19"), Output.bTailCancel);
	FGSTN_TimelinePlayer::Evaluate(Data, At(20, 19), Output);
	TestTrue(TEXT("explicit start: flag at 20"), Output.bTailCancel);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TimelineTailCancelNotBeforeStartTest, "GSTNCombat.Core.Timeline.TailCancelNotBeforeStart", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TimelineTailCancelNotBeforeStartTest::RunTest(const FString& Parameters)
{
	using namespace GSTNTimelineTest;
	FGSTN_CombatTimelineData Data = MakeData();
	Data.Tail.bHasTail = true;
	Data.Tail.StartFrame = 15;

	FGSTN_TimelineOutput Output;
	for (int32 Frame = 0; Frame < 15; ++Frame)
	{
		FGSTN_TimelinePlayer::Evaluate(Data, At(Frame, Frame - 1), Output);
		TestFalse(*FString::Printf(TEXT("no tail cancel at frame %d"), Frame), Output.bTailCancel);
		TestFalse(*FString::Printf(TEXT("IsInTail false at frame %d"), Frame), FGSTN_TimelinePlayer::IsInTail(Data, Frame));
	}
	FGSTN_TimelinePlayer::Evaluate(Data, At(14, 13), Output);
	TestFalse(TEXT("none one frame before the start, even though the character is actionable since 12"), Output.bTailCancel);
	TestFalse(TEXT("not busy there: the normal rules apply, not the tail"), Output.bBusy);
	FGSTN_TimelinePlayer::Evaluate(Data, At(15, 14), Output);
	TestTrue(TEXT("flag on the start frame"), Output.bTailCancel);
	TestFalse(TEXT("negative frames are never in the tail"), FGSTN_TimelinePlayer::IsInTail(Data, -3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TimelineTailCancelUnsetMeansNoneTest, "GSTNCombat.Core.Timeline.TailCancelUnsetMeansNone", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TimelineTailCancelUnsetMeansNoneTest::RunTest(const FString& Parameters)
{
	using namespace GSTNTimelineTest;
	FGSTN_CombatTimelineData Data = MakeData(); // default tail: off
	TestFalse(TEXT("a tail is off by default"), Data.Tail.bHasTail);
	TestEqual(TEXT("no tail start"), Data.GetTailStartFrame(), static_cast<int32>(INDEX_NONE));

	// A start frame without the switch is ignored
	Data.Tail.StartFrame = 5;

	FGSTN_TimelineOutput Output;
	bool bAny = false;
	for (int32 Frame = 0; Frame <= 31; ++Frame)
	{
		FGSTN_TimelinePlayer::Evaluate(Data, At(Frame, Frame - 1), Output);
		bAny |= Output.bTailCancel || FGSTN_TimelinePlayer::IsInTail(Data, Frame);
	}
	TestFalse(TEXT("no frame is in a tail"), bAny);
	TestEqual(TEXT("output reports no tail start"), Output.TailStartFrame, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("and no blend-out"), Output.TailBlendOutFrames, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TimelineTailStartClampedToLengthTest, "GSTNCombat.Core.Timeline.TailStartClampedToLength", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TimelineTailStartClampedToLengthTest::RunTest(const FString& Parameters)
{
	using namespace GSTNTimelineTest;
	FGSTN_CombatTimelineData Data = MakeData();
	Data.Tail.bHasTail = true;
	Data.Tail.StartFrame = 999;

	TestEqual(TEXT("start clamped to TotalFrames"), Data.GetTailStartFrame(), 30);
	bool bAny = false;
	for (int32 Frame = 0; Frame <= 40; ++Frame)
	{
		bAny |= FGSTN_TimelinePlayer::IsInTail(Data, Frame);
	}
	TestFalse(TEXT("a tail that starts at the end is empty"), bAny);

	// Blend-out cannot outlast the timeline
	Data.Tail.StartFrame = 25;
	Data.Tail.BlendOutFrames = 100;
	FGSTN_TimelineOutput Output;
	FGSTN_TimelinePlayer::Evaluate(Data, At(26, 25), Output);
	TestEqual(TEXT("blend-out clamped to the frames left"), Output.TailBlendOutFrames, 5);

	// Shorter timeline than the first actionable frame: the default start is clamped as well
	FGSTN_CombatTimelineData Short;
	Short.TotalFrames = 10;
	Short.FirstActionableFrame = 50;
	Short.Tail.bHasTail = true;
	TestEqual(TEXT("default start clamped"), Short.GetTailStartFrame(), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TimelineRicochetOutcomeSwitchesToRecoilTest, "GSTNCombat.Core.Timeline.RicochetOutcomeSwitchesToRecoil", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TimelineRicochetOutcomeSwitchesToRecoilTest::RunTest(const FString& Parameters)
{
	using namespace GSTNTimelineTest;
	FGSTN_CombatTimelineData Data = MakeData();
	Data.Tail.bHasTail = true;
	AddRicochet(Data);
	FGSTN_TimelineOutput Output;

	// Without the flag the timeline plays as usual
	FGSTN_TimelinePlayer::Evaluate(Data, At(20, 19), Output);
	TestFalse(TEXT("no recoil without the outcome flag"), Output.bRecoil);
	TestEqual(TEXT("timeline length"), Output.TotalFrames, 30);
	TestTrue(TEXT("tail applies to the timeline"), Output.bTailCancel);

	// With the flag the same timeline frame is ignored and the recoil data is evaluated
	FGSTN_TimelineEvalParams Params = At(20, 19);
	Params.bRicochet = true;
	Params.RecoilFrame = 0;
	Params.PreviousRecoilFrame = INDEX_NONE;
	FGSTN_TimelinePlayer::Evaluate(Data, Params, Output);
	TestTrue(TEXT("recoil evaluated"), Output.bRecoil);
	TestEqual(TEXT("recoil length"), Output.TotalFrames, 8);
	TestTrue(TEXT("the recoil starts now"), Output.bRecoilStartsNow);
	TestTrue(TEXT("with its cue"), Output.RecoilCueTag == GSTNCombatButtonTags::Button_Attack.GetTag());
	TestFalse(TEXT("the timeline's tail does not apply to the recoil"), Output.bTailCancel);
	TestEqual(TEXT("timeline windows are not reported"), Output.TagWindows.Num() + Output.CancelWindows.Num() + Output.Events.Num(), 0);

	Params.RecoilFrame = 3;
	Params.PreviousRecoilFrame = 2;
	FGSTN_TimelinePlayer::Evaluate(Data, Params, Output);
	TestFalse(TEXT("cue only on the first recoil frame"), Output.bRecoilStartsNow);
	TestEqual(TEXT("recoil movement active at 3"), Output.ActiveMovement.Num(), 1);
	TestTrue(TEXT("it is the response's movement"), Output.ActiveMovement.Num() == 1 && Output.ActiveMovement[0] == &Data.RicochetResponse.Movement);
	TestFalse(TEXT("not finished"), Output.bFinished);

	Params.RecoilFrame = 8;
	Params.PreviousRecoilFrame = 7;
	FGSTN_TimelinePlayer::Evaluate(Data, Params, Output);
	TestTrue(TEXT("recoil finished after its frames"), Output.bFinished);

	// No response authored: the flag changes nothing (the weapon or kit fallback is the caller's to pass)
	FGSTN_CombatTimelineData Plain = MakeData();
	Params.RecoilFrame = 3;
	Params.PreviousRecoilFrame = 2;
	FGSTN_TimelinePlayer::Evaluate(Plain, Params, Output);
	TestFalse(TEXT("no response: normal evaluation"), Output.bRecoil);

	FGSTN_RicochetResponse Fallback;
	Fallback.RecoilFrames = 5;
	Params.RicochetFallback = &Fallback;
	FGSTN_TimelinePlayer::Evaluate(Plain, Params, Output);
	TestTrue(TEXT("fallback response used when the timeline has none"), Output.bRecoil && Output.TotalFrames == 5);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
