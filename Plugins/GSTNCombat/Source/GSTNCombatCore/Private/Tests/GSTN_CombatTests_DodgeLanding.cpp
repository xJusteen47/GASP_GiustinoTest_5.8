// Automation tests of the dodge landing (owner 2026-11-17: ending lag and re-dodge lock): pure frame arithmetic on the combat state, so every
// machine and every re-simulation decides the same.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Movement/GSTN_DodgeLanding.h"
#include "Movement/GSTN_MoveTimelines.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const int32 Landed = 100;

	FGameplayTag Dodge() { return GSTNCommandTags::Defence_Dodge.GetTag(); }
	FGameplayTag Jump() { return GSTNCommandTags::Movement_Jump.GetTag(); }
	FGameplayTag Attack() { return GSTNCommandTags::Attack_Neutral.GetTag(); }

	FGSTN_DodgeLandingState RoundTrip(const FGSTN_DodgeLandingState& State)
	{
		FBitWriter Writer(256, true);
		FGSTN_DodgeLandingState Copy = State;
		Copy.NetSerialize(Writer);
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_DodgeLandingState Loaded;
		Loaded.LandedFrame = 7;
		Loaded.NetSerialize(Reader);
		return Loaded;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DodgeLandingEndLagTest, "GSTNCombat.Core.DodgeLanding.EndLagBlocksActions", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DodgeLandingEndLagTest::RunTest(const FString& Parameters)
{
	const FGSTN_DodgeLandingKitExtension Kit;
	// Owner 2026-11-25: the belly part (the move's 8) plus the get-up roll to stand (10)
	TestEqual(TEXT("default get-up roll 10"), Kit.GetUpRollFrames, 10);
	TestEqual(TEXT("default lag = the move's plus the get-up roll"), FGSTN_DodgeLandingLogic::ResolveEndLag(Kit, FGSTN_MoveTimelines::DefaultDodgeEndLagFrames), 18);
	const FGSTN_DodgeLandingState State = FGSTN_DodgeLandingState::Open(Landed, 8, Kit.RedodgeLockFrames);

	// Frames 1..8 of the lag (the landing frame is 1): nothing acts, not even a jump or an attack
	for (int32 Number = 1; Number <= 8; ++Number)
	{
		const int32 Frame = Landed + Number - 1;
		TestTrue(*FString::Printf(TEXT("lag frame %d"), Number), State.IsEndLag(Frame));
		TestEqual(*FString::Printf(TEXT("lag frame %d counted from 1"), Number), State.EndLagFrameNumber(Frame), Number);
		TestEqual(*FString::Printf(TEXT("jump refused on lag frame %d"), Number), FGSTN_DodgeLandingLogic::Gate(State, Kit, Frame, Jump()), EGSTN_DodgeLandingGate::Refused);
		TestEqual(*FString::Printf(TEXT("attack refused on lag frame %d"), Number), FGSTN_DodgeLandingLogic::Gate(State, Kit, Frame, Attack()), EGSTN_DodgeLandingGate::Refused);
	}
	TestFalse(TEXT("frame 9 is out of the lag"), State.IsEndLag(Landed + 8));
	TestEqual(TEXT("frame 9: a jump is the other rules' business"), FGSTN_DodgeLandingLogic::Gate(State, Kit, Landed + 8, Jump()), EGSTN_DodgeLandingGate::Free);
	TestFalse(TEXT("the frame before the landing is no lag"), State.IsEndLag(Landed - 1));

	// A kit's own lag wins over the move's
	FGSTN_DodgeLandingKitExtension Slow;
	Slow.EndLagFrames = 14;
	TestEqual(TEXT("kit override plus the get-up roll"), FGSTN_DodgeLandingLogic::ResolveEndLag(Slow, 8), 24);
	Slow.GetUpRollFrames = 0;
	TestEqual(TEXT("kit override, no get-up roll"), FGSTN_DodgeLandingLogic::ResolveEndLag(Slow, 8), 14);
	Slow.EndLagFrames = 0;
	TestEqual(TEXT("kit 0 and no get-up roll = no lag"), FGSTN_DodgeLandingLogic::ResolveEndLag(Slow, 8), 0);
	Slow.GetUpRollFrames = 10;
	TestEqual(TEXT("no belly lag: only the get-up roll"), FGSTN_DodgeLandingLogic::ResolveEndLag(Slow, 8), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DodgeLandingLockTest, "GSTNCombat.Core.DodgeLanding.RedodgeLock", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DodgeLandingLockTest::RunTest(const FString& Parameters)
{
	const FGSTN_DodgeLandingKitExtension Kit;
	TestEqual(TEXT("default lock 10"), Kit.RedodgeLockFrames, 10);
	const FGSTN_DodgeLandingState State = FGSTN_DodgeLandingState::Open(Landed, 8, Kit.RedodgeLockFrames);

	for (int32 Number = 1; Number <= 10; ++Number)
	{
		TestEqual(*FString::Printf(TEXT("dodge refused on frame %d from the landing"), Number), FGSTN_DodgeLandingLogic::Gate(State, Kit, Landed + Number - 1, Dodge()),
			EGSTN_DodgeLandingGate::Refused);
	}
	TestEqual(TEXT("frame 11: the dodge is free"), FGSTN_DodgeLandingLogic::Gate(State, Kit, Landed + 10, Dodge()), EGSTN_DodgeLandingGate::Free);
	TestEqual(TEXT("frames 9-10: other commands are free"), FGSTN_DodgeLandingLogic::Gate(State, Kit, Landed + 9, Attack()), EGSTN_DodgeLandingGate::Free);

	// The kit's list
	FGSTN_DodgeLandingKitExtension Custom;
	Custom.LockedCommands.AddTag(Jump());
	TestEqual(TEXT("a kit locks the jump instead"), FGSTN_DodgeLandingLogic::Gate(State, Custom, Landed + 9, Jump()), EGSTN_DodgeLandingGate::Refused);
	TestEqual(TEXT("and not the dodge"), FGSTN_DodgeLandingLogic::Gate(State, Custom, Landed + 9, Dodge()), EGSTN_DodgeLandingGate::Free);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DodgeLandingZeroTest, "GSTNCombat.Core.DodgeLanding.ZeroIsBefore", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DodgeLandingZeroTest::RunTest(const FString& Parameters)
{
	// 0 lag and 0 lock: no state at all, the behaviour before 2026-11-17
	FGSTN_DodgeLandingKitExtension Kit;
	Kit.RedodgeLockFrames = 0;
	const FGSTN_DodgeLandingState State = FGSTN_DodgeLandingState::Open(Landed, 0, 0);
	TestFalse(TEXT("nothing opened"), State.IsSet());
	TestEqual(TEXT("dodge free on the landing frame"), FGSTN_DodgeLandingLogic::Gate(State, Kit, Landed, Dodge()), EGSTN_DodgeLandingGate::Free);
	TestEqual(TEXT("jump free on the landing frame"), FGSTN_DodgeLandingLogic::Gate(State, Kit, Landed, Jump()), EGSTN_DodgeLandingGate::Free);

	// Lock with no lag, lag with no lock
	const FGSTN_DodgeLandingState LockOnly = FGSTN_DodgeLandingState::Open(Landed, 0, 10);
	TestFalse(TEXT("lock only: no lag"), LockOnly.IsEndLag(Landed));
	TestEqual(TEXT("lock only: jump free"), FGSTN_DodgeLandingLogic::Gate(LockOnly, Kit, Landed, Jump()), EGSTN_DodgeLandingGate::Free);
	const FGSTN_DodgeLandingState LagOnly = FGSTN_DodgeLandingState::Open(Landed, 8, 0);
	TestEqual(TEXT("lag only: dodge free after the lag"), FGSTN_DodgeLandingLogic::Gate(LagOnly, Kit, Landed + 8, Dodge()), EGSTN_DodgeLandingGate::Free);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DodgeLandingRecoveryTest, "GSTNCombat.Core.DodgeLanding.RollOut", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DodgeLandingRecoveryTest::RunTest(const FString& Parameters)
{
	FGSTN_DodgeLandingKitExtension Kit;
	FGSTN_DodgeLandingState State = FGSTN_DodgeLandingState::Open(Landed, 8, 10);
	TestEqual(TEXT("off by default"), FGSTN_DodgeLandingLogic::Gate(State, Kit, Landed + 5, Jump()), EGSTN_DodgeLandingGate::Refused);

	Kit.RecoveryCommands.AddTag(Jump());
	Kit.RecoveryCommands.AddTag(Dodge());
	Kit.RecoveryFromFrame = 4;
	TestEqual(TEXT("before its frame: refused"), FGSTN_DodgeLandingLogic::Gate(State, Kit, Landed + 2, Jump()), EGSTN_DodgeLandingGate::Refused);
	TestEqual(TEXT("from lag frame 4: a roll-out"), FGSTN_DodgeLandingLogic::Gate(State, Kit, Landed + 3, Jump()), EGSTN_DodgeLandingGate::Recovery);
	TestEqual(TEXT("a locked command is never a roll-out"), FGSTN_DodgeLandingLogic::Gate(State, Kit, Landed + 3, Dodge()), EGSTN_DodgeLandingGate::Refused);
	TestEqual(TEXT("not listed: refused"), FGSTN_DodgeLandingLogic::Gate(State, Kit, Landed + 3, Attack()), EGSTN_DodgeLandingGate::Refused);

	// The roll-out ends the lag on its frame; the lock keeps running
	State.CutEndLag(Landed + 3);
	TestFalse(TEXT("lag over"), State.IsEndLag(Landed + 3));
	TestTrue(TEXT("lag frames before it stay"), State.IsEndLag(Landed + 2));
	TestTrue(TEXT("lock still on"), State.IsLocked(Landed + 5));
	TestEqual(TEXT("dodge still refused"), FGSTN_DodgeLandingLogic::Gate(State, Kit, Landed + 5, Dodge()), EGSTN_DodgeLandingGate::Refused);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DodgeLandingStateTest, "GSTNCombat.Core.DodgeLanding.StateAndReplay", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DodgeLandingStateTest::RunTest(const FString& Parameters)
{
	// A hit reaction ends both windows; so does the end of the longer one
	FGSTN_DodgeLandingState Hit = FGSTN_DodgeLandingState::Open(Landed, 8, 10);
	FGSTN_DodgeLandingLogic::Normalize(Hit, Landed + 2, true);
	TestFalse(TEXT("a hit clears it"), Hit.IsSet());
	FGSTN_DodgeLandingState Expiring = FGSTN_DodgeLandingState::Open(Landed, 8, 10);
	FGSTN_DodgeLandingLogic::Normalize(Expiring, Landed + 9, false);
	TestTrue(TEXT("kept while the lock runs"), Expiring.IsSet());
	FGSTN_DodgeLandingLogic::Normalize(Expiring, Landed + 10, false);
	TestFalse(TEXT("cleared once both are over"), Expiring.IsSet());

	// Serialized exactly (the state is what a rollback restores and a client reconciles against)
	const FGSTN_DodgeLandingState Open = FGSTN_DodgeLandingState::Open(123456, 8, 10);
	TestTrue(TEXT("round trip of a set state"), RoundTrip(Open) == Open);
	TestFalse(TEXT("round trip of an unset state stays unset"), RoundTrip(FGSTN_DodgeLandingState()).IsSet());

	// Replay: the same landing and frames give the same decisions on every run
	auto Run = [](TArray<uint8>& Out)
	{
		FGSTN_DodgeLandingKitExtension Kit;
		Kit.RecoveryCommands.AddTag(GSTNCommandTags::Movement_Jump.GetTag());
		FGSTN_DodgeLandingState State = FGSTN_DodgeLandingState::Open(Landed, 8, 10);
		const FGameplayTag Commands[] = { GSTNCommandTags::Defence_Dodge.GetTag(), GSTNCommandTags::Movement_Jump.GetTag(), GSTNCommandTags::Attack_Neutral.GetTag() };
		for (int32 Frame = Landed - 2; Frame < Landed + 14; ++Frame)
		{
			FGSTN_DodgeLandingLogic::Normalize(State, Frame, false);
			for (const FGameplayTag& Command : Commands)
			{
				const EGSTN_DodgeLandingGate Gate = FGSTN_DodgeLandingLogic::Gate(State, Kit, Frame, Command);
				Out.Add(static_cast<uint8>(Gate));
				if (Gate == EGSTN_DodgeLandingGate::Recovery && Frame == Landed + 5)
				{
					State.CutEndLag(Frame);
				}
			}
		}
	};
	TArray<uint8> First;
	TArray<uint8> Second;
	Run(First);
	Run(Second);
	TestTrue(TEXT("replay identical"), First == Second);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DodgeLandingDefaultsTest, "GSTNCombat.Core.DodgeLanding.Defaults", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DodgeLandingDefaultsTest::RunTest(const FString& Parameters)
{
	const FGSTN_DodgeLandingKitExtension Kit;
	TestTrue(TEXT("belly slide on by default"), Kit.LandingSlide.bEnabled);
	TestEqual(TEXT("it slides along the landing velocity whatever the facing"), Kit.LandingSlide.OppositeDotThreshold, -1.f);
	TestFalse(TEXT("no run carry"), Kit.LandingSlide.bUseCarriedSpeed);
	TestTrue(TEXT("roll-out off by default"), Kit.RecoveryCommands.IsEmpty());

	// The ground and air dodge carry the 8-frame lag as the move's data, with room for it in the timeline
	const FGSTN_DashMoveData Move = FGSTN_MoveTimelines::GroundDodgeMove();
	const FGSTN_MoveTimelineParams Ground = FGSTN_MoveTimelines::GroundDodgeParams(Move, 60.f);
	const FGSTN_ArcPrediction Arc = FGSTN_DashMath::PredictArc(Move, FGSTN_MoveTimelines::NominalGravity, 60.f);
	TestEqual(TEXT("ground dodge lag"), Ground.LandingLagFrames, 8);
	TestTrue(TEXT("room for the arc and the lag"), Ground.TotalFrames >= Arc.AirFrames + Ground.LandingLagFrames);
	TestEqual(TEXT("air dodge lag"), FGSTN_MoveTimelines::AirDodgeFromGroundParams(Move, 60.f).LandingLagFrames, 8);
	return true;
}

#endif
