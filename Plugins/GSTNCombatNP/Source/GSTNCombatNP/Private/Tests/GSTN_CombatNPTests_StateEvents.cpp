// Automation tests of the Blueprint combat state and events (bp-exposure P1): the state view read from the combat core, the watcher that finds
// the state events (no double fire for a state seen again, as after a re-simulation) and the typed payloads of the confirmed events.
// No world: the view, the watcher and the payload builders are pure.

#include "Misc/AutomationTest.h"
#include "Events/GSTN_SimEvent.h"
#include "GSTN_CombatNPTags.h"
#include "GSTN_CombatTags.h"
#include "Presentation/GSTN_CombatStateTypes.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatVictim.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	FGSTN_CombatSyncCore StateEvCore(int32 Frame)
	{
		FGSTN_CombatSyncCore Core;
		FGSTN_InputSample Sample;
		Sample.Frame = Frame;
		Core.Input.History.Append(Sample);
		return Core;
	}

	FGSTN_CombatStateView StateEvView(int32 Frame)
	{
		return FGSTN_CombatStateView::FromCore(StateEvCore(Frame));
	}

	int32 StateEvCount(const TArray<EGSTN_CombatStateChange>& Changes, EGSTN_CombatStateChange Change)
	{
		int32 Count = 0;
		for (const EGSTN_CombatStateChange Each : Changes)
		{
			Count += Each == Change ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StateViewFromCoreTest, "GSTNCombat.NP.StateEvents.ViewFromCore", GSTN_NP_TEST_FLAGS)
bool FGSTN_StateViewFromCoreTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("no frame yet: invalid"), FGSTN_CombatStateView::FromCore(FGSTN_CombatSyncCore()).bValid);

	FGSTN_CombatSyncCore Core = StateEvCore(100);
	Core.ActionState = EGSTN_ActionState::Hitstun;
	Core.Hit.HitstunEndFrame = 112;
	Core.HitstopStartFrame = 98;
	Core.HitstopEndFrame = 104;
	FGSTN_CombatStateView View = FGSTN_CombatStateView::FromCore(Core);
	TestTrue(TEXT("valid"), View.bValid);
	TestTrue(TEXT("status"), View.Status == EGSTN_CombatStatus::Hitstun);
	TestEqual(TEXT("hitstun left"), View.HitstunFramesLeft, 12);
	TestEqual(TEXT("hitstop left"), View.HitstopFramesLeft, 4);
	TestTrue(TEXT("in hitstun and hitstop"), View.bInHitstun && View.bInHitstop);

	Core.ActionState = EGSTN_ActionState::Tumble;
	Core.bTumbleActionable = true;
	View = FGSTN_CombatStateView::FromCore(Core);
	TestTrue(TEXT("an actionable tumble is a tumble, not hitstun"), View.bInTumble && View.bTumbleActionable && !View.bInHitstun);

	Core = StateEvCore(200);
	Core.ActionState = EGSTN_ActionState::InTimeline;
	Core.ActiveTimeline = 2;
	Core.TimelineStartFrame = 190;
	Core.TimelinePausedFrames = 3;
	View = FGSTN_CombatStateView::FromCore(Core);
	TestTrue(TEXT("in a move"), View.bInMove && View.Status == EGSTN_CombatStatus::InMove);
	TestEqual(TEXT("move frame excludes the hitstop frames"), View.MoveFrame, 7);
	TestEqual(TEXT("move start"), View.MoveStartFrame, 190);

	Core = StateEvCore(300);
	Core.Jump.Phase = EGSTN_JumpPhase::Falling;
	Core.Jump.CoyoteFramesLeft = 5;
	View = FGSTN_CombatStateView::FromCore(Core);
	TestTrue(TEXT("falling is airborne"), View.bAirborne && View.AirState == EGSTN_CombatAirState::Falling);
	TestEqual(TEXT("coyote"), View.CoyoteFramesLeft, 5);

	Core = StateEvCore(400);
	Core.ActionState = EGSTN_ActionState::KO;
	TestTrue(TEXT("knocked out"), FGSTN_CombatStateView::FromCore(Core).bKnockedOut);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StateWatcherEdgesTest, "GSTNCombat.NP.StateEvents.WatcherEdges", GSTN_NP_TEST_FLAGS)
bool FGSTN_StateWatcherEdgesTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatStateWatcher Watcher;
	TArray<EGSTN_CombatStateChange> Changes;

	FGSTN_CombatStateView Air = StateEvView(10);
	Air.bAirborne = true;
	Watcher.Update(Air, Changes);
	TestEqual(TEXT("the first call only primes"), Changes.Num(), 0);

	FGSTN_CombatStateView Ground = StateEvView(11);
	Watcher.Update(Ground, Changes);
	TestEqual(TEXT("landed once"), StateEvCount(Changes, EGSTN_CombatStateChange::Landed), 1);
	TestEqual(TEXT("frames since landed"), Watcher.GetFramesSinceLanded(14), 3);

	// The same presented state again (a re-simulation that ended where the prediction was): nothing fires twice
	Changes.Reset();
	Watcher.Update(StateEvView(12), Changes);
	TestEqual(TEXT("no change, no event"), Changes.Num(), 0);

	// A move, then the same move replaced by itself (new start frame): an end and a start
	FGSTN_CombatStateView Move = StateEvView(13);
	Move.bInMove = true;
	Move.MoveEntry = 1;
	Move.MoveStartFrame = 13;
	Watcher.Update(Move, Changes);
	TestEqual(TEXT("move started"), StateEvCount(Changes, EGSTN_CombatStateChange::MoveStarted), 1);
	Changes.Reset();
	Move.Frame = 14;
	Watcher.Update(Move, Changes);
	TestEqual(TEXT("same run, nothing"), Changes.Num(), 0);
	Move.Frame = 15;
	Move.MoveStartFrame = 15;
	Watcher.Update(Move, Changes);
	TestTrue(TEXT("replaced by itself: ended then started"), Changes.Num() == 2 && Changes[0] == EGSTN_CombatStateChange::MoveEnded
		&& Changes[1] == EGSTN_CombatStateChange::MoveStarted);

	// Hit into tumble, then out
	Changes.Reset();
	FGSTN_CombatStateView Tumble = StateEvView(16);
	Tumble.bInTumble = true;
	Tumble.bInHitstun = true;
	Tumble.bAirborne = true;
	Watcher.Update(Tumble, Changes);
	TestEqual(TEXT("move ended by the hit"), StateEvCount(Changes, EGSTN_CombatStateChange::MoveEnded), 1);
	TestEqual(TEXT("tumble started"), StateEvCount(Changes, EGSTN_CombatStateChange::TumbleStarted), 1);
	TestEqual(TEXT("hitstun started"), StateEvCount(Changes, EGSTN_CombatStateChange::HitstunStarted), 1);
	TestEqual(TEXT("left ground"), StateEvCount(Changes, EGSTN_CombatStateChange::LeftGround), 1);
	TestEqual(TEXT("airborne: no landing frame"), Watcher.GetFramesSinceLanded(16), (int32)INDEX_NONE);
	Changes.Reset();
	Watcher.Update(StateEvView(30), Changes);
	TestEqual(TEXT("tumble ended"), StateEvCount(Changes, EGSTN_CombatStateChange::TumbleEnded), 1);
	TestEqual(TEXT("hitstun ended"), StateEvCount(Changes, EGSTN_CombatStateChange::HitstunEnded), 1);
	TestEqual(TEXT("landed"), StateEvCount(Changes, EGSTN_CombatStateChange::Landed), 1);

	// Run stages: a change only between two states that have them
	Changes.Reset();
	FGSTN_CombatStateView Run = StateEvView(31);
	Run.bHasRunStages = true;
	Run.RunStage = 0;
	Watcher.Update(Run, Changes);
	TestEqual(TEXT("first state with stages: no change"), Changes.Num(), 0);
	Run.RunStage = 2;
	Watcher.Update(Run, Changes);
	TestEqual(TEXT("stage changed"), StateEvCount(Changes, EGSTN_CombatStateChange::RunStageChanged), 1);
	TestEqual(TEXT("previous holds the new stage"), Watcher.GetPrevious().RunStage, 2);

	// An invalid view (no state yet) is ignored, it does not end anything
	Changes.Reset();
	Watcher.Update(FGSTN_CombatStateView(), Changes);
	TestEqual(TEXT("invalid ignored"), Changes.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StateEventPayloadsTest, "GSTNCombat.NP.StateEvents.Payloads", GSTN_NP_TEST_FLAGS)
bool FGSTN_StateEventPayloadsTest::RunTest(const FString& Parameters)
{
	FGSTN_VictimHit Hit;
	Hit.Entry.Hit.Damage = 12.f;
	Hit.bStaggered = true;
	Hit.Reaction = EGSTN_HitReaction::Launch;
	Hit.bTumble = true;
	Hit.HitstunFrames = 40;
	Hit.HitstopFrames = 8;
	Hit.Outcome.bHasKnockback = true;
	Hit.Outcome.Knockback.LaunchVelocity = FVector(1000.0, 0.0, 1000.0);
	FGameplayTagContainer Tags;
	Tags.AddTag(GSTNCombatTags::Entity_Fighter);
	const FGSTN_CombatHitEventData Data = FGSTN_CombatHitEventData::FromVictimHit(Hit, 77, nullptr, nullptr, Tags);
	TestEqual(TEXT("damage"), Data.Damage, 12.f);
	TestEqual(TEXT("frame"), Data.Frame, 77);
	TestTrue(TEXT("reaction"), Data.bStaggered && Data.bTumble && Data.Reaction == EGSTN_HitReaction::Launch);
	TestEqual(TEXT("frames"), Data.HitstunFrames + Data.HitstopFrames, 48);
	TestTrue(TEXT("45 degree launch"), FMath::IsNearlyEqual(Data.LaunchAngle, 45.f, 0.01f));
	TestTrue(TEXT("event tags carried"), Data.Tags.HasTagExact(GSTNCombatTags::Entity_Fighter));

	Hit.bStaggered = false;
	TestTrue(TEXT("no stagger, no launch"), FGSTN_CombatHitEventData::FromVictimHit(Hit, 77, nullptr, nullptr, Tags).LaunchVelocity.IsZero());

	FGSTN_SimEvent Event;
	Event.EventType = GSTNCombatTags::Event_KO;
	Event.Frame = 90;
	Event.Tags.AddTag(GSTNCombatTags::KOReason_Zone);
	FGSTN_CombatKOEventData KO;
	TestTrue(TEXT("a KO event converts"), FGSTN_CombatKOEventData::FromSimEvent(Event, KO));
	TestTrue(TEXT("zone KO: reason, nobody scored"), KO.Reason == GSTNCombatTags::KOReason_Zone && KO.KnockedOutBy == nullptr && KO.Frame == 90);

	Event.Tags.AddTag(GSTNCombatNPTags::EventHit_CreditPartner);
	TestFalse(TEXT("the credit partner's copy does not fire On Knocked Out twice"), FGSTN_CombatKOEventData::FromSimEvent(Event, KO));

	Event = FGSTN_SimEvent();
	Event.EventType = GSTNCombatTags::Event_HitReceived;
	TestFalse(TEXT("another event does not convert"), FGSTN_CombatKOEventData::FromSimEvent(Event, KO));
	return true;
}

#endif
