// Automation tests of the timeline nodes (move-timeline-decoupling 1A and 1B): the rules a start from an ability graph is checked and reported
// with, the registry entries of a host's Playable Timelines, and the state a start in the frame of the call writes. The in-frame write itself
// needs the ability system's simulation step and runs in the PIE harness on the desktop (see the handover of this checkpoint).

#include "Abilities/GSTN_TimelinePlay.h"
#include "Misc/AutomationTest.h"
#include "Simulation/GSTN_CombatActionTypes.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "Tests/GSTN_TimelinePlayTestTypes.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelinePlayer.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_TIMELINE_PLAY_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNTimelinePlayTest
{
	UGSTN_CombatTimeline* MakeTimeline(int32 TotalFrames)
	{
		UGSTN_CombatTimeline* Timeline = NewObject<UGSTN_CombatTimeline>(GetTransientPackage());
		Timeline->Data.TotalFrames = TotalFrames;
		Timeline->Data.FirstActionableFrame = TotalFrames;
		Timeline->Data.OverrideFrames = 0;
		return Timeline;
	}

	/** Sets the host's Playable Timelines on its class default for the scope of a test, and restores them */
	struct FPlayableScope
	{
		TArray<TSoftObjectPtr<UGSTN_CombatTimeline>> Saved;

		explicit FPlayableScope(TArray<UGSTN_CombatTimeline*> Timelines)
		{
			UGSTN_TestTimelineHost* Default = GetMutableDefault<UGSTN_TestTimelineHost>();
			Saved = Default->PlayableTimelines;
			Default->PlayableTimelines.Reset();
			for (UGSTN_CombatTimeline* Timeline : Timelines)
			{
				Default->PlayableTimelines.Add(Timeline);
			}
		}

		~FPlayableScope()
		{
			GetMutableDefault<UGSTN_TestTimelineHost>()->PlayableTimelines = Saved;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTimelinePlayRulesTest, "GSTNCombat.NP.TimelinePlay.Rules", GSTN_TIMELINE_PLAY_TEST_FLAGS)
bool FGSTN_NPTimelinePlayRulesTest::RunTest(const FString& Parameters)
{
	// Refusals, in the order a caller meets them: outside the simulation first (a call from an input event must never write), then a restore
	TestEqual(TEXT("outside the simulation step"), FGSTN_TimelinePlayRules::CheckStart(false, true, true, false), EGSTN_TimelineStartRefusal::OutsideSimulation);
	TestEqual(TEXT("while a frame is restored"), FGSTN_TimelinePlayRules::CheckStart(true, true, false, true), EGSTN_TimelineStartRefusal::Restoring);
	TestEqual(TEXT("gate closed by a loadout swap"), FGSTN_TimelinePlayRules::CheckStart(true, false, true, true), EGSTN_TimelineStartRefusal::GateClosed);
	TestEqual(TEXT("timeline not registered"), FGSTN_TimelinePlayRules::CheckStart(true, false, false, false), EGSTN_TimelineStartRefusal::NotRegistered);
	TestEqual(TEXT("accepted"), FGSTN_TimelinePlayRules::CheckStart(true, false, false, true), EGSTN_TimelineStartRefusal::None);

	// The pins
	TestEqual(TEXT("finished is completed"), FGSTN_TimelinePlayRules::ResolveResult(EGSTN_MoveEndReason::Finished, false), EGSTN_TimelinePlayResult::Completed);
	TestEqual(TEXT("finished on the frame the ability ends is still completed"), FGSTN_TimelinePlayRules::ResolveResult(EGSTN_MoveEndReason::Finished, true), EGSTN_TimelinePlayResult::Completed);
	TestEqual(TEXT("a hit is interrupted"), FGSTN_TimelinePlayRules::ResolveResult(EGSTN_MoveEndReason::Interrupted, false), EGSTN_TimelinePlayResult::Interrupted);
	TestEqual(TEXT("a tail cancel is interrupted"), FGSTN_TimelinePlayRules::ResolveResult(EGSTN_MoveEndReason::TailCancelled, false), EGSTN_TimelinePlayResult::Interrupted);
	TestEqual(TEXT("a recoil is interrupted"), FGSTN_TimelinePlayRules::ResolveResult(EGSTN_MoveEndReason::AfterRecoil, false), EGSTN_TimelinePlayResult::Interrupted);
	TestEqual(TEXT("the ability ending is cancelled"), FGSTN_TimelinePlayRules::ResolveResult(EGSTN_MoveEndReason::Interrupted, true), EGSTN_TimelinePlayResult::Cancelled);

	// A host survives a completed play, and ends on an interrupted one unless told otherwise
	TestFalse(TEXT("completed never ends a host"), FGSTN_TimelinePlayRules::ShouldHostEndAfterPlay(EGSTN_TimelinePlayResult::Completed, true));
	TestTrue(TEXT("interrupted ends a host by default"), FGSTN_TimelinePlayRules::ShouldHostEndAfterPlay(EGSTN_TimelinePlayResult::Interrupted, true));
	TestFalse(TEXT("interrupted with the option off keeps the host"), FGSTN_TimelinePlayRules::ShouldHostEndAfterPlay(EGSTN_TimelinePlayResult::Interrupted, false));
	TestFalse(TEXT("cancelled: the ability is ending already"), FGSTN_TimelinePlayRules::ShouldHostEndAfterPlay(EGSTN_TimelinePlayResult::Cancelled, true));

	// The tag difference of a late start
	const FGameplayTag A = FGameplayTag::RequestGameplayTag(TEXT("Combat.Ability.Timeline"));
	const FGameplayTag B = FGameplayTag::RequestGameplayTag(TEXT("Combat.Ability.AwaitingAdoption"));
	FGameplayTagContainer Before;
	Before.AddTag(A);
	FGameplayTagContainer After;
	After.AddTag(B);
	FGameplayTagContainer Remove;
	FGameplayTagContainer Add;
	FGSTN_TimelinePlayRules::DiffTags(Before, After, Remove, Add);
	TestTrue(TEXT("the old timeline's tag goes"), Remove.HasTagExact(A) && Remove.Num() == 1);
	TestTrue(TEXT("the new timeline's tag comes"), Add.HasTagExact(B) && Add.Num() == 1);
	FGSTN_TimelinePlayRules::DiffTags(Before, Before, Remove, Add);
	TestTrue(TEXT("the same tags move nothing"), Remove.IsEmpty() && Add.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTimelinePlayRegistryTest, "GSTNCombat.NP.TimelinePlay.Registry", GSTN_TIMELINE_PLAY_TEST_FLAGS)
bool FGSTN_NPTimelinePlayRegistryTest::RunTest(const FString& Parameters)
{
	using namespace GSTNTimelinePlayTest;
	UGSTN_CombatTimeline* PhaseA = MakeTimeline(10);
	UGSTN_CombatTimeline* PhaseB = MakeTimeline(20);
	UGSTN_CombatTimeline* NotPlayable = MakeTimeline(5);
	const FPlayableScope Scope({PhaseA, PhaseB, PhaseA});

	TestTrue(TEXT("Play Timeline On Activation off is a host"), UGSTN_CombatTableSet::IsHostClass(UGSTN_TestTimelineHost::StaticClass()));
	TestFalse(TEXT("a standard move is not"), UGSTN_CombatTableSet::IsHostClass(UGSTN_TestTimelineMove::StaticClass()));
	TestFalse(TEXT("no class is not"), UGSTN_CombatTableSet::IsHostClass(nullptr));

	UGSTN_CombatTableSet* Set = NewObject<UGSTN_CombatTableSet>(GetTransientPackage());
	Set->AddTimelineClass(UGSTN_TestTimelineMove::StaticClass(), 1, 2, true);
	Set->AddTimelineClass(UGSTN_TestTimelineHost::StaticClass(), 0, 0, true);
	Set->AppendAdoptable({});

	// The host's slot, then one entry per distinct playable timeline, in list order
	TestEqual(TEXT("registry: move, host, A, B"), Set->TimelineRegistry.Num(), 4);
	const int32 EntryA = Set->FindPlayEntry(UGSTN_TestTimelineHost::StaticClass(), PhaseA);
	const int32 EntryB = Set->FindPlayEntry(UGSTN_TestTimelineHost::StaticClass(), PhaseB);
	TestEqual(TEXT("A is entry 2"), EntryA, 2);
	TestEqual(TEXT("B is entry 3"), EntryB, 3);
	TestTrue(TEXT("an entry runs its timeline's data"), Set->GetTimelineData(EntryB) == &PhaseB->Data);
	TestEqual(TEXT("a timeline the host does not list is not playable"), Set->FindPlayEntry(UGSTN_TestTimelineHost::StaticClass(), NotPlayable), INDEX_NONE);
	TestEqual(TEXT("another class does not play the host's timelines"), Set->FindPlayEntry(UGSTN_TestTimelineMove::StaticClass(), PhaseA), INDEX_NONE);
	TestEqual(TEXT("no timeline is no entry"), Set->FindPlayEntry(UGSTN_TestTimelineHost::StaticClass(), nullptr), INDEX_NONE);
	TestEqual(TEXT("played entries have no tier"), Set->TimelineTier[EntryA], 0);
	TestEqual(TEXT("played entries have no default window"), Set->TimelineDefaultOverride[EntryA], 0);

	// Start Combat Move runs the first entry of a class
	TestEqual(TEXT("the move's entry"), Set->FindMoveEntry(UGSTN_TestTimelineMove::StaticClass()), 0);
	TestEqual(TEXT("the host's own slot comes before its plays"), Set->FindMoveEntry(UGSTN_TestTimelineHost::StaticClass()), 1);
	TestEqual(TEXT("an unknown class has none"), Set->FindMoveEntry(UGSTN_GA_BaseCombat::StaticClass()), INDEX_NONE);

	// Building again adds nothing: a rebuilt set must hold the same indices on every machine
	Set->AppendPlayableTimelines();
	TestEqual(TEXT("idempotent"), Set->TimelineRegistry.Num(), 4);
	const uint32 Checksum = Set->ComputeChecksum();
	UGSTN_CombatTableSet* Again = NewObject<UGSTN_CombatTableSet>(GetTransientPackage());
	Again->AddTimelineClass(UGSTN_TestTimelineMove::StaticClass(), 1, 2, true);
	Again->AddTimelineClass(UGSTN_TestTimelineHost::StaticClass(), 0, 0, true);
	Again->AppendAdoptable({});
	TestEqual(TEXT("the same build gives the same checksum"), Again->ComputeChecksum(), Checksum);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTimelinePlayStartTest, "GSTNCombat.NP.TimelinePlay.StartInFrame", GSTN_TIMELINE_PLAY_TEST_FLAGS)
bool FGSTN_NPTimelinePlayStartTest::RunTest(const FString& Parameters)
{
	// What StartTimelineFromAbility writes into the frame's state, as the pure steps it runs: Start (source Forced) and EvaluateStart
	using namespace GSTNTimelinePlayTest;
	UGSTN_CombatTimeline* Phase = MakeTimeline(12);
	FGSTN_TimelineOutput Output;
	Output.Reserve(Phase->Data);

	// From actionable (a host between two plays, or a stance): the new timeline runs from this frame, and with no window it commits on it
	{
		FGSTN_CombatSyncCore Core;
		FGSTN_CombatTimelineLogic::Start(Core, 200, 3, EGSTN_ActivationSource::Forced);
		FGSTN_CombatTimelineLogic::EvaluateStart(Core, 200, Phase->Data, 0, Output);
		TestEqual(TEXT("in a timeline"), Core.ActionState, EGSTN_ActionState::InTimeline);
		TestEqual(TEXT("the played entry"), static_cast<int32>(Core.ActiveTimeline), 3);
		TestEqual(TEXT("from the frame of the call"), Core.TimelineStartFrame, 200);
		TestEqual(TEXT("source Forced"), Core.ActiveSource, EGSTN_ActivationSource::Forced);
		TestNotNull(TEXT("commit due on frame 0: the caller commits (the pre-tick ran already)"), Core.Requests.Find(EGSTN_RequestType::CommitTimeline, 200, static_cast<int16>(3)));
	}

	// Over a running move (Start Combat Move from inside a move): the old one is replaced in the same frame, no actionable frame in between
	{
		FGSTN_CombatSyncCore Core;
		FGSTN_CombatTimelineLogic::Start(Core, 190, 1, EGSTN_ActivationSource::Command);
		FGSTN_CombatTimelineLogic::Start(Core, 200, 3, EGSTN_ActivationSource::Forced);
		FGSTN_CombatTimelineLogic::EvaluateStart(Core, 200, Phase->Data, 0, Output);
		TestEqual(TEXT("still in a timeline"), Core.ActionState, EGSTN_ActionState::InTimeline);
		TestEqual(TEXT("the new entry"), static_cast<int32>(Core.ActiveTimeline), 3);
		TestEqual(TEXT("its own start frame"), Core.TimelineStartFrame, 200);
		TestEqual(TEXT("no pause carried over"), Core.TimelinePausedFrames, 0);
	}
	return true;
}

#endif
