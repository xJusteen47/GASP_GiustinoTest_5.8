// Automation tests of the lag compensation of the attacker side (combat-m4-hits 9, M4 H10): the per-attacker overlay, the history ring
// that rewinds the reaction state with the transform, who rewinds and how far. No world: the subsystem is a plain object and the
// entities are stand-ins that implement the snapshot source interface.

#include "Misc/AutomationTest.h"
#include "Presentation/GSTN_HitPresentation.h"
#include "Rules/GSTN_CombatRules.h"
#include "Simulation/GSTN_LagCompensation.h"
#include "World/GSTN_CombatWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	constexpr float LagStepMs = 16.f;

	/** A victim whose position, damage percent and flags a test sets before each frame */
	struct FLagFakeSource : IGSTN_SnapshotSource
	{
		FVector Location = FVector::ZeroVector;
		double Percent = 0.0;
		uint8 Flags = 0;

		virtual bool ReadSnapshotEntry(int32 Frame, FGSTN_SnapshotEntry& Out) override
		{
			Out.Transform = FTransform(Location);
			Out.Velocity = FVector(Location.X, 0.0, 0.0);
			Out.Reaction = FGSTN_VictimReactionState::Make(Percent, 1.0, Flags, FGSTN_StaggerThreshold());
			Out.bInHitstop = (Flags & GSTNVictimReactionFlags::InHitstop) != 0;
			return true;
		}
	};

	constexpr int32 AttackerId = 1;
	constexpr int32 VictimId = 2;

	/** Frames FirstFrame..LastFrame: the victim at x = frame * 10, percent = frame * 10, in hitstop from HitstopFrom on. Time = frame * 16 ms */
	void PlayFrames(UGSTN_CombatWorldSubsystem& Subsystem, FLagFakeSource& Victim, int32 FirstFrame, int32 LastFrame, int32 HitstopFrom = INT32_MAX)
	{
		for (int32 Frame = FirstFrame; Frame <= LastFrame; ++Frame)
		{
			Victim.Location = FVector(Frame * 10.0, 0.0, 0.0);
			Victim.Percent = Frame * 10.0;
			Victim.Flags = Frame >= HitstopFrom ? GSTNVictimReactionFlags::InHitstop : 0;
			Subsystem.EnsureSnapshot(Frame, Frame * LagStepMs, LagStepMs);
		}
	}

	bool NoPose(int32 EntityId, FTransform& OutPose)
	{
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LagCompOverlayNotSharedTest, "GSTNCombat.NP.Hits.LagComp.OverlayNotSharedSnapshot", GSTN_NP_TEST_FLAGS)
bool FGSTN_LagCompOverlayNotSharedTest::RunTest(const FString& Parameters)
{
	UGSTN_CombatWorldSubsystem* Subsystem = NewObject<UGSTN_CombatWorldSubsystem>(GetTransientPackage());
	FLagFakeSource Attacker, Victim;
	Subsystem->RegisterEntity(AttackerId, Attacker, nullptr, FGenericTeamId(1), nullptr);
	Subsystem->RegisterEntity(VictimId, Victim, nullptr, FGenericTeamId(2), nullptr);
	Subsystem->RequestHistory(250);
	PlayFrames(*Subsystem, Victim, 10, 14);

	const FGSTN_StepSnapshot& Shared = Subsystem->GetSnapshot();
	const FGSTN_SnapshotEntry* VictimNow = Shared.Find(VictimId);
	TestNotNull(TEXT("the shared snapshot holds the victim"), VictimNow);
	if (!VictimNow)
	{
		return false;
	}
	TestEqual(TEXT("the shared snapshot is the present: x"), VictimNow->Transform.GetLocation().X, 140.0);

	FGSTN_StepSnapshot Overlay;
	GSTNLagComp::BuildOverlay(Shared, AttackerId, 10 * LagStepMs, *Subsystem, NoPose, Overlay);

	const FGSTN_SnapshotEntry* VictimThen = Overlay.Find(VictimId);
	TestNotNull(TEXT("the overlay holds the victim"), VictimThen);
	if (VictimThen)
	{
		TestEqual(TEXT("the overlay shows the victim where it was"), VictimThen->Transform.GetLocation().X, 100.0);
		TestEqual(TEXT("and its damage percent then"), VictimThen->Reaction.GetDamagePercent(), 100.f);
	}
	// The shared snapshot, which every other attacker and every victim step reads, did not move
	TestEqual(TEXT("the shared snapshot is untouched: x"), Subsystem->GetSnapshot().Find(VictimId)->Transform.GetLocation().X, 140.0);
	TestEqual(TEXT("the shared snapshot is untouched: percent"), Subsystem->GetSnapshot().Find(VictimId)->Reaction.GetDamagePercent(), 140.f);
	TestEqual(TEXT("no snapshot was rebuilt"), Subsystem->GetSnapshotBuildCount(), 5);

	// The attacker's own entry is its present: only the others are in the past
	TestEqual(TEXT("the attacker's own entry is the shared one"), Overlay.Find(AttackerId)->Transform.GetLocation().X, Shared.Find(AttackerId)->Transform.GetLocation().X);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LagCompReactionRewoundTest, "GSTNCombat.NP.Hits.LagComp.ReactionStateRewoundWithTransform", GSTN_NP_TEST_FLAGS)
bool FGSTN_LagCompReactionRewoundTest::RunTest(const FString& Parameters)
{
	UGSTN_CombatWorldSubsystem* Subsystem = NewObject<UGSTN_CombatWorldSubsystem>(GetTransientPackage());
	FLagFakeSource Attacker, Victim;
	Subsystem->RegisterEntity(AttackerId, Attacker, nullptr, FGenericTeamId(1), nullptr);
	Subsystem->RegisterEntity(VictimId, Victim, nullptr, FGenericTeamId(2), nullptr);
	Subsystem->RequestHistory(250);
	// The victim enters hitstop on frame 13
	PlayFrames(*Subsystem, Victim, 10, 15, 13);
	const FGSTN_StepSnapshot& Shared = Subsystem->GetSnapshot();

	FGSTN_StepSnapshot Before;
	GSTNLagComp::BuildOverlay(Shared, AttackerId, 12 * LagStepMs, *Subsystem, NoPose, Before);
	FGSTN_StepSnapshot After;
	GSTNLagComp::BuildOverlay(Shared, AttackerId, 13 * LagStepMs, *Subsystem, NoPose, After);

	TestFalse(TEXT("frame 12: not in hitstop"), Before.Find(VictimId)->bInHitstop);
	TestFalse(TEXT("frame 12: the reaction flag says so too"), Before.Find(VictimId)->Reaction.HasFlag(GSTNVictimReactionFlags::InHitstop));
	TestEqual(TEXT("frame 12: percent"), Before.Find(VictimId)->Reaction.GetDamagePercent(), 120.f);
	TestEqual(TEXT("frame 12: x"), Before.Find(VictimId)->Transform.GetLocation().X, 120.0);
	TestTrue(TEXT("frame 13: in hitstop"), After.Find(VictimId)->bInHitstop);
	TestEqual(TEXT("frame 13: percent"), After.Find(VictimId)->Reaction.GetDamagePercent(), 130.f);
	TestEqual(TEXT("frame 13: x"), After.Find(VictimId)->Transform.GetLocation().X, 130.0);

	// Between two frames the transform blends and the discrete state keeps the earlier frame's value
	FGSTN_StepSnapshot Between;
	GSTNLagComp::BuildOverlay(Shared, AttackerId, 12.5f * LagStepMs, *Subsystem, NoPose, Between);
	TestEqual(TEXT("between 12 and 13: x blends"), Between.Find(VictimId)->Transform.GetLocation().X, 125.0, 0.01);
	TestEqual(TEXT("between 12 and 13: the percent is frame 12's"), Between.Find(VictimId)->Reaction.GetDamagePercent(), 120.f);
	TestFalse(TEXT("between 12 and 13: still not in hitstop"), Between.Find(VictimId)->bInHitstop);

	// The pose the fork's rewind gives wins over the ring's blend; the reaction still comes from the ring
	FGSTN_StepSnapshot Provided;
	GSTNLagComp::BuildOverlay(Shared, AttackerId, 13 * LagStepMs, *Subsystem, [](int32 Id, FTransform& Out) { Out = FTransform(FVector(999.0, 0.0, 0.0)); return Id == VictimId; }, Provided);
	TestEqual(TEXT("the provided pose is used"), Provided.Find(VictimId)->Transform.GetLocation().X, 999.0);
	TestTrue(TEXT("and the reaction is still the ring's"), Provided.Find(VictimId)->bInHitstop);
	TestEqual(TEXT("the attacker is never given a provided pose"), Provided.Find(AttackerId)->Transform.GetLocation().X, Shared.Find(AttackerId)->Transform.GetLocation().X);

	// A time before the ring is the oldest record
	FGSTN_StepSnapshot Early;
	GSTNLagComp::BuildOverlay(Shared, AttackerId, 0.f, *Subsystem, NoPose, Early);
	TestEqual(TEXT("before the ring: the oldest record"), Early.Find(VictimId)->Transform.GetLocation().X, 100.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LagCompHostNeverRewindsTest, "GSTNCombat.NP.Hits.LagComp.HostNeverRewinds", GSTN_NP_TEST_FLAGS)
bool FGSTN_LagCompHostNeverRewindsTest::RunTest(const FString& Parameters)
{
	UGSTN_CombatRules* Rules = NewObject<UGSTN_CombatRules>(GetTransientPackage());
	Rules->bUseLagCompensation = true;
	Rules->MaxRewindMs = 250;

	TestTrue(TEXT("a remote player's pawn on the authority rewinds"), GSTNLagComp::ShouldRewind(ROLE_Authority, ROLE_AutonomousProxy, Rules));
	// The listen-server host's own pawn and an AI pawn are authority with a simulated remote role: they hit the present they show
	TestFalse(TEXT("the host pawn does not"), GSTNLagComp::ShouldRewind(ROLE_Authority, ROLE_SimulatedProxy, Rules));
	TestFalse(TEXT("a pawn that replicates nowhere does not"), GSTNLagComp::ShouldRewind(ROLE_Authority, ROLE_None, Rules));
	TestFalse(TEXT("a predicting client does not (its view is already the interpolated one)"), GSTNLagComp::ShouldRewind(ROLE_AutonomousProxy, ROLE_Authority, Rules));
	TestFalse(TEXT("a simulated proxy does not tick"), GSTNLagComp::ShouldRewind(ROLE_SimulatedProxy, ROLE_Authority, Rules));
	TestFalse(TEXT("no rules, no rewind"), GSTNLagComp::ShouldRewind(ROLE_Authority, ROLE_AutonomousProxy, nullptr));

	Rules->bUseLagCompensation = false;
	TestFalse(TEXT("rules that switch it off"), GSTNLagComp::ShouldRewind(ROLE_Authority, ROLE_AutonomousProxy, Rules));
	Rules->bUseLagCompensation = true;
	Rules->MaxRewindMs = 0;
	TestFalse(TEXT("a window of 0 ms rewinds nothing"), GSTNLagComp::ShouldRewind(ROLE_Authority, ROLE_AutonomousProxy, Rules));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LagCompCapAtMaxRewindTest, "GSTNCombat.NP.Hits.LagComp.CapAtMaxRewind", GSTN_NP_TEST_FLAGS)
bool FGSTN_LagCompCapAtMaxRewindTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("inside the window the request stands"), GSTNLagComp::ClampRewindTime(1000.f, 900.f, 250), 900.f);
	TestEqual(TEXT("at the edge of the window"), GSTNLagComp::ClampRewindTime(1000.f, 750.f, 250), 750.f);
	TestEqual(TEXT("older than the window: the oldest the rules allow"), GSTNLagComp::ClampRewindTime(1000.f, 100.f, 250), 750.f);
	TestEqual(TEXT("a request in the future is the present"), GSTNLagComp::ClampRewindTime(1000.f, 1200.f, 250), 1000.f);
	TestEqual(TEXT("a window of 0 is the present"), GSTNLagComp::ClampRewindTime(1000.f, 500.f, 0), 1000.f);

	// The ring holds MaxRewindMs of frames (plus a margin), not more
	UGSTN_CombatWorldSubsystem* Subsystem = NewObject<UGSTN_CombatWorldSubsystem>(GetTransientPackage());
	FLagFakeSource Victim;
	Subsystem->RegisterEntity(VictimId, Victim, nullptr, FGenericTeamId(2), nullptr);
	TestFalse(TEXT("no history is kept until a rewinding rules asset asks"), Subsystem->IsRecordingHistory());
	PlayFrames(*Subsystem, Victim, 1, 5);
	TestEqual(TEXT("nothing recorded before the request"), Subsystem->GetNumHistoryRecords(VictimId), 0);

	Subsystem->RequestHistory(250);
	PlayFrames(*Subsystem, Victim, 6, 200);
	const int32 Expected = FMath::CeilToInt32(250.f / LagStepMs) + 8;
	TestEqual(TEXT("the ring is bounded by the window"), Subsystem->GetNumHistoryRecords(VictimId), Expected);

	// A re-simulated frame replaces what was written from it on
	Subsystem->InvalidateSnapshot();
	PlayFrames(*Subsystem, Victim, 190, 190);
	FGSTN_EntityHistoryRecord Newest;
	TestTrue(TEXT("the newest record is readable"), Subsystem->SampleHistory(VictimId, 1.0e9f, Newest, false));
	TestEqual(TEXT("a rollback dropped the frames after it"), Newest.Frame, 190);
	TestEqual(TEXT("and kept the ring bounded"), Subsystem->GetNumHistoryRecords(VictimId) <= Expected, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LagCompPresentationCancelledTest, "GSTNCombat.NP.Hits.LagComp.PresentationCancelledOnCorrection", GSTN_NP_TEST_FLAGS)
bool FGSTN_LagCompPresentationCancelledTest::RunTest(const FString& Parameters)
{
	FGSTN_HitPresentationTracker Tracker;
	FGSTN_HitPresentationTracker::FDelta Delta;
	FGSTN_HitSyncState Hit;

	Tracker.Update(Hit, true, 10, Delta);
	TestEqual(TEXT("nothing hit: nothing started"), Delta.Started.Num(), 0);

	Hit.AddHitTarget(VictimId, 10, 0);
	Tracker.Update(Hit, true, 10, Delta);
	TestEqual(TEXT("a new target starts one effect"), Delta.Started.Num(), 1);
	Tracker.Update(Hit, true, 10, Delta);
	TestEqual(TEXT("and only once"), Delta.Started.Num(), 0);

	// The authority did not confirm the hit: the corrected state has no target while the same move still runs
	FGSTN_HitSyncState Corrected;
	Tracker.Update(Corrected, true, 10, Delta);
	TestEqual(TEXT("the vanished target cancels its effect"), Delta.Cancelled.Num(), 1);
	TestEqual(TEXT("nothing is left active"), Tracker.GetActive().Num(), 0);

	// A move that ends takes its targets with it: not a correction
	Tracker.Update(Hit, true, 10, Delta);
	Tracker.Update(Corrected, false, 10, Delta);
	TestEqual(TEXT("the end of the move cancels nothing"), Delta.Cancelled.Num(), 0);
	// Nor does another move starting
	Tracker.Update(Hit, true, 10, Delta);
	Tracker.Update(Corrected, true, 40, Delta);
	TestEqual(TEXT("a new move cancels nothing"), Delta.Cancelled.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LagCompCueDedupTest, "GSTNCombat.NP.Hits.LagComp.CueDedupByHitId", GSTN_NP_TEST_FLAGS)
bool FGSTN_LagCompCueDedupTest::RunTest(const FString& Parameters)
{
	const uint32 Id = GSTNHitPresentation::MakeHitId(100, 200, 3, 1);
	TestEqual(TEXT("the id is stable"), Id, GSTNHitPresentation::MakeHitId(100, 200, 3, 1));
	TestNotEqual(TEXT("another victim, another id"), Id, GSTNHitPresentation::MakeHitId(100, 201, 3, 1));
	TestNotEqual(TEXT("another hitbox, another id"), Id, GSTNHitPresentation::MakeHitId(100, 200, 3, 2));

	FGSTN_HitCueDedup Dedup;
	TestFalse(TEXT("no local cue: the real one plays"), Dedup.ConsumeLocal(Id));
	Dedup.NoteLocal(Id, 10);
	TestFalse(TEXT("another hit's real cue plays"), Dedup.ConsumeLocal(Id + 1));
	TestTrue(TEXT("the real cue of a hit already played locally is skipped"), Dedup.ConsumeLocal(Id));
	TestFalse(TEXT("once"), Dedup.ConsumeLocal(Id));

	Dedup.NoteLocal(Id, 10);
	Dedup.Forget(Id);
	TestFalse(TEXT("a cancelled prediction leaves the real cue alone"), Dedup.ConsumeLocal(Id));

	Dedup.NoteLocal(Id, 10);
	Dedup.Prune(10 + FGSTN_HitCueDedup::MaxAgeFrames + 1);
	TestEqual(TEXT("old records are forgotten"), Dedup.Num(), 0);
	return true;
}

#endif
