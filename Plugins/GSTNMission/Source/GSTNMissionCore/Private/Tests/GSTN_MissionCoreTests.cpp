// M12 G4 part 1: mission tracker, pure. The tests borrow this plugin's native tags as identities (objective and event leaves are ini-only).

#include "Events/GSTN_SimEvent.h"
#include "GSTN_MissionTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNMissionTags;

namespace
{
	/** The counted event of these tests (stands for Combat.Event.KO + Minion.Type.X) */
	const FGameplayTag& KillTag() { return Message_Mission_Collected; }
	/** An event that fails things (stands for the defended target's KO) */
	const FGameplayTag& LossTag() { return Mission_Def; }

	FGSTN_SimEvent MakeEvent(const FGameplayTag& Type, int32 Frame = INDEX_NONE, AActor* Target = nullptr)
	{
		FGSTN_SimEvent Event;
		Event.EventType = Type;
		Event.Frame = Frame;
		Event.Target = Target;
		return Event;
	}

	FGameplayTagQuery Query(const FGameplayTag& Tag)
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(Tag);
		return FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);
	}

	UGSTN_ObjectiveRule_CountEvents* CountRule(UObject* Outer, int32 Count, const FGameplayTag& Tag)
	{
		UGSTN_ObjectiveRule_CountEvents* Rule = NewObject<UGSTN_ObjectiveRule_CountEvents>(Outer);
		Rule->EventQuery = Query(Tag);
		Rule->Count = Count;
		return Rule;
	}

	FGSTN_ObjectiveSpec Spec(const FGameplayTag& Id, UGSTN_ObjectiveRule* Rule, bool bOptional = false)
	{
		FGSTN_ObjectiveSpec Out;
		Out.ObjectiveTag = Id;
		Out.Rule = Rule;
		Out.bOptional = bOptional;
		return Out;
	}

	int32 CountChanges(const TArray<FGSTN_MissionChange>& Changes, const FGameplayTag& Type)
	{
		int32 N = 0;
		for (const FGSTN_MissionChange& Change : Changes)
		{
			N += Change.EventType == Type ? 1 : 0;
		}
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MissionSequentialTest, "GSTNMission.Core.Sequential",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_MissionSequentialTest::RunTest(const FString& Parameters)
{
	UGSTN_MissionDefinition* Def = NewObject<UGSTN_MissionDefinition>(GetTransientPackage());
	Def->Order = EGSTN_MissionOrder::Sequential;
	Def->Objectives.Add(Spec(Message_Mission_ObjectiveActivated, CountRule(Def, 3, KillTag())));
	Def->Objectives.Add(Spec(Message_Mission_ObjectiveProgressed, CountRule(Def, 5, KillTag()), true));
	Def->Objectives.Add(Spec(Message_Mission_ObjectiveCompleted, CountRule(Def, 1, KillTag())));

	FGSTN_MissionTracker Tracker;
	TArray<FGSTN_MissionChange> Changes;
	Tracker.Start(Def, 0.f, true, Changes);
	TestTrue(TEXT("running"), Tracker.GetState() == EGSTN_MissionState::Running);
	TestTrue(TEXT("first active"), Tracker.GetObjectives()[0].State == EGSTN_ObjectiveState::Active);
	TestTrue(TEXT("optional waits for the required one before it"), Tracker.GetObjectives()[1].State == EGSTN_ObjectiveState::Locked);
	TestTrue(TEXT("third locked"), Tracker.GetObjectives()[2].State == EGSTN_ObjectiveState::Locked);

	for (int32 Kill = 0; Kill < 3; ++Kill)
	{
		Tracker.HandleEvent(MakeEvent(KillTag()), 1.f, Changes);
	}
	TestTrue(TEXT("first completed after 3"), Tracker.GetObjectives()[0].State == EGSTN_ObjectiveState::Completed);
	TestTrue(TEXT("optional opens"), Tracker.GetObjectives()[1].State == EGSTN_ObjectiveState::Active);
	TestTrue(TEXT("third opens (the optional one does not hold it)"), Tracker.GetObjectives()[2].State == EGSTN_ObjectiveState::Active);
	TestEqual(TEXT("the 3rd kill did not leak into the next objective"), Tracker.GetObjectives()[2].Progress, 0);

	Tracker.HandleEvent(MakeEvent(KillTag()), 2.f, Changes);
	TestTrue(TEXT("mission completed without the optional one"), Tracker.GetState() == EGSTN_MissionState::Completed);
	TestEqual(TEXT("one MissionCompleted"), CountChanges(Changes, Message_Mission_MissionCompleted), 1);
	Tracker.HandleEvent(MakeEvent(KillTag()), 3.f, Changes);
	TestEqual(TEXT("a finished mission ignores events"), CountChanges(Changes, Message_Mission_MissionCompleted), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MissionGraphTest, "GSTNMission.Core.ParallelGraphAndFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_MissionGraphTest::RunTest(const FString& Parameters)
{
	// A and B in parallel, C needs both
	UGSTN_MissionDefinition* Def = NewObject<UGSTN_MissionDefinition>(GetTransientPackage());
	Def->Order = EGSTN_MissionOrder::Parallel;
	Def->Objectives.Add(Spec(Message_Mission_ObjectiveActivated, CountRule(Def, 1, KillTag())));
	Def->Objectives.Add(Spec(Message_Mission_ObjectiveProgressed, CountRule(Def, 1, Message_Mission_ObjectiveFailed)));
	FGSTN_ObjectiveSpec Final = Spec(Message_Mission_ObjectiveCompleted, CountRule(Def, 1, KillTag()));
	Final.Prerequisites.AddTag(Message_Mission_ObjectiveActivated);
	Final.Prerequisites.AddTag(Message_Mission_ObjectiveProgressed);
	Def->Objectives.Add(Final);

	FGSTN_MissionTracker Tracker;
	TArray<FGSTN_MissionChange> Changes;
	Tracker.Start(Def, 0.f, true, Changes);
	TestEqual(TEXT("A and B active at once"), CountChanges(Changes, Message_Mission_ObjectiveActivated), 2);
	Tracker.HandleEvent(MakeEvent(KillTag()), 1.f, Changes);
	TestTrue(TEXT("C still locked with B open"), Tracker.GetObjectives()[2].State == EGSTN_ObjectiveState::Locked);
	Tracker.HandleEvent(MakeEvent(Message_Mission_ObjectiveFailed), 1.f, Changes);
	TestTrue(TEXT("C opens when both are done"), Tracker.GetObjectives()[2].State == EGSTN_ObjectiveState::Active);

	// Objective failure query fails the mission (required objective)
	UGSTN_ObjectiveRule_Defend* Defend = NewObject<UGSTN_ObjectiveRule_Defend>(Def);
	Defend->TimerSeconds = 10.f;
	Defend->FailQuery = Query(LossTag());
	UGSTN_MissionDefinition* DefendDef = NewObject<UGSTN_MissionDefinition>(GetTransientPackage());
	DefendDef->Objectives.Add(Spec(Message_Mission_ObjectiveActivated, Defend));
	FGSTN_MissionTracker Lost;
	Lost.Start(DefendDef, 0.f, true, Changes);
	Lost.HandleEvent(MakeEvent(LossTag()), 4.f, Changes);
	TestTrue(TEXT("defended target lost: mission failed"), Lost.GetState() == EGSTN_MissionState::Failed);

	// The same defend survives its timer
	FGSTN_MissionTracker Held;
	Held.Start(DefendDef, 0.f, true, Changes);
	FGSTN_ObjectivePollContext Context;
	Context.Now = 9.9f;
	Held.Poll(Context, Changes);
	TestTrue(TEXT("still defending at 9.9 s"), Held.GetState() == EGSTN_MissionState::Running);
	Context.Now = 10.f;
	Held.Poll(Context, Changes);
	TestTrue(TEXT("defended 10 s: completed"), Held.GetState() == EGSTN_MissionState::Completed);

	// Mission time limit and mission fail query
	UGSTN_MissionDefinition* Timed = NewObject<UGSTN_MissionDefinition>(GetTransientPackage());
	Timed->TimeLimitSeconds = 30.f;
	Timed->Objectives.Add(Spec(Message_Mission_ObjectiveActivated, CountRule(Timed, 5, KillTag())));
	FGSTN_MissionTracker Late;
	Late.Start(Timed, 100.f, true, Changes);
	Context.Now = 131.f;
	Late.Poll(Context, Changes);
	TestTrue(TEXT("time limit fails the mission"), Late.GetState() == EGSTN_MissionState::Failed);
	Timed->TimeLimitSeconds = 0.f;
	Timed->FailQuery = Query(LossTag());
	FGSTN_MissionTracker Key;
	Key.Start(Timed, 0.f, true, Changes);
	Key.HandleEvent(MakeEvent(LossTag()), 1.f, Changes);
	TestTrue(TEXT("mission fail query"), Key.GetState() == EGSTN_MissionState::Failed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MissionReachTest, "GSTNMission.Core.Reach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_MissionReachTest::RunTest(const FString& Parameters)
{
	UGSTN_MissionDefinition* Def = NewObject<UGSTN_MissionDefinition>(GetTransientPackage());
	UGSTN_ObjectiveRule_Reach* Reach = NewObject<UGSTN_ObjectiveRule_Reach>(Def);
	Reach->ZoneTag = Mission_Objective;
	Def->Objectives.Add(Spec(Mission_Objective, Reach));

	FGSTN_MissionTracker Tracker;
	TArray<FGSTN_MissionChange> Changes;
	Tracker.Start(Def, 0.f, true, Changes);
	FGSTN_ObjectivePollContext Context;
	Context.AlivePlayers = 3;
	Context.PlayersInZone.Add(Mission_Objective, 2);
	Tracker.Poll(Context, Changes);
	TestTrue(TEXT("all alive required: 2 of 3 is not enough"), Tracker.GetState() == EGSTN_MissionState::Running);
	Context.AlivePlayers = 2;
	Tracker.Poll(Context, Changes);
	TestTrue(TEXT("the third is down: 2 of 2 completes"), Tracker.GetState() == EGSTN_MissionState::Completed);

	Reach->Require = EGSTN_ReachRequire::AnyPlayer;
	FGSTN_MissionTracker Any;
	Any.Start(Def, 0.f, true, Changes);
	Context.AlivePlayers = 4;
	Context.PlayersInZone.Add(Mission_Objective, 1);
	Any.Poll(Context, Changes);
	TestTrue(TEXT("any player: one inside completes"), Any.GetState() == EGSTN_MissionState::Completed);
	return true;
}

/**
 * game-modes 9 / plan G4: "10 kills with forced rollbacks count exactly 10". The pure part of the guarantee: a client's outbox sees each kill on its
 * predicted frame and again on every re-simulation, but only the authority publishes (FGSTN_SimEventOutbox::Flush), and the credit partner's copy
 * of a KO (same victim, same frame) merges. The in-engine half (2 % loss, real rollbacks) is spike G0/G5 and scenario_mission_replicate.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MissionRollbackCountTest, "GSTNMission.Core.TenKillsWithRollbacksCountTen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_MissionRollbackCountTest::RunTest(const FString& Parameters)
{
	UGSTN_MissionDefinition* Def = NewObject<UGSTN_MissionDefinition>(GetTransientPackage());
	Def->Objectives.Add(Spec(Message_Mission_ObjectiveActivated, CountRule(Def, 20, KillTag())));
	FGSTN_MissionTracker Tracker;
	TArray<FGSTN_MissionChange> Changes;
	Tracker.Start(Def, 0.f, true, Changes);

	// Ten victims: distinct objects stand in for the KO'd actors
	TArray<UObject*> Victims;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Victims.Add(NewObject<UGSTN_ObjectiveRule_CountEvents>(GetTransientPackage()));
	}

	int32 ClientPublished = 0;
	for (int32 Kill = 0; Kill < 10; ++Kill)
	{
		const int32 Frame = 100 + Kill * 7;
		// Client: predicted frame + two re-simulations of it, never published
		FGSTN_SimEventOutbox Client;
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Client.Add(MakeEvent(KillTag(), Frame));
		}
		ClientPublished += Client.Flush(false, [](FGSTN_SimEvent&) {});

		// Host: the KO once, plus the credit partner's copy (same victim and frame)
		FGSTN_SimEventOutbox Host;
		FGSTN_SimEvent KO = MakeEvent(KillTag(), Frame);
		KO.Target = reinterpret_cast<AActor*>(Victims[Kill]); // identity only, never dereferenced
		FGSTN_SimEvent Partner = KO;
		Host.Add(MoveTemp(KO));
		Host.Add(MoveTemp(Partner));
		Host.Flush(true, [&Tracker, &Changes](FGSTN_SimEvent& Event) { Tracker.HandleEvent(Event, 1.f, Changes); });
	}
	TestEqual(TEXT("the client published nothing"), ClientPublished, 0);
	TestEqual(TEXT("exactly 10 counted"), Tracker.GetObjectives()[0].Progress, 10);
	return true;
}

#endif
