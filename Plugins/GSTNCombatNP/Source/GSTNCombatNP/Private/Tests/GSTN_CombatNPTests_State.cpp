// Automation tests of the action part of the combat sync state (combat-m3 7.1): wire format, requests, reconcile and the
// loadout revision gate. They need no world: the state is plain data and the archives are bit archives.

#include "Misc/AutomationTest.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Jump/GSTN_JumpLogic.h"
#include "Simulation/GSTN_CombatSimTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	FGSTN_CombatSyncState MakeBusyState()
	{
		FGSTN_CombatSyncState State;
		State.Core.ActionState = EGSTN_ActionState::InTimeline;
		State.Core.ActiveTimeline = 12;
		State.Core.ActiveSource = EGSTN_ActivationSource::Adopted;
		State.Core.TimelineStartFrame = 400;
		State.Core.TimelinePausedFrames = 6;
		State.Core.PausedFramesTotal = 30;
		State.Core.ActiveWeaponIndex = 2;
		State.Core.LoadoutRevision = 7;
		State.Core.Requests.Add(EGSTN_RequestType::ActivateEntry, 410, 3);
		State.Core.Requests.Add(EGSTN_RequestType::CancelAbility, 410, 99);
		State.Core.Jump = FGSTN_JumpLogic::ApplyTakeoff(FGSTN_JumpState(), 405, 5, EGSTN_JumpKind::Air);
		return State;
	}

	int64 BitsOfState(FGSTN_CombatSyncState& State)
	{
		FBitWriter Writer(512, true);
		State.NetSerialize(FNetSerializeParams(Writer));
		return Writer.GetNumBits();
	}

	FGSTN_CombatSyncState ReadBackState(FGSTN_CombatSyncState& State, bool& bOutError)
	{
		FBitWriter Writer(512, true);
		State.NetSerialize(FNetSerializeParams(Writer));
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_CombatSyncState Loaded;
		Loaded.NetSerialize(FNetSerializeParams(Reader));
		bOutError = Reader.IsError() || !Reader.AtEnd();
		return Loaded;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StateRoundTripTest, "GSTNCombat.NP.State.RoundTrip", GSTN_NP_TEST_FLAGS)

bool FGSTN_StateRoundTripTest::RunTest(const FString& Parameters)
{
	// An idle combatant that never had a loadout costs two bits more than the part that existed before: the action part and the jump state
	FGSTN_CombatSyncState Idle;
	FBitWriter CoreWriter(512, true);
	Idle.Core.Input.NetSerialize(CoreWriter);
	int32 Start = INDEX_NONE;
	CoreWriter << Start;
	CoreWriter << Start;
	TestTrue(TEXT("the default action part is default"), Idle.Core.IsActionPartDefault());
	TestEqual(TEXT("an idle combatant pays one bit for the whole action part and one for the jump state"), BitsOfState(Idle) - 1 - CoreWriter.GetNumBits(), static_cast<int64>(2));

	bool bError = false;
	const FGSTN_CombatSyncState IdleLoaded = ReadBackState(Idle, bError);
	TestFalse(TEXT("an idle state reads back"), bError);
	TestTrue(TEXT("and is idle"), IdleLoaded.Core.IsActionPartDefault());

	// A busy one keeps every field, in the same order of requests
	FGSTN_CombatSyncState Busy = MakeBusyState();
	TestFalse(TEXT("the busy action part is not default"), Busy.Core.IsActionPartDefault());
	const FGSTN_CombatSyncState Loaded = ReadBackState(Busy, bError);
	TestFalse(TEXT("a busy state reads back to the end"), bError);
	TestTrue(TEXT("action state"), Loaded.Core.ActionState == EGSTN_ActionState::InTimeline);
	TestTrue(TEXT("jump state"), Loaded.Core.Jump == Busy.Core.Jump && Loaded.Core.Jump.AirJumpsUsed == 1);
	TestEqual(TEXT("timeline"), static_cast<int32>(Loaded.Core.ActiveTimeline), 12);
	TestTrue(TEXT("source"), Loaded.Core.ActiveSource == EGSTN_ActivationSource::Adopted);
	TestEqual(TEXT("start frame"), Loaded.Core.TimelineStartFrame, 400);
	TestEqual(TEXT("paused frames"), Loaded.Core.TimelinePausedFrames, 6);
	TestEqual(TEXT("paused total"), Loaded.Core.PausedFramesTotal, 30);
	TestEqual(TEXT("weapon"), static_cast<int32>(Loaded.Core.ActiveWeaponIndex), 2);
	TestEqual(TEXT("revision"), static_cast<int32>(Loaded.Core.LoadoutRevision), 7);
	TestTrue(TEXT("requests"), Loaded.Core.Requests == Busy.Core.Requests);
	TestFalse(TEXT("no reconcile against itself"), Loaded.ShouldReconcile(Busy));

	// A timeline with no entry (INDEX_NONE) while busy in another way still round-trips the sentinel
	FGSTN_CombatSyncState NoTimeline = MakeBusyState();
	NoTimeline.Core.ActiveTimeline = INDEX_NONE;
	NoTimeline.Core.TimelineStartFrame = INDEX_NONE;
	const FGSTN_CombatSyncState NoTimelineLoaded = ReadBackState(NoTimeline, bError);
	TestFalse(TEXT("reads back"), bError);
	TestEqual(TEXT("INDEX_NONE timeline"), static_cast<int32>(NoTimelineLoaded.Core.ActiveTimeline), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("INDEX_NONE start"), NoTimelineLoaded.Core.TimelineStartFrame, static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StateRequestsStaleDroppedTest, "GSTNCombat.NP.State.RequestsStaleDropped", GSTN_NP_TEST_FLAGS)

bool FGSTN_StateRequestsStaleDroppedTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatRequests Requests;
	TestTrue(TEXT("add"), Requests.Add(EGSTN_RequestType::ActivateEntry, 100, 1));
	TestTrue(TEXT("add a second"), Requests.Add(EGSTN_RequestType::CommitTimeline, 101, 0));
	TestTrue(TEXT("and a third"), Requests.Add(EGSTN_RequestType::Cue, 101, 5));
	TestEqual(TEXT("three set"), Requests.Num(), 3);

	// The top of frame 101 drops what was made before it and packs the rest
	Requests.PruneBefore(101);
	TestEqual(TEXT("one request is dead"), Requests.Num(), 2);
	TestTrue(TEXT("the survivors are packed to the front"), Requests.Slots[0].Type == EGSTN_RequestType::CommitTimeline && Requests.Slots[1].Type == EGSTN_RequestType::Cue);
	TestTrue(TEXT("the dead one is not found"), Requests.Find(EGSTN_RequestType::ActivateEntry, 100) == nullptr);
	TestTrue(TEXT("a live one is"), Requests.Find(EGSTN_RequestType::Cue, 101) != nullptr);

	// What was dropped never reaches the wire
	FGSTN_CombatSyncState State;
	State.Core.Requests = Requests;
	bool bError = false;
	const FGSTN_CombatSyncState Loaded = ReadBackState(State, bError);
	TestFalse(TEXT("reads back"), bError);
	TestEqual(TEXT("two requests on the other side"), Loaded.Core.Requests.Num(), 2);

	// A full array refuses the next request instead of overwriting one
	FGSTN_CombatRequests Full;
	for (int32 Index = 0; Index < FGSTN_CombatRequests::Capacity; ++Index)
	{
		TestTrue(TEXT("fills"), Full.Add(EGSTN_RequestType::Cue, 200, static_cast<int16>(Index)));
	}
	TestFalse(TEXT("the fifth is refused"), Full.Add(EGSTN_RequestType::Cue, 200, 9));
	TestEqual(TEXT("and nothing was overwritten"), static_cast<int32>(Full.Slots[0].Arg), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StateInterpolateTest, "GSTNCombat.NP.State.Interpolate", GSTN_NP_TEST_FLAGS)

bool FGSTN_StateInterpolateTest::RunTest(const FString& Parameters)
{
	// The action part is discrete: an interpolated proxy takes the newer frame
	FGSTN_CombatSyncState From;
	FGSTN_CombatSyncState To = MakeBusyState();
	FGSTN_CombatSyncState Out;
	Out.Interpolate(&From, &To, 0.5f);
	TestTrue(TEXT("the newer action part"), Out.Core.ActionState == EGSTN_ActionState::InTimeline && Out.Core.ActiveTimeline == 12);
	TestTrue(TEXT("requests included"), Out.Core.Requests == To.Core.Requests);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StateShouldReconcileTest, "GSTNCombat.NP.State.ShouldReconcile", GSTN_NP_TEST_FLAGS)

bool FGSTN_StateShouldReconcileTest::RunTest(const FString& Parameters)
{
	const FGSTN_CombatSyncState Authority = MakeBusyState();
	FGSTN_CombatSyncState Local = Authority;
	TestFalse(TEXT("equal states do not reconcile"), Local.ShouldReconcile(Authority));

	auto Differs = [&](const TCHAR* What, TFunctionRef<void(FGSTN_CombatSyncState&)> Change)
	{
		FGSTN_CombatSyncState Changed = Authority;
		Change(Changed);
		TestTrue(What, Changed.ShouldReconcile(Authority));
	};
	Differs(TEXT("action state"), [](FGSTN_CombatSyncState& S) { S.Core.ActionState = EGSTN_ActionState::Actionable; });
	Differs(TEXT("active timeline"), [](FGSTN_CombatSyncState& S) { S.Core.ActiveTimeline = 13; });
	Differs(TEXT("source"), [](FGSTN_CombatSyncState& S) { S.Core.ActiveSource = EGSTN_ActivationSource::Forced; });
	Differs(TEXT("start frame"), [](FGSTN_CombatSyncState& S) { S.Core.TimelineStartFrame = 401; });
	Differs(TEXT("paused frames"), [](FGSTN_CombatSyncState& S) { S.Core.TimelinePausedFrames = 7; });
	Differs(TEXT("paused total"), [](FGSTN_CombatSyncState& S) { S.Core.PausedFramesTotal = 31; });
	Differs(TEXT("weapon"), [](FGSTN_CombatSyncState& S) { S.Core.ActiveWeaponIndex = 1; });
	Differs(TEXT("loadout revision"), [](FGSTN_CombatSyncState& S) { S.Core.LoadoutRevision = 8; });
	Differs(TEXT("requests"), [](FGSTN_CombatSyncState& S) { S.Core.Requests.PruneBefore(411); });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StateRevisionGateTest, "GSTNCombat.NP.State.RevisionGate", GSTN_NP_TEST_FLAGS)

bool FGSTN_StateRevisionGateTest::RunTest(const FString& Parameters)
{
	// Nothing applied: both sides are at revision 0 and the gate is open, so nothing changes for a game without loadouts
	FGSTN_CombatSyncState State;
	TestTrue(TEXT("revision 0 matches an empty table"), State.Core.IsLoadoutCurrent(0));

	// The server wrote revision 3 out of band; a client whose table is still at 2 stays gated until the table arrives
	State.Core.LoadoutRevision = 3;
	TestFalse(TEXT("the table is behind: the gate is closed"), State.Core.IsLoadoutCurrent(2));
	TestTrue(TEXT("the table caught up: open"), State.Core.IsLoadoutCurrent(3));

	// The revision travels with the state, so the client learns it from the next correction
	bool bError = false;
	const FGSTN_CombatSyncState Loaded = ReadBackState(State, bError);
	TestFalse(TEXT("reads back"), bError);
	TestEqual(TEXT("the revision arrived"), static_cast<int32>(Loaded.Core.LoadoutRevision), 3);

	// A difference in the revision alone is a reconcile: the server's value wins
	FGSTN_CombatSyncState Stale;
	TestTrue(TEXT("a client that never heard of revision 3 reconciles"), Stale.ShouldReconcile(State));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
