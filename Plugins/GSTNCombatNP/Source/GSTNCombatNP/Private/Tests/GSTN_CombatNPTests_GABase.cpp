// Automation tests of the rules UGSTN_GA_Base decides with (combat-m3 6.1): the combat clock and the waits counted on it, the
// interrupt policy and the end on trigger release. The ability only gathers their arguments from the simulation, so the rules
// run here without a world or an ability system; the integration is checked by the PIE scenarios (stance_lock, camera_aim, triggered_timeline).

#include "Abilities/GSTN_GATypes.h"
#include "Misc/AutomationTest.h"
#include "Simulation/GSTN_CombatActionTypes.h"
#include "Simulation/GSTN_InputHostUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** What WaitCombatFrames keeps in the ability's sync variables, and the tick that consumes it */
	struct FWaitState
	{
		int32 StartClock = INDEX_NONE;
		int32 Frames = 0;

		void Arm(int32 Clock, int32 InFrames)
		{
			StartClock = Clock;
			Frames = FMath::Max(InFrames, 1);
		}

		/** One ability tick on the combat clock; true on the tick the wait elapses */
		bool Tick(int32 Clock)
		{
			if (Frames > 0 && FGSTN_CombatClock::Elapsed(Clock, StartClock) >= Frames)
			{
				Frames = 0;
				return true;
			}
			return false;
		}
	};

	/** Frame at which a wait of N started at StartFrame elapses, hitstop being [HitstopStart, HitstopEnd) in simulation frames */
	int32 ElapseFrame(int32 StartFrame, int32 N, int32 HitstopStart, int32 HitstopEnd, int32 SnapshotFrame = INDEX_NONE, bool bRollBack = false)
	{
		int32 Paused = 0;
		FWaitState Wait;
		FWaitState Snapshot;
		int32 SnapshotPaused = 0;
		bool bRolled = false;

		for (int32 Frame = 0; Frame < 200; ++Frame)
		{
			// The combat tick adds the frame to the paused total when it is a hitstop frame, before the ability system ticks
			if (Frame >= HitstopStart && Frame < HitstopEnd)
			{
				++Paused;
			}
			const int32 Clock = FGSTN_CombatClock::FromFrame(Frame, Paused);

			if (Frame == StartFrame)
			{
				Wait.Arm(Clock, N);
			}
			if (Wait.Tick(Clock))
			{
				return Frame;
			}

			if (Frame == SnapshotFrame)
			{
				Snapshot = Wait;
				SnapshotPaused = Paused;
			}
			if (bRollBack && !bRolled && Frame == SnapshotFrame + 6)
			{
				// A correction: the state of the snapshot frame comes back and the frames after it are simulated again
				bRolled = true;
				Wait = Snapshot;
				Paused = SnapshotPaused;
				Frame = SnapshotFrame;
			}
		}
		return INDEX_NONE;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GABaseWaitFramesRollbackTest, "GSTNCombat.NP.GABase.WaitFramesRollback", GSTN_NP_TEST_FLAGS)
bool FGSTN_GABaseWaitFramesRollbackTest::RunTest(const FString& Parameters)
{
	// The clock is the frame minus the hitstop frames so far, and not valid before the first frame
	TestEqual(TEXT("clock without hitstop"), FGSTN_CombatClock::FromFrame(40, 0), 40);
	TestEqual(TEXT("clock stands still in hitstop"), FGSTN_CombatClock::FromFrame(41, 6) - FGSTN_CombatClock::FromFrame(40, 5), 0);
	TestEqual(TEXT("clock before the first frame"), FGSTN_CombatClock::FromFrame(INDEX_NONE, 0), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("elapsed counts from the start clock"), FGSTN_CombatClock::Elapsed(50, 42), 8);
	TestEqual(TEXT("elapsed is never negative"), FGSTN_CombatClock::Elapsed(40, 42), 0);
	TestEqual(TEXT("elapsed without a start is 0"), FGSTN_CombatClock::Elapsed(50, INDEX_NONE), 0);
	TestEqual(TEXT("elapsed without a clock is 0"), FGSTN_CombatClock::Elapsed(INDEX_NONE, 3), 0);

	// A wait of 8 armed on frame 10 elapses on frame 18 without hitstop
	TestEqual(TEXT("wait without hitstop"), ElapseFrame(10, 8, INDEX_NONE, INDEX_NONE), 18);
	// A wait of 0 or less still waits one frame, so the callback never runs inside the call that armed it
	TestEqual(TEXT("a wait of 0 waits one frame"), ElapseFrame(10, 0, INDEX_NONE, INDEX_NONE), 11);
	// Hitstop on [12, 17) stops the clock for 5 frames: the same wait elapses 5 frames later
	TestEqual(TEXT("wait through hitstop"), ElapseFrame(10, 8, 12, 17), 23);
	// Hitstop before the wait started changes nothing for it
	TestEqual(TEXT("hitstop before the start"), ElapseFrame(30, 8, 12, 17), 38);

	// A rollback to frame 14 (inside the wait and inside the hitstop) and a re-simulation elapse on the same frame
	const int32 Straight = ElapseFrame(10, 8, 12, 17);
	TestEqual(TEXT("rollback and replay elapse on the same frame"), ElapseFrame(10, 8, 12, 17, 14, true), Straight);
	// ... also when the replay starts before the hitstop
	TestEqual(TEXT("rollback before the hitstop"), ElapseFrame(10, 8, 12, 17, 11, true), Straight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GABaseInterruptPolicyTest, "GSTNCombat.NP.GABase.InterruptPolicy", GSTN_NP_TEST_FLAGS)
bool FGSTN_GABaseInterruptPolicyTest::RunTest(const FString& Parameters)
{
	using Policy = EGSTN_InterruptPolicy;

	// SkipIfBusy blocks a non-command activation while a timeline runs, and only that
	TestTrue(TEXT("skip: trigger while busy"), FGSTN_GARules::BlocksActivation(Policy::SkipIfBusy, true, false));
	TestFalse(TEXT("skip: trigger while idle"), FGSTN_GARules::BlocksActivation(Policy::SkipIfBusy, false, false));
	TestFalse(TEXT("skip: a command while busy (the combat tick already allowed it)"), FGSTN_GARules::BlocksActivation(Policy::SkipIfBusy, true, true));
	TestFalse(TEXT("skip: a command while idle"), FGSTN_GARules::BlocksActivation(Policy::SkipIfBusy, false, true));
	TestFalse(TEXT("skip never interrupts"), FGSTN_GARules::ShouldInterrupt(Policy::SkipIfBusy, true, false));

	// InterruptCurrent never blocks, and cancels the running timeline's abilities for a non-command activation while one runs
	TestFalse(TEXT("interrupt never blocks"), FGSTN_GARules::BlocksActivation(Policy::InterruptCurrent, true, false));
	TestTrue(TEXT("interrupt: trigger while busy"), FGSTN_GARules::ShouldInterrupt(Policy::InterruptCurrent, true, false));
	TestFalse(TEXT("interrupt: trigger while idle"), FGSTN_GARules::ShouldInterrupt(Policy::InterruptCurrent, false, false));
	TestFalse(TEXT("interrupt: a command while busy is the combat tick's business"), FGSTN_GARules::ShouldInterrupt(Policy::InterruptCurrent, true, true));

	// The command marker is a property of the call for one spec: set under its scope, nested scopes keep it, gone after the last
	// one, and an ability another one triggers inside the command's activation (another handle) is not a command
	FGameplayAbilitySpecHandle Command;
	Command.GenerateNewHandle();
	FGameplayAbilitySpecHandle Triggered;
	Triggered.GenerateNewHandle();
	TestFalse(TEXT("no command activation outside a scope"), FGSTN_InputHostUtils::IsCommandActivation(Command));
	{
		const FGSTN_CommandActivationScope Outer(Command);
		TestTrue(TEXT("command activation under a scope"), FGSTN_InputHostUtils::IsCommandActivation(Command));
		TestFalse(TEXT("an ability triggered inside it is not a command"), FGSTN_InputHostUtils::IsCommandActivation(Triggered));
		TestFalse(TEXT("an invalid handle never is"), FGSTN_InputHostUtils::IsCommandActivation(FGameplayAbilitySpecHandle()));
		{
			const FGSTN_CommandActivationScope Inner(Command);
			TestTrue(TEXT("nested scope"), FGSTN_InputHostUtils::IsCommandActivation(Command));
		}
		TestTrue(TEXT("still under the outer scope"), FGSTN_InputHostUtils::IsCommandActivation(Command));
		{
			const FGSTN_CommandActivationScope Other(Triggered);
			TestTrue(TEXT("a second spec under its own scope"), FGSTN_InputHostUtils::IsCommandActivation(Triggered));
			TestTrue(TEXT("the outer one is still a command"), FGSTN_InputHostUtils::IsCommandActivation(Command));
		}
		TestFalse(TEXT("the second spec's scope is gone"), FGSTN_InputHostUtils::IsCommandActivation(Triggered));
	}
	TestFalse(TEXT("no command activation after the scope"), FGSTN_InputHostUtils::IsCommandActivation(Command));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GABaseReleaseEndsTest, "GSTNCombat.NP.GABase.ReleaseEnds", GSTN_NP_TEST_FLAGS)
bool FGSTN_GABaseReleaseEndsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("button released ends"), FGSTN_GARules::EndsOnRelease(true, 3, false));
	TestFalse(TEXT("button held keeps running"), FGSTN_GARules::EndsOnRelease(true, 3, true));
	TestFalse(TEXT("without the flag the release is ignored"), FGSTN_GARules::EndsOnRelease(false, 3, false));
	TestFalse(TEXT("a start without a button never ends on release"), FGSTN_GARules::EndsOnRelease(true, INDEX_NONE, false));
	TestTrue(TEXT("button 0 counts"), FGSTN_GARules::EndsOnRelease(true, 0, false));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
