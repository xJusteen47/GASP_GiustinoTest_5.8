// Automation tests of the combat core seams the spin and drill plugin asked for (spin-drill plan D6/D7): S1, the re-hit interval of a hitbox, and
// D-1, the authoritative event key that posts to the outbox. No world: the attacker step and the timeline logic are pure functions.

#include "Events/GSTN_SimEvent.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_CombatRules.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "StructUtils/InstancedStruct.h"
#include "Timeline/GSTN_TimelinePlayer.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNCombatSeamsTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Attacker 1 at the origin, a victim 2 within reach, a run that started on frame 0, one hitbox track over [0, 40) */
	struct FRehitRig
	{
		FGSTN_CombatTimelineData Timeline;
		FGSTN_StepSnapshot Snapshot;
		FGSTN_CombatSyncCore Core;
		FGSTN_TimelineFrameResult Scratch;
		TArray<FGSTN_AttackerHit> Hits;
		TArray<FGSTN_AttackerHit> AllHits;
		TStrongObjectPtr<UGSTN_CombatRules> Rules;
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set;

		explicit FRehitRig(int32 RehitIntervalFrames, uint8 Group = 0)
		{
			Rules.Reset(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
			Timeline.TotalFrames = 40;
			Timeline.FirstActionableFrame = 40;
			Timeline.OverrideFrames = 0;
			FGSTN_HitboxSpec Spec;
			Spec.Shape.Type = EGSTN_ShapeType::Sphere;
			Spec.Shape.Extent = FVector(80.0);
			Spec.HitGroup = Group;
			Spec.RehitIntervalFrames = RehitIntervalFrames;
			Spec.Data.Damage = 2.f;
			FGSTN_TimelineTrack_Hitbox Track;
			Track.StartFrame = 0;
			Track.EndFrame = 40;
			Track.Hitboxes.Add(Spec);
			Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));

			Set.Reset(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
			FGSTN_Hurtbox Hurtbox;
			Hurtbox.Shape.Type = EGSTN_ShapeType::Sphere;
			Hurtbox.Shape.Extent = FVector(40.0);
			Set->Hurtboxes.Add(Hurtbox);
			FGSTN_SnapshotEntry& Entry = Snapshot.Entries.AddDefaulted_GetRef();
			Entry.EntityId = 2;
			Entry.Team = FGenericTeamId(2);
			Entry.Transform = FTransform(FVector(60.0, 0.0, 0.0));
			Entry.Hurtboxes = Set.Get();
			Entry.HurtStates.SetNum(1);
			Entry.BoundRadius = static_cast<float>(UGSTN_CombatWorldSubsystem::ComputeBoundRadius(Set.Get()));
			Snapshot.BuildGrid(UGSTN_CombatWorldSubsystem::DefaultCellSize);

			Core.ActionState = EGSTN_ActionState::InTimeline;
			Core.ActiveTimeline = 0;
			Core.TimelineStartFrame = 0;
			Core.Hit.ResetRun();
		}

		/** Runs frames [0, Count) and returns the frames that hit */
		TArray<int32> RunFrames(int32 Count)
		{
			TArray<int32> HitFrames;
			for (int32 Frame = 0; Frame < Count; ++Frame)
			{
				FGSTN_AttackerTickInput In;
				In.Frame = Frame;
				In.AttackerId = 1;
				In.AttackerTeam = FGenericTeamId(1);
				In.Timeline = &Timeline;
				In.TimelineFrame = Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames;
				In.Snapshot = &Snapshot;
				In.Rules = Rules.Get();
				FGSTN_CombatAttacker::Tick(In, Core, Scratch, Hits);
				if (!Hits.IsEmpty())
				{
					HitFrames.Add(Frame);
					AllHits.Append(Hits);
				}
			}
			return HitFrames;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SeamRehitKeyTest, "GSTNCombat.NP.Seams.S1.TargetKey", GSTNCombatSeamsTest::Flags)
bool FGSTN_SeamRehitKeyTest::RunTest(const FString& Parameters)
{
	FGSTN_HitboxSpec Spec;
	Spec.HitGroup = 3;
	TestEqual(TEXT("no interval: the group, whatever the frame"), static_cast<int32>(Spec.MakeTargetKey(57)), 3);
	Spec.RehitIntervalFrames = 6;
	TestEqual(TEXT("interval 6, frame 5: the first interval"), static_cast<int32>(Spec.MakeTargetKey(5)), 3);
	TestEqual(TEXT("interval 6, frame 6: the second interval"), static_cast<int32>(Spec.MakeTargetKey(6)), 3 | (1 << 3));
	TestEqual(TEXT("the group stays in the low three bits"), Spec.MakeTargetKey(200) & 7, 3);
	TestEqual(TEXT("the interval index wraps at 32"), static_cast<int32>(Spec.MakeTargetKey(6 * 32)), 3);
	Spec.HitGroup = 9;
	TestEqual(TEXT("a group of 8 or more ignores the interval"), static_cast<int32>(Spec.MakeTargetKey(30)), 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SeamRehitAttackerTest, "GSTNCombat.NP.Seams.S1.RehitEveryInterval", GSTNCombatSeamsTest::Flags)
bool FGSTN_SeamRehitAttackerTest::RunTest(const FString& Parameters)
{
	using namespace GSTNCombatSeamsTest;

	FRehitRig Once(0);
	TestEqual(TEXT("interval 0: one hit for the whole attack (today's rule)"), Once.RunFrames(40), TArray<int32>({ 0 }));

	FRehitRig Every6(6);
	const TArray<int32> Frames = Every6.RunFrames(40);
	TestEqual(TEXT("interval 6: a hit on the first frame of every interval"), Frames, TArray<int32>({ 0, 6, 12, 18, 24, 30, 36 }));
	TestEqual(TEXT("the hits report the plain group"), Every6.AllHits.Num() < 2 ? -1 : static_cast<int32>(Every6.AllHits[1].HitGroup), 0);
	TestTrue(TEXT("and the ring's key carries the interval"), Every6.AllHits.Num() >= 2 && Every6.AllHits[1].TargetKey == (1 << 3));
	TestEqual(TEXT("the confirm flag is the group's bit"), static_cast<int32>(Every6.Core.Hit.HitConfirmFlags), 1);

	// Held frames count: a stalled timeline (the timeline frame stands still) keeps re-hitting by the run's frames
	FRehitRig Held(6);
	Held.Core.TimelinePausedFrames = 0;
	TArray<int32> HeldFrames;
	for (int32 Frame = 0; Frame < 20; ++Frame)
	{
		FGSTN_AttackerTickInput In;
		In.Frame = Frame;
		In.AttackerId = 1;
		In.AttackerTeam = FGenericTeamId(1);
		In.Timeline = &Held.Timeline;
		In.TimelineFrame = FMath::Min(Frame, 3);
		In.Snapshot = &Held.Snapshot;
		In.Rules = Held.Rules.Get();
		FGSTN_CombatAttacker::Tick(In, Held.Core, Held.Scratch, Held.Hits);
		if (!Held.Hits.IsEmpty())
		{
			HeldFrames.Add(Frame);
		}
	}
	TestEqual(TEXT("held on timeline frame 3: still a hit every 6 frames"), HeldFrames, TArray<int32>({ 0, 6, 12, 18 }));

	FRehitRig Wide(6, 9);
	TestEqual(TEXT("a group of 8 or more: once, the interval is ignored"), Wide.RunFrames(40), TArray<int32>({ 0 }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SeamOutboxKeyTest, "GSTNCombat.NP.Seams.D1.EventKeyPostsToOutbox", GSTNCombatSeamsTest::Flags)
bool FGSTN_SeamOutboxKeyTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 20;
	Data.FirstActionableFrame = 20;
	Data.OverrideFrames = 0;
	FGSTN_TimelineTrack_Event CueOnly;
	CueOnly.StartFrame = 2;
	CueOnly.EndFrame = 3;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Event>(CueOnly));
	FGSTN_TimelineTrack_Event Key;
	Key.StartFrame = 5;
	Key.EndFrame = 6;
	Key.OutboxEventTag = FGameplayTag::RequestGameplayTag(TEXT("Combat.Event.TimelineStarted"));
	Key.OutboxMagnitude = -1.f;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Event>(Key));

	FGSTN_SimEventOutbox Outbox;
	FGSTN_TimelineOutput Output;
	int32 Posted = 0;
	TArray<int32> PostedFrames;
	int32 Previous = INDEX_NONE;
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		FGSTN_TimelineEvalParams Params;
		Params.Frame = Frame;
		Params.PreviousFrame = Previous;
		FGSTN_TimelinePlayer::Evaluate(Data, Params, Output);
		Previous = Frame;
		const int32 Now = FGSTN_CombatTimelineLogic::AppendOutboxEvents(Output, 100 + Frame, nullptr, Outbox);
		Posted += Now;
		if (Now > 0)
		{
			PostedFrames.Add(100 + Frame);
		}
	}
	TestEqual(TEXT("one event: the key with an outbox tag, not the cue-only one"), Posted, 1);
	TestEqual(TEXT("on the frame the key is crossed"), PostedFrames, TArray<int32>({ 105 }));

	const TArray<FGSTN_SimEvent>& Events = Outbox.Events;
	TestTrue(TEXT("it carries the key's type and magnitude"), Events.Num() == 1 && Events[0].EventType == Key.OutboxEventTag && Events[0].Magnitude == -1.f && Events[0].Frame == 105);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
