// Automation tests of the hit part of the combat sync state (combat-m4-hits 11, M4 H6): wire format, idle cost, the target ring,
// blending and reconcile. They need no world: the state is plain data and the archives are bit archives.

#include "Misc/AutomationTest.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Simulation/GSTN_CombatSimTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	constexpr int32 HitStateFrame = 105;
	constexpr int32 HitTimelineStart = 100;

	void HitTestSetFrame(FGSTN_CombatSyncCore& Core, int32 Frame)
	{
		FGSTN_InputSample Sample;
		Sample.Frame = Frame;
		Core.Input.History.Append(Sample);
	}

	/** An attacker running a timeline that has hit one victim: origin, one target, one confirmed group */
	FGSTN_CombatSyncState MakeAttackerWithHit()
	{
		FGSTN_CombatSyncState State;
		State.Core.ActionState = EGSTN_ActionState::InTimeline;
		State.Core.ActiveTimeline = 3;
		State.Core.TimelineStartFrame = HitTimelineStart;
		HitTestSetFrame(State.Core, HitStateFrame);
		State.Core.Hit.MoveOrigin = FGSTN_MoveOrigin::Make(FTransform(FRotator(0.0, 90.0, 0.0), FVector(1234.5, -678.9, 20.0)));
		State.Core.Hit.AddHitTarget(7, HitTimelineStart, 1);
		State.Core.Hit.HitConfirmFlags = 0x2;
		return State;
	}

	/** A victim that was hit this frame and sits in the hitstop that precedes a tumbling launch */
	FGSTN_CombatSyncState MakeVictimWithHit()
	{
		FGSTN_CombatSyncState State;
		State.Core.ActionState = EGSTN_ActionState::Tumble;
		HitTestSetFrame(State.Core, HitStateFrame);
		State.Core.HitstopStartFrame = HitStateFrame;
		State.Core.HitstopEndFrame = HitStateFrame + 6;
		State.Core.Hit.HitstunEndFrame = HitStateFrame + 40;
		State.Core.Hit.SetPendingReaction(EGSTN_HitReaction::Launch);
		State.Core.Hit.SetPendingLaunch(FVector(900.0, -300.0, 1500.0));
		State.Core.Requests.Add(EGSTN_RequestType::ApplyDamage, HitStateFrame, 1);
		State.Core.Requests.Add(EGSTN_RequestType::Freeze, HitStateFrame, 6);
		State.Core.Requests.Add(EGSTN_RequestType::Cue, HitStateFrame, 2);
		return State;
	}

	int64 HitStateBits(FGSTN_CombatSyncState& State)
	{
		FBitWriter Writer(1024, true);
		State.NetSerialize(FNetSerializeParams(Writer));
		return Writer.GetNumBits();
	}

	FGSTN_CombatSyncState HitStateReadBack(FGSTN_CombatSyncState& State, bool& bOutError)
	{
		FBitWriter Writer(1024, true);
		State.NetSerialize(FNetSerializeParams(Writer));
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_CombatSyncState Loaded;
		Loaded.NetSerialize(FNetSerializeParams(Reader));
		bOutError = Reader.IsError() || !Reader.AtEnd();
		return Loaded;
	}

	bool HitStateSame(const FGSTN_CombatSyncState& A, const FGSTN_CombatSyncState& B)
	{
		return !A.ShouldReconcile(B) && !B.ShouldReconcile(A);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitStateRoundTripTest, "GSTNCombat.NP.Hits.State.RoundTrip", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitStateRoundTripTest::RunTest(const FString& Parameters)
{
	// The attacker: origin, target and confirm flags survive the wire, and the loaded targets carry the timeline start of the core
	FGSTN_CombatSyncState Attacker = MakeAttackerWithHit();
	bool bError = false;
	FGSTN_CombatSyncState LoadedAttacker = HitStateReadBack(Attacker, bError);
	TestFalse(TEXT("the attacker state reads back without an error"), bError);
	TestTrue(TEXT("the attacker state is equal after the wire"), HitStateSame(Attacker, LoadedAttacker));
	TestTrue(TEXT("the target is there"), LoadedAttacker.Core.Hit.HasHitTarget(7, HitTimelineStart, 1));
	TestEqual(TEXT("the confirm flags"), static_cast<int32>(LoadedAttacker.Core.Hit.HitConfirmFlags), 0x2);
	TestTrue(TEXT("the origin is the quantised one"), LoadedAttacker.Core.Hit.MoveOrigin == Attacker.Core.Hit.MoveOrigin);
	TestEqual(TEXT("the origin position is in 1/10 cm"), LoadedAttacker.Core.Hit.MoveOrigin.PositionQ[0], 12345);

	// The victim: the windows, the pending reaction and launch, the new action state and the new request types
	FGSTN_CombatSyncState Victim = MakeVictimWithHit();
	FGSTN_CombatSyncState LoadedVictim = HitStateReadBack(Victim, bError);
	TestFalse(TEXT("the victim state reads back without an error"), bError);
	TestTrue(TEXT("the victim state is equal after the wire"), HitStateSame(Victim, LoadedVictim));
	TestTrue(TEXT("the action state is Tumble"), LoadedVictim.Core.ActionState == EGSTN_ActionState::Tumble);
	TestEqual(TEXT("the hitstun end"), LoadedVictim.Core.Hit.HitstunEndFrame, HitStateFrame + 40);
	TestTrue(TEXT("the pending reaction is a launch"), LoadedVictim.Core.Hit.HasPendingReaction() && LoadedVictim.Core.Hit.GetPendingReaction() == EGSTN_HitReaction::Launch);
	TestEqual(TEXT("the launch is quantised to 4 cm/s"), LoadedVictim.Core.Hit.GetPendingLaunch().Z, 1500.0);
	TestNotNull(TEXT("the damage request"), LoadedVictim.Core.Requests.Find(EGSTN_RequestType::ApplyDamage, HitStateFrame));
	TestNotNull(TEXT("the freeze request"), LoadedVictim.Core.Requests.Find(EGSTN_RequestType::Freeze, HitStateFrame, 6));

	// Every action state and request type fits its field
	for (uint8 Action = 0; Action < static_cast<uint8>(EGSTN_ActionState::Count); ++Action)
	{
		FGSTN_CombatSyncState State;
		State.Core.ActionState = static_cast<EGSTN_ActionState>(Action);
		State.Core.ActiveTimeline = 1;
		FGSTN_CombatSyncState Loaded = HitStateReadBack(State, bError);
		TestTrue(*FString::Printf(TEXT("action state %d round trips"), Action), !bError && Loaded.Core.ActionState == State.Core.ActionState);
	}
	for (uint8 Type = 1; Type < static_cast<uint8>(EGSTN_RequestType::Count); ++Type)
	{
		FGSTN_CombatSyncState State;
		State.Core.Requests.Add(static_cast<EGSTN_RequestType>(Type), 9, 4);
		FGSTN_CombatSyncState Loaded = HitStateReadBack(State, bError);
		TestTrue(*FString::Printf(TEXT("request type %d round trips"), Type), !bError && Loaded.Core.Requests.Find(static_cast<EGSTN_RequestType>(Type), 9, 4) != nullptr);
	}

	// The reserved SDI count keeps its value
	FGSTN_CombatSyncState Sdi = MakeVictimWithHit();
	Sdi.Core.Hit.SDIDistanceUsed = 3;
	TestEqual(TEXT("the SDI count survives the wire"), static_cast<int32>(HitStateReadBack(Sdi, bError).Core.Hit.SDIDistanceUsed), 3);

	// The byte counts the host upload budget is read from (combat-m4-hits 9b)
	FGSTN_CombatSyncState Idle;
	FGSTN_CombatSyncState Busy;
	Busy.Core.ActionState = EGSTN_ActionState::InTimeline;
	Busy.Core.ActiveTimeline = 3;
	Busy.Core.TimelineStartFrame = HitTimelineStart;
	HitTestSetFrame(Busy.Core, HitStateFrame);
	const int64 IdleBits = HitStateBits(Idle);
	const int64 BusyBits = HitStateBits(Busy);
	const int64 AttackerBits = HitStateBits(Attacker);
	const int64 VictimBits = HitStateBits(Victim);
	// A trade frame: the victim's damage, cue and freeze plus the attacker's freeze and meter gain, and a cancel of the move that was running
	FGSTN_CombatSyncState HitFrame = Victim;
	const int32 RequestFrame = HitFrame.Core.Input.History.GetLatestFrame();
	HitFrame.Core.Requests = FGSTN_CombatRequests();
	HitFrame.Core.Requests.Add(EGSTN_RequestType::ApplyDamage, RequestFrame, 1);
	HitFrame.Core.Requests.Add(EGSTN_RequestType::Cue, RequestFrame, 1);
	HitFrame.Core.Requests.Add(EGSTN_RequestType::VictimFreeze, RequestFrame, 6);
	HitFrame.Core.Requests.Add(EGSTN_RequestType::Freeze, RequestFrame, 6);
	HitFrame.Core.Requests.Add(EGSTN_RequestType::MeterGain, RequestFrame, 3);
	HitFrame.Core.Requests.Add(EGSTN_RequestType::CancelTimeline, RequestFrame, 3);
	const int64 HitFrameBits = HitStateBits(HitFrame);
	AddInfo(FString::Printf(TEXT("hit frame with six requests: %lld bits (%lld bytes)"), HitFrameBits, (HitFrameBits + 7) / 8));
	AddInfo(FString::Printf(TEXT("sync state size: idle %lld bits (%lld bytes), running a move with no hit %lld bits (%lld bytes), attacker with one hit %lld bits (%lld bytes), victim in hitstop with a tumbling launch %lld bits (%lld bytes)"),
		IdleBits, (IdleBits + 7) / 8, BusyBits, (BusyBits + 7) / 8, AttackerBits, (AttackerBits + 7) / 8, VictimBits, (VictimBits + 7) / 8));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitStateIdleZeroCostTest, "GSTNCombat.NP.Hits.State.IdleZeroCost", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitStateIdleZeroCostTest::RunTest(const FString& Parameters)
{
	// The idle baseline of the state before the hit part existed (the same measure as GSTNCombat.NP.State.RoundTrip): the hit part adds nothing to it
	FGSTN_CombatSyncState Idle;
	FBitWriter CoreWriter(512, true);
	Idle.Core.Input.NetSerialize(CoreWriter);
	int32 Start = INDEX_NONE;
	CoreWriter << Start;
	CoreWriter << Start;
	TestEqual(TEXT("an idle combatant pays the action part bit and the jump state bit, and nothing for the hit part"), HitStateBits(Idle) - 1 - CoreWriter.GetNumBits(), static_cast<int64>(2));

	// A combatant whose every hit field has run out looks idle: the same bits, the same state
	FGSTN_CombatSyncState Stale;
	HitTestSetFrame(Stale.Core, HitStateFrame);
	Stale.Core.Hit.HitstunEndFrame = HitStateFrame - 1;
	Stale.Core.Hit.ImpactEndFrame = HitStateFrame;
	Stale.Core.Hit.SetPendingReaction(EGSTN_HitReaction::PushBack);
	Stale.Core.Hit.SetPendingLaunch(FVector(400.0, 0.0, 0.0));
	Stale.Core.Hit.SDIDistanceUsed = 2;
	Stale.Core.Hit.HitConfirmFlags = 0xFF;
	Stale.Core.Hit.MoveOrigin = FGSTN_MoveOrigin::Make(FTransform(FVector(10.0, 20.0, 30.0)));
	Stale.Core.Hit.AddHitTarget(5, HitTimelineStart, 0);
	Stale.Core.HitstopStartFrame = HitStateFrame - 8;
	Stale.Core.HitstopEndFrame = HitStateFrame - 2;
	FGSTN_CombatSyncState IdleAtFrame;
	HitTestSetFrame(IdleAtFrame.Core, HitStateFrame);
	IdleAtFrame.Core.HitstopStartFrame = Stale.Core.HitstopStartFrame;
	IdleAtFrame.Core.HitstopEndFrame = Stale.Core.HitstopEndFrame;
	TestEqual(TEXT("expired hit fields cost nothing"), HitStateBits(Stale), HitStateBits(IdleAtFrame));
	TestTrue(TEXT("expired hit fields are not a difference"), HitStateSame(Stale, IdleAtFrame));
	TestTrue(TEXT("the stale state counts as an idle action part"), Stale.Core.IsActionPartDefault());

	// Inside a busy action part the hit part is one bit while nothing is live in it, and the targets of an earlier run are dead
	FGSTN_CombatSyncState Busy = MakeAttackerWithHit();
	Busy.Core.Hit = FGSTN_HitSyncState();
	const int64 BusyBits = HitStateBits(Busy);
	Busy.Core.Hit.AddHitTarget(9, HitTimelineStart - 40, 0);
	TestEqual(TEXT("a target of an earlier timeline run costs nothing"), HitStateBits(Busy), BusyBits);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitStateRingWrapTest, "GSTNCombat.NP.Hits.State.HitTargetsRingWrap", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitStateRingWrapTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatSyncState State = MakeAttackerWithHit();
	State.Core.Hit = FGSTN_HitSyncState();
	for (int32 Id = 1; Id <= 10; ++Id)
	{
		const bool bReplaced = State.Core.Hit.AddHitTarget(Id, HitTimelineStart, static_cast<uint8>(Id));
		TestEqual(*FString::Printf(TEXT("target %d replaces the oldest only once the ring is full"), Id), bReplaced, Id > FGSTN_HitSyncState::MaxHitTargets);
	}
	TestEqual(TEXT("the ring holds eight"), State.Core.Hit.NumHitTargets(), 8);
	TestEqual(TEXT("the oldest is the third added"), State.Core.Hit.GetHitTarget(0).EntityId, 3);
	TestEqual(TEXT("the newest is the tenth"), State.Core.Hit.GetHitTarget(7).EntityId, 10);
	TestFalse(TEXT("the first target was replaced"), State.Core.Hit.HasHitTarget(1, HitTimelineStart, 1));
	TestTrue(TEXT("the last target is there"), State.Core.Hit.HasHitTarget(10, HitTimelineStart, 10));

	// The slot layout of the ring is not state: the wire packs it oldest first and the loaded ring is equal
	bool bError = false;
	FGSTN_CombatSyncState Loaded = HitStateReadBack(State, bError);
	TestFalse(TEXT("no error"), bError);
	TestTrue(TEXT("equal after the wire although the head moved"), HitStateSame(State, Loaded));
	for (int32 Age = 0; Age < 8; ++Age)
	{
		TestEqual(*FString::Printf(TEXT("target %d keeps its age"), Age), Loaded.Core.Hit.GetHitTarget(Age).EntityId, 3 + Age);
	}

	// A write after the wrap on both sides stays equal: the loaded head continues where the logical order ends
	State.Core.Hit.AddHitTarget(11, HitTimelineStart, 11);
	Loaded.Core.Hit.AddHitTarget(11, HitTimelineStart, 11);
	TestTrue(TEXT("equal after one more write"), HitStateSame(State, Loaded));
	TestEqual(TEXT("the oldest moved on"), Loaded.Core.Hit.GetHitTarget(0).EntityId, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitStateInterpolateTest, "GSTNCombat.NP.Hits.State.Interpolate", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitStateInterpolateTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatSyncState From = MakeAttackerWithHit();
	FGSTN_CombatSyncState To = MakeAttackerWithHit();
	From.Core.Hit.MoveOrigin = FGSTN_MoveOrigin::Make(FTransform(FRotator(0.0, 350.0, 0.0), FVector(0.0, 0.0, 0.0)));
	To.Core.Hit.MoveOrigin = FGSTN_MoveOrigin::Make(FTransform(FRotator(0.0, 10.0, 0.0), FVector(100.0, -50.0, 20.0)));
	To.Core.Hit.HitConfirmFlags = 0x5;

	FGSTN_CombatSyncState Out;
	Out.Interpolate(&From, &To, 0.5f);
	TestEqual(TEXT("position x is the middle in 1/10 cm"), Out.Core.Hit.MoveOrigin.PositionQ[0], 500);
	TestEqual(TEXT("position y"), Out.Core.Hit.MoveOrigin.PositionQ[1], -250);
	TestEqual(TEXT("position z"), Out.Core.Hit.MoveOrigin.PositionQ[2], 100);
	// 350 to 10 degrees goes through 0, not through 180
	const double Yaw = FRotator::ClampAxis(Out.Core.Hit.MoveOrigin.ToTransform().Rotator().Yaw);
	TestTrue(TEXT("the yaw takes the short way round (about 0 degrees)"), Yaw < 0.1 || Yaw > 359.9);
	TestEqual(TEXT("discrete fields are the newer frame's"), static_cast<int32>(Out.Core.Hit.HitConfirmFlags), 0x5);
	TestTrue(TEXT("the targets are the newer frame's"), Out.Core.Hit.HasHitTarget(7, HitTimelineStart, 1));

	FGSTN_CombatSyncState AtStart;
	AtStart.Interpolate(&From, &To, 0.f);
	TestTrue(TEXT("PCT 0 is From's origin"), AtStart.Core.Hit.MoveOrigin == From.Core.Hit.MoveOrigin);
	FGSTN_CombatSyncState AtEnd;
	AtEnd.Interpolate(&From, &To, 1.f);
	TestTrue(TEXT("PCT 1 is To's origin"), AtEnd.Core.Hit.MoveOrigin == To.Core.Hit.MoveOrigin);

	FGSTN_CombatSyncState NoFrom;
	NoFrom.Interpolate(nullptr, &To, 0.5f);
	TestTrue(TEXT("without From the state is To"), HitStateSame(NoFrom, To));

	// A From that holds no origin is not blended with nothing
	From.Core.Hit.MoveOrigin = FGSTN_MoveOrigin();
	FGSTN_CombatSyncState NoOrigin;
	NoOrigin.Interpolate(&From, &To, 0.5f);
	TestTrue(TEXT("no origin in From: the origin is To's"), NoOrigin.Core.Hit.MoveOrigin == To.Core.Hit.MoveOrigin);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitStateShouldReconcileTest, "GSTNCombat.NP.Hits.State.ShouldReconcile", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitStateShouldReconcileTest::RunTest(const FString& Parameters)
{
	const FGSTN_CombatSyncState Base = MakeAttackerWithHit();
	TestFalse(TEXT("the same state does not reconcile"), Base.ShouldReconcile(MakeAttackerWithHit()));

	{
		FGSTN_CombatSyncState Other = MakeAttackerWithHit();
		Other.Core.Hit.MoveOrigin.PositionQ[1] += 1;
		TestTrue(TEXT("one unit of origin position reconciles"), Base.ShouldReconcile(Other));
		Other = MakeAttackerWithHit();
		Other.Core.Hit.MoveOrigin.RotationQ[1] += 1;
		TestTrue(TEXT("one unit of origin yaw reconciles"), Base.ShouldReconcile(Other));
	}
	{
		FGSTN_CombatSyncState Other = MakeAttackerWithHit();
		Other.Core.Hit.AddHitTarget(8, HitTimelineStart, 1);
		TestTrue(TEXT("another target reconciles"), Base.ShouldReconcile(Other));
		Other = MakeAttackerWithHit();
		Other.Core.Hit.HitConfirmFlags = 0x1;
		TestTrue(TEXT("another confirm flag reconciles"), Base.ShouldReconcile(Other));
	}
	{
		FGSTN_CombatSyncState Other = MakeAttackerWithHit();
		Other.Core.Hit.AddHitTarget(99, HitTimelineStart - 10, 0);
		TestFalse(TEXT("a target of an earlier timeline run is not a difference"), Base.ShouldReconcile(Other));
	}

	const FGSTN_CombatSyncState Victim = MakeVictimWithHit();
	{
		FGSTN_CombatSyncState Other = MakeVictimWithHit();
		Other.Core.Hit.HitstunEndFrame += 1;
		TestTrue(TEXT("another hitstun end reconciles"), Victim.ShouldReconcile(Other));
		Other = MakeVictimWithHit();
		Other.Core.Hit.PendingLaunchQ[2] += 1;
		TestTrue(TEXT("one quantum of launch reconciles"), Victim.ShouldReconcile(Other));
		Other = MakeVictimWithHit();
		Other.Core.Hit.SetPendingReaction(EGSTN_HitReaction::PushBack);
		TestTrue(TEXT("another pending reaction reconciles"), Victim.ShouldReconcile(Other));
		Other = MakeVictimWithHit();
		Other.Core.Hit.SDIDistanceUsed = 1;
		TestTrue(TEXT("a set SDI count reconciles"), Victim.ShouldReconcile(Other));
		Other = MakeVictimWithHit();
		Other.Core.ActionState = EGSTN_ActionState::Hitstun;
		TestTrue(TEXT("another action state reconciles"), Victim.ShouldReconcile(Other));
	}
	{
		// A launch velocity that rounds to the same quantum is the same state: no float equality anywhere
		FGSTN_CombatSyncState A = MakeVictimWithHit();
		FGSTN_CombatSyncState B = MakeVictimWithHit();
		A.Core.Hit.SetPendingLaunch(FVector(900.0, -300.0, 1500.0));
		B.Core.Hit.SetPendingLaunch(FVector(901.0, -299.0, 1501.0));
		TestFalse(TEXT("velocities inside one 4 cm/s quantum do not reconcile"), A.ShouldReconcile(B));
	}
	{
		// The pending launch of a hitstop that ended is dead, so a leftover value on one side is not a difference
		FGSTN_CombatSyncState A = MakeVictimWithHit();
		FGSTN_CombatSyncState B = MakeVictimWithHit();
		A.Core.HitstopEndFrame = HitStateFrame;
		B.Core.HitstopEndFrame = HitStateFrame;
		B.Core.Hit.SetPendingLaunch(FVector(4.0, 4.0, 4.0));
		TestFalse(TEXT("a leftover launch after the hitstop is not a difference"), A.ShouldReconcile(B));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
