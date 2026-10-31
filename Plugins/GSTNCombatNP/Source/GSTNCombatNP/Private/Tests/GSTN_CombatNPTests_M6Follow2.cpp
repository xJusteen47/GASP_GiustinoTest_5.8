// Automation tests of the M6 follow-up 2 seams (F2) in the NP module: extra hurtboxes in the snapshot and its history, the track hooks of the combat
// clock, the can-act veto of the tick extensions, the reverse deliveries of the victim step and the request array. No world.

#include "GSTN_CombatNPTestTypes.h"
#include "GSTN_VictimTestTypes.h"
#include "GameFramework/Actor.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_CombatRules.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "Simulation/GSTN_LagCompensation.h"
#include "Simulation/GSTN_TimelineTrackHook.h"
#include "UObject/StrongObjectPtr.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	// ---- Extra hurtboxes in the snapshot

	/** Appends up to Count spheres whose radius is the frame number (so a record of frame N is told from one of frame M) */
	class F2TestExtraContributor : public IGSTN_SnapshotContributor
	{
	public:
		explicit F2TestExtraContributor(int32 InCount) : Count(InCount) {}

		virtual uint8 GetTypeId() const override { return GSTNFeatureIds::TestA; }
		virtual void Contribute(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionBlob& Blob) const override {}

		virtual void AppendExtraHurtboxes(const FGSTN_SnapshotContributorContext& Context, FGSTN_ExtraHurtboxList& Out) const override
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FGSTN_ExtraHurtbox& Extra = Out.AddDefaulted_GetRef();
				Extra.Shape.Type = EGSTN_ShapeType::Sphere;
				Extra.Shape.Extent = FVector(static_cast<double>(Context.Frame));
				Extra.Priority = static_cast<uint8>(Index);
				Extra.Source = GSTNFeatureIds::TestA;
			}
		}

	private:
		int32 Count;
	};

	bool F2ExtraNoPose(int32 EntityId, FTransform& OutPose)
	{
		return false;
	}

	// ---- The clock's track hooks

	/** Answers with a lambda and remembers the timeline frame each call saw */
	class F2TestTrackHook : public IGSTN_TimelineTrackHook
	{
	public:
		using FAnswer = TFunction<FGSTN_TrackHookResult(const FGSTN_TrackHookContext&)>;

		explicit F2TestTrackHook(FAnswer InAnswer) : Answer(MoveTemp(InAnswer)) {}

		virtual FGSTN_TrackHookResult Evaluate(const FGSTN_TrackHookContext& Context) const override
		{
			SeenTimelineFrames.Add(Context.TimelineFrame);
			return Answer(Context);
		}

		mutable TArray<int32> SeenTimelineFrames;

	private:
		FAnswer Answer;
	};

	/** Registers a hook for the test track for the length of the scope: the registry is the process-wide one the combat tick uses */
	struct F2HookScope
	{
		explicit F2HookScope(IGSTN_TimelineTrackHook& InHook) : Hook(InHook)
		{
			bRegistered = FGSTN_TimelineTrackHookRegistry::Get().Register(FGSTN_TestHoldTrack::StaticStruct(), &Hook);
		}

		~F2HookScope()
		{
			FGSTN_TimelineTrackHookRegistry::Get().Unregister(&Hook);
		}

		IGSTN_TimelineTrackHook& Hook;
		bool bRegistered = false;
	};

	/** 30 frames, actionable from 20, override window of 2 frames, and a test track [TrackStart, TrackEnd) per entry of Tracks */
	FGSTN_CombatTimelineData F2MakeTimeline(std::initializer_list<TPair<int32, int32>> Tracks)
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 30;
		Data.FirstActionableFrame = 20;
		Data.OverrideFrames = 2;
		for (const TPair<int32, int32>& Range : Tracks)
		{
			FGSTN_TestHoldTrack Track;
			Track.StartFrame = Range.Key;
			Track.EndFrame = Range.Value;
			Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TestHoldTrack>(Track));
		}
		return Data;
	}

	FGSTN_CombatSyncCore F2MakeRunning(int32 StartFrame)
	{
		FGSTN_CombatSyncCore Core;
		FGSTN_CombatTimelineLogic::Start(Core, StartFrame, 3, EGSTN_ActivationSource::Command);
		return Core;
	}

	// ---- The can-act veto

	class F2VetoExtension : public IGSTN_CombatTickExtension
	{
	public:
		F2VetoExtension(uint8 InTypeId, int32 InFromFrame) : TypeId(InTypeId), FromFrame(InFromFrame) {}

		virtual uint8 GetTypeId() const override { return TypeId; }
		virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) override {}
		virtual bool BlocksActions(const FGSTN_CombatTickContext& Context) const override
		{
			++Asked;
			return Context.Frame >= FromFrame;
		}

		mutable int32 Asked = 0;

	private:
		uint8 TypeId;
		int32 FromFrame;
	};

	/** Registers nothing and answers nothing: the default BlocksActions */
	class F2PlainExtension : public IGSTN_CombatTickExtension
	{
	public:
		virtual uint8 GetTypeId() const override { return 9; }
		virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) override {}
	};

	// ---- The victim step

	struct F2OutgoingFixture
	{
		static constexpr int32 F2VictimId = 2;

		TStrongObjectPtr<UGSTN_CombatRules> Rules;
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set;
		FGSTN_SnapshotEntry Self;
		FGSTN_CombatSyncCore Core;
		FGSTN_SyncFragmentArray Fragments;
		FGSTN_VictimResult Result;
		TArray<FGSTN_HitInboxEntry> Entries;

		F2OutgoingFixture()
		{
			Rules.Reset(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
			Rules->HitPipeline.Add(UGSTN_TestOutgoingStep::StaticClass());
			Rules->ResolveHitPipeline();
			Set.Reset(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
			FGSTN_Hurtbox Hurtbox;
			Hurtbox.Shape.Type = EGSTN_ShapeType::Sphere;
			Hurtbox.Shape.Extent = FVector(40.0);
			Set->Hurtboxes.Add(Hurtbox);

			Self.EntityId = F2VictimId;
			Self.Team = FGenericTeamId(2);
			Self.Transform = FTransform::Identity;
			Self.Hurtboxes = Set.Get();
			Self.HurtStates.SetNum(1);
			Self.Reaction = FGSTN_VictimReactionState::Make(0.0, 1.0, 0, FGSTN_StaggerThreshold());
		}

		static FGSTN_HitInboxEntry MakeEntry(int32 Frame, int32 F2AttackerId)
		{
			FGSTN_HitInboxEntry Entry;
			Entry.Frame = Frame;
			Entry.AttackerId = F2AttackerId;
			Entry.AttackerTeam = 1;
			Entry.Hit.Damage = 10.f;
			Entry.Hit.BaseKnockback = 600.f;
			Entry.Hit.KnockbackGrowth = 20.f;
			Entry.Hit.AngleDeg = 30.f;
			Entry.HitLocation = FVector(-40.0, 0.0, 0.0);
			Entry.Relationship = GSTNHitRelationship::ToIndex(GSTNCombatHitTags::Relationship_Enemy.GetTag());
			return Entry;
		}

		void Run(int32 Frame)
		{
			FGSTN_VictimTickInput In;
			In.Frame = Frame;
			In.Self = &Self;
			In.Rules = Rules.Get();
			In.Entries = Entries;
			In.Fragments = &Fragments;
			FGSTN_CombatVictim::Resolve(In, Core, Result);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SnapshotExtraHurtboxInHistoryTest, "GSTNCombat.NP.Snapshot.ExtraHurtboxInHistory", GSTN_NP_TEST_FLAGS)
bool FGSTN_SnapshotExtraHurtboxInHistoryTest::RunTest(const FString& Parameters)
{
	// The build: what the contributor appends is on the entry, cut at the two an index can name, and the entry is reset first
	{
		FGSTN_SnapshotContributorRegistry Registry;
		F2TestExtraContributor Contributor(3);
		TestTrue(TEXT("registers"), Registry.Register(&Contributor));
		FGSTN_SnapshotContributorContext Context;
		Context.Frame = 20;
		FGSTN_SnapshotEntry Entry;
		Registry.Fill(Context, Entry);
		TestEqual(TEXT("three appended, two kept"), Entry.ExtraHurtboxes.Num(), GSTNHurtboxIndex::MaxExtraHurtboxes);
		Registry.Fill(Context, Entry);
		TestEqual(TEXT("a second fill does not accumulate"), Entry.ExtraHurtboxes.Num(), GSTNHurtboxIndex::MaxExtraHurtboxes);
		TestFalse(TEXT("a contributor that writes no blob still contributes extras"), Entry.Extension.IsSet());

		const FGSTN_HitShape* Shape = Entry.FindHurtboxShape(GSTNHurtboxIndex::FromExtraPosition(1));
		TestNotNull(TEXT("the second extra is found by its hit index"), Shape);
		TestEqual(TEXT("with the shape the contributor gave"), Shape ? Shape->Extent.X : 0.0, 20.0);
		TestNull(TEXT("a position past the list is not a shape"), Entry.FindHurtboxShape(GSTNHurtboxIndex::FromExtraPosition(2)));
		TestNull(TEXT("nor is an index of an empty set"), Entry.FindHurtboxShape(0));
		TestEqual(TEXT("the victim view exposes them"), FGSTN_CombatVictim::MakeVictimView(Entry, nullptr).ExtraHurtboxes.Num(), 2);

		FGSTN_SnapshotEntry Bare;
		FGSTN_SnapshotContributorRegistry Empty;
		Empty.Fill(Context, Bare);
		TestTrue(TEXT("nothing registered: no extras"), Bare.ExtraHurtboxes.IsEmpty());
		TestTrue(TEXT("and none in the view"), FGSTN_CombatVictim::MakeVictimView(Bare, nullptr).ExtraHurtboxes.IsEmpty());
		Registry.Unregister(&Contributor);
	}

	// The broadphase radius of an extra: its offset plus its largest extent
	{
		FGSTN_ExtraHurtbox Extra;
		Extra.Shape.Type = EGSTN_ShapeType::Sphere;
		Extra.Shape.Extent = FVector(30.0);
		Extra.Shape.Local = FTransform(FVector(0.0, 0.0, 40.0));
		TestEqual(TEXT("offset 40 and radius 30"), GSTNExtraHurtboxes::ComputeBoundRadius(MakeArrayView(&Extra, 1)), 70.0);
		TestEqual(TEXT("no extras, no radius"), GSTNExtraHurtboxes::ComputeBoundRadius(TConstArrayView<FGSTN_ExtraHurtbox>()), 0.0);
	}

	// The history: a rewind hands the step the extras of the rewound frame, and the broadphase radius grows to hold them
	constexpr int32 F2VictimId = 7;
	constexpr int32 F2AttackerId = 3;
	constexpr float StepMs = 16.f;
	UGSTN_CombatWorldSubsystem* Subsystem = NewObject<UGSTN_CombatWorldSubsystem>(GetTransientPackage());
	Subsystem->RequestHistory(400);
	for (int32 Frame = 10; Frame <= 14; ++Frame)
	{
		FGSTN_StepSnapshot Snapshot;
		Snapshot.Frame = Frame;
		for (const int32 Id : { F2AttackerId, F2VictimId })
		{
			FGSTN_SnapshotEntry& Entry = Snapshot.Entries.AddDefaulted_GetRef();
			Entry.EntityId = Id;
			Entry.BoundRadius = 50.f;
			if (Id == F2VictimId)
			{
				// The bubble grows with the frame: radius 10 x (Frame - 9)
				FGSTN_ExtraHurtbox& Extra = Entry.ExtraHurtboxes.AddDefaulted_GetRef();
				Extra.Shape.Type = EGSTN_ShapeType::Sphere;
				Extra.Shape.Extent = FVector(10.0 * (Frame - 9));
			}
		}
		Subsystem->RecordHistory(Snapshot, Frame * StepMs, StepMs);
	}

	FGSTN_StepSnapshot Present;
	Present.Frame = 14;
	for (const int32 Id : { F2AttackerId, F2VictimId })
	{
		FGSTN_SnapshotEntry& Entry = Present.Entries.AddDefaulted_GetRef();
		Entry.EntityId = Id;
		Entry.BoundRadius = 50.f;
		if (Id == F2VictimId)
		{
			FGSTN_ExtraHurtbox& Extra = Entry.ExtraHurtboxes.AddDefaulted_GetRef();
			Extra.Shape.Type = EGSTN_ShapeType::Sphere;
			Extra.Shape.Extent = FVector(50.0);
		}
	}
	FGSTN_StepSnapshot Overlay;
	GSTNLagComp::BuildOverlay(Present, F2AttackerId, 12 * StepMs, *Subsystem, &F2ExtraNoPose, Overlay);

	const FGSTN_SnapshotEntry* Victim = Overlay.Find(F2VictimId);
	TestNotNull(TEXT("the overlay holds the victim"), Victim);
	if (!Victim)
	{
		return false;
	}
	TestEqual(TEXT("one extra rewound"), Victim->ExtraHurtboxes.Num(), 1);
	TestEqual(TEXT("the bubble of frame 12, not the present one"), Victim->ExtraHurtboxes.IsEmpty() ? 0.0 : Victim->ExtraHurtboxes[0].Shape.Extent.X, 30.0);
	TestTrue(TEXT("the radius never shrinks below what the present entry had"), Victim->BoundRadius >= 50.f);
	TestEqual(TEXT("the shared snapshot is untouched"), Present.Find(F2VictimId)->ExtraHurtboxes[0].Shape.Extent.X, 50.0);

	// A rewind to a frame whose bubble was larger than the body's radius grows the radius for the broadphase
	FGSTN_StepSnapshot Narrow = Present;
	for (FGSTN_SnapshotEntry& Entry : Narrow.Entries)
	{
		Entry.BoundRadius = 5.f;
		if (!Entry.ExtraHurtboxes.IsEmpty())
		{
			Entry.ExtraHurtboxes[0].Shape.Extent = FVector(5.0);
		}
	}
	FGSTN_StepSnapshot NarrowOverlay;
	GSTNLagComp::BuildOverlay(Narrow, F2AttackerId, 14 * StepMs, *Subsystem, &F2ExtraNoPose, NarrowOverlay);
	const FGSTN_SnapshotEntry* Grown = NarrowOverlay.Find(F2VictimId);
	TestTrue(TEXT("the rewound bubble (radius 50) widens the entry's bound"), Grown && Grown->BoundRadius >= 50.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ClockTrackHookStallsTimelineNotClockTest, "GSTNCombat.NP.Clock.TrackHookStallsTimelineNotClock", GSTN_NP_TEST_FLAGS)
bool FGSTN_ClockTrackHookStallsTimelineNotClockTest::RunTest(const FString& Parameters)
{
	// Window of 2 frames: the commit is at timeline frame 2. Started on 100; the hook stalls on frames 102 to 104, the same three frames the hitstop test pauses
	const FGSTN_CombatTimelineData Data = F2MakeTimeline({ { 1, 10 } });
	F2TestTrackHook Hook([](const FGSTN_TrackHookContext& Context)
	{
		FGSTN_TrackHookResult Result;
		Result.bStallThisFrame = Context.Frame >= 102 && Context.Frame <= 104;
		return Result;
	});
	F2HookScope Scope(Hook);
	TestTrue(TEXT("the hook registers for the track struct"), Scope.bRegistered);

	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	FGSTN_CombatSyncCore Core = F2MakeRunning(100);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 1, Output);

	int32 CommitFrame = INDEX_NONE;
	for (int32 Frame = 101; Frame <= 110; ++Frame)
	{
		Core.Requests.PruneBefore(Frame);
		FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 1, true, Output);
		if (Frame >= 102 && Frame <= 104)
		{
			TestEqual(*FString::Printf(TEXT("the timeline frame stands still on %d"), Frame), Output.Frame, 1);
			TestTrue(*FString::Printf(TEXT("nothing is committed on %d"), Frame), !Core.Requests.Find(EGSTN_RequestType::CommitTimeline, Frame));
		}
		if (Core.Requests.Find(EGSTN_RequestType::CommitTimeline, Frame))
		{
			CommitFrame = Frame;
		}
	}
	TestEqual(TEXT("the commit comes three frames later"), CommitFrame, 105);
	TestEqual(TEXT("the timeline counted the stalled frames"), Core.TimelinePausedFrames, 3);
	TestEqual(TEXT("the combat clock did not pause"), Core.PausedFramesTotal, 0);
	TestEqual(TEXT("so it still counts every frame"), FGSTN_CombatClock::FromFrame(104, Core.PausedFramesTotal), 104);

	// What the hook was told: the frame the timeline is on at the end of the previous tick (a stall holds it). The first call is EvaluateStart's, on the start frame
	TestTrue(TEXT("asked on the start frame and once per tick after it"), Hook.SeenTimelineFrames.Num() >= 7);
	if (Hook.SeenTimelineFrames.Num() >= 7)
	{
		TestEqual(TEXT("frame 100 (EvaluateStart): on the first frame"), Hook.SeenTimelineFrames[0], 0);
		TestEqual(TEXT("frame 101: on 0"), Hook.SeenTimelineFrames[1], 0);
		TestEqual(TEXT("frame 102: on 1"), Hook.SeenTimelineFrames[2], 1);
		TestEqual(TEXT("frame 103: still on 1 (stalled)"), Hook.SeenTimelineFrames[3], 1);
		TestEqual(TEXT("frame 104: still on 1"), Hook.SeenTimelineFrames[4], 1);
		TestEqual(TEXT("frame 105: still on 1, then it advances"), Hook.SeenTimelineFrames[5], 1);
		TestEqual(TEXT("frame 106: on 2"), Hook.SeenTimelineFrames[6], 2);
	}

	// The end moves by the stalled frames: 30 frames from 100 plus 3 is 133, as for a hitstop of the same length
	FGSTN_CombatSyncCore Ends = F2MakeRunning(100);
	FGSTN_CombatTimelineLogic::EvaluateStart(Ends, 100, Data, 1, Output);
	int32 EndFrame = INDEX_NONE;
	for (int32 Frame = 101; Frame <= 140 && EndFrame == INDEX_NONE; ++Frame)
	{
		FGSTN_CombatTimelineLogic::AdvanceClock(Ends, Frame, &Data, 1, true, Output);
		if (Ends.ActionState != EGSTN_ActionState::InTimeline)
		{
			EndFrame = Frame;
		}
	}
	TestEqual(TEXT("the timeline ends after its length plus the stall"), EndFrame, 133);

	// A hitstop frame and a stall on the same frame count once
	FGSTN_CombatSyncCore Both = F2MakeRunning(100);
	FGSTN_CombatTimelineLogic::EvaluateStart(Both, 100, Data, 1, Output);
	for (int32 Frame = 101; Frame <= 106; ++Frame)
	{
		FGSTN_CombatTimelineLogic::AdvanceClock(Both, Frame, &Data, 1, true, Output, Frame >= 103 && Frame <= 105);
	}
	TestEqual(TEXT("stalls on 102 to 104 and hitstop on 103 to 105: four frames in all"), Both.TimelinePausedFrames, 4);

	// A timeline without the track is not asked, and runs as if no hook existed
	const int32 SeenBefore = Hook.SeenTimelineFrames.Num();
	const FGSTN_CombatTimelineData Plain = F2MakeTimeline({});
	FGSTN_CombatSyncCore Other = F2MakeRunning(100);
	FGSTN_CombatTimelineLogic::EvaluateStart(Other, 100, Plain, 1, Output);
	for (int32 Frame = 101; Frame <= 106; ++Frame)
	{
		FGSTN_CombatTimelineLogic::AdvanceClock(Other, Frame, &Plain, 1, true, Output);
	}
	TestEqual(TEXT("the hook was not called"), Hook.SeenTimelineFrames.Num(), SeenBefore);
	TestEqual(TEXT("and nothing stalled"), Other.TimelinePausedFrames, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ClockHookStampsWindowTest, "GSTNCombat.NP.Clock.HookStampsWindow", GSTN_NP_TEST_FLAGS)
bool FGSTN_ClockHookStampsWindowTest::RunTest(const FString& Parameters)
{
	// The hook stamps a window from now to the end of the track while the timeline is inside the track, with profile 5, and never stalls
	F2TestTrackHook Hook([](const FGSTN_TrackHookContext& Context)
	{
		FGSTN_TrackHookResult Result;
		if (Context.Track.Contains(Context.TimelineFrame))
		{
			Result.bStampWindow = true;
			Result.Window.StartFrame = Context.Frame;
			Result.Window.EndFrame = Context.Frame + (Context.Track.EndFrame - Context.TimelineFrame);
			Result.Window.Profile = 5;
		}
		return Result;
	});
	F2HookScope Scope(Hook);

	const FGSTN_CombatTimelineData Data = F2MakeTimeline({ { 4, 8 } });
	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	FGSTN_CombatSyncCore Core = F2MakeRunning(100);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 1, Output);
	TestTrue(TEXT("the start frame is outside the track: no window"), Output.StampedWindows.IsEmpty());

	for (int32 Frame = 101; Frame <= 112; ++Frame)
	{
		Core.Requests.PruneBefore(Frame);
		FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 1, true, Output);
		// The hook sees the frame the timeline was on after the previous tick: Frame - 101 + ... = Frame - 1 - 100
		const int32 Seen = Frame - 1 - 100;
		const bool bInside = Seen >= 4 && Seen < 8;
		TestEqual(*FString::Printf(TEXT("frame %d: windows stamped"), Frame), Output.StampedWindows.Num(), bInside ? 1 : 0);
		if (bInside && Output.StampedWindows.Num() == 1)
		{
			TestEqual(*FString::Printf(TEXT("frame %d: starts now"), Frame), Output.StampedWindows[0].StartFrame, Frame);
			TestEqual(*FString::Printf(TEXT("frame %d: ends with the track"), Frame), Output.StampedWindows[0].EndFrame, Frame + (8 - Seen));
			TestEqual(*FString::Printf(TEXT("frame %d: the profile"), Frame), static_cast<int32>(Output.StampedWindows[0].Profile), 5);
		}
	}
	TestEqual(TEXT("a stamp is not a stall"), Core.TimelinePausedFrames, 0);

	// A window that opens on the first frame is stamped on it
	const FGSTN_CombatTimelineData FromStart = F2MakeTimeline({ { 0, 3 } });
	FGSTN_CombatSyncCore Fresh = F2MakeRunning(200);
	FGSTN_CombatTimelineLogic::EvaluateStart(Fresh, 200, FromStart, 1, Output);
	TestEqual(TEXT("frame 0 of the timeline stamps"), Output.StampedWindows.Num(), 1);
	TestEqual(TEXT("with the frame of the start"), Output.StampedWindows.IsEmpty() ? INDEX_NONE : Output.StampedWindows[0].StartFrame, 200);

	// A timeline carries at most two windows a frame
	const FGSTN_CombatTimelineData Three = F2MakeTimeline({ { 0, 3 }, { 0, 4 }, { 0, 5 } });
	FGSTN_CombatSyncCore Crowded = F2MakeRunning(300);
	FGSTN_CombatTimelineLogic::EvaluateStart(Crowded, 300, Three, 1, Output);
	TestEqual(TEXT("three stamping tracks, two windows kept"), Output.StampedWindows.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TickBlocksActionsVetoTest, "GSTNCombat.NP.Tick.BlocksActionsVeto", GSTN_NP_TEST_FLAGS)
bool FGSTN_TickBlocksActionsVetoTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatSyncState In;
	FGSTN_CombatSyncState Out;
	FGSTN_CombatTickContext Early(5, In, Out);
	FGSTN_CombatTickContext Late(50, In, Out);

	{
		const FGSTN_CombatTickExtensionRegistry Registry;
		TestFalse(TEXT("nothing registered never blocks"), Registry.BlocksActions(Late));
	}
	{
		FGSTN_CombatTickExtensionRegistry Registry;
		F2PlainExtension Plain;
		Registry.Register(&Plain);
		TestFalse(TEXT("an extension that does not override the veto never blocks"), Registry.BlocksActions(Late));
	}
	{
		FGSTN_CombatTickExtensionRegistry Registry;
		F2VetoExtension Blocker(3, 10);
		F2PlainExtension Plain;
		Registry.Register(&Plain);
		Registry.Register(&Blocker);
		TestFalse(TEXT("before the frame it blocks from"), Registry.BlocksActions(Early));
		TestTrue(TEXT("from it on, one extension is enough"), Registry.BlocksActions(Late));
		TestEqual(TEXT("asked once per question"), Blocker.Asked, 2);
	}
	{
		// Every extension is asked, so an answer does not depend on the order of the others
		FGSTN_CombatTickExtensionRegistry Registry;
		F2VetoExtension First(3, 10);
		F2VetoExtension Second(4, 10);
		Registry.Register(&First);
		Registry.Register(&Second);
		TestTrue(TEXT("two blockers"), Registry.BlocksActions(Late));
		TestTrue(TEXT("both were asked"), First.Asked == 1 && Second.Asked == 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VictimOutgoingEntriesPostedTest, "GSTNCombat.NP.Victim.OutgoingEntriesPosted", GSTN_NP_TEST_FLAGS)
bool FGSTN_VictimOutgoingEntriesPostedTest::RunTest(const FString& Parameters)
{
	F2OutgoingFixture Fixture;
	Fixture.Entries = { F2OutgoingFixture::MakeEntry(10, 3), F2OutgoingFixture::MakeEntry(10, 5) };
	Fixture.Run(11);

	TestEqual(TEXT("both hits resolved"), Fixture.Result.Hits.Num(), 2);
	TestEqual(TEXT("each hit sent its entry back"), Fixture.Result.Outgoing.Num(), 2);
	if (Fixture.Result.Outgoing.Num() != 2)
	{
		return false;
	}
	TestEqual(TEXT("the first goes to the first attacker"), Fixture.Result.Outgoing[0].TargetId, 3);
	TestEqual(TEXT("the second to the second"), Fixture.Result.Outgoing[1].TargetId, 5);
	for (const FGSTN_VictimOutgoing& Reply : Fixture.Result.Outgoing)
	{
		TestEqual(TEXT("stamped with the frame the victim resolved on"), Reply.Entry.Frame, 11);
		TestEqual(TEXT("and the victim as the sender"), Reply.Entry.SenderId, F2OutgoingFixture::F2VictimId);
		TestEqual(TEXT("of the kind the step chose"), static_cast<int32>(Reply.Entry.Kind), static_cast<int32>(GSTNInboxKinds::FirstDefence));
		const FGSTN_TestVictimExtension* Payload = Reply.Entry.Payload.GetPtr<FGSTN_TestVictimExtension>();
		TestEqual(TEXT("with its payload"), Payload ? Payload->Value : 0, 7);
	}

	// A ricochet is only the victim's cue: nothing is sent back for it
	F2OutgoingFixture Ricochet;
	Ricochet.Entries = { F2OutgoingFixture::MakeEntry(10, 3) };
	Ricochet.Entries[0].bRicochet = true;
	Ricochet.Run(11);
	TestTrue(TEXT("a ricochet sends nothing"), Ricochet.Result.Outgoing.IsEmpty());

	// The result is the step's, not an accumulation: the next frame without hits has none
	Fixture.Entries.Reset();
	Fixture.Run(12);
	TestTrue(TEXT("a frame without hits starts empty"), Fixture.Result.Outgoing.IsEmpty());

	// Posting: the entry reaches an attacker that ticks here, one frame later, and an interpolated proxy takes nothing
	F2OutgoingFixture Again;
	Again.Entries = { F2OutgoingFixture::MakeEntry(10, 3) };
	Again.Run(11);
	AActor* Owner = NewObject<AActor>(GetTransientPackage());
	Owner->SetRole(ROLE_Authority);
	UGSTN_CombatSimComponent* Attacker = NewObject<UGSTN_CombatSimComponent>(Owner);
	for (const FGSTN_VictimOutgoing& Reply : Again.Result.Outgoing)
	{
		TestTrue(TEXT("the attacker that ticks here takes it"), Attacker->PostToInbox(Reply.Entry));
	}
	TestEqual(TEXT("it waits in the attacker's inbox"), Attacker->GetInbox().Num(), 1);
	// The delivery gate is the container's: an entry stamped 11 is drained by the tick of frame 12, not by the one it was written in
	FGSTN_CombatInbox Gate;
	for (const FGSTN_InboxEntry& Waiting : Attacker->GetInbox().GetEntries())
	{
		Gate.Add(Waiting);
	}
	TArray<FGSTN_InboxEntry> Drained;
	Gate.Drain(11, Drained);
	TestTrue(TEXT("it is not delivered on the frame it was written"), Drained.IsEmpty());
	Gate.Drain(12, Drained);
	TestEqual(TEXT("it is on the next one"), Drained.Num(), 1);
	TestEqual(TEXT("from the victim"), Drained.IsEmpty() ? INDEX_NONE : Drained[0].SenderId, F2OutgoingFixture::F2VictimId);

	Owner->SetRole(ROLE_SimulatedProxy);
	UGSTN_CombatSimComponent* Remote = NewObject<UGSTN_CombatSimComponent>(Owner);
	TestFalse(TEXT("an attacker that does not tick here takes nothing"), Remote->PostToInbox(Again.Result.Outgoing[0].Entry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RequestsEightSlotsRoundTripTest, "GSTNCombat.NP.Requests.EightSlotsRoundTrip", GSTN_NP_TEST_FLAGS)
bool FGSTN_RequestsEightSlotsRoundTripTest::RunTest(const FString& Parameters)
{
	// The array already holds eight (the plan asked for six): this checks it, and that the generic kinds of the link plugin ride in it
	TestEqual(TEXT("the capacity is eight slots"), FGSTN_CombatRequests::Capacity, 8);
	TestTrue(TEXT("every kind fits the five bits of the wire"), static_cast<int32>(EGSTN_RequestType::Count) <= 32);

	const EGSTN_RequestType Kinds[] =
	{
		EGSTN_RequestType::EnterLinked, EGSTN_RequestType::ExitLink, EGSTN_RequestType::ApplyLinkEffect, EGSTN_RequestType::RemoveLinkEffect,
		EGSTN_RequestType::LaunchLinked, EGSTN_RequestType::ActivateEntry, EGSTN_RequestType::Cue, EGSTN_RequestType::EndReaction
	};

	// Bits on the wire for the first Count slots set, all of one frame and one size so the slots cost the same
	auto BitsFor = [&](int32 Count, FGSTN_CombatRequests* OutRequests = nullptr) -> int64
	{
		FGSTN_CombatRequests Requests;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			TestTrue(TEXT("room for the slot"), Requests.Add(Kinds[Index], 400, static_cast<int16>(1000 + Index)));
		}
		FBitWriter Writer(512, true);
		Requests.NetSerialize(Writer);
		if (OutRequests)
		{
			*OutRequests = Requests;
		}
		return Writer.GetNumBits();
	};

	const int64 Empty = BitsFor(0);
	const int64 One = BitsFor(1);
	const int64 Two = BitsFor(2);
	const int64 Slot = One - Empty;
	TestEqual(TEXT("an empty array is the four bits of the count"), Empty, static_cast<int64>(4));
	TestEqual(TEXT("a second slot costs what the first did"), Two - One, Slot);
	TestEqual(TEXT("eight slots cost eight times one: an absent slot is free"), BitsFor(8), Empty + 8 * Slot);

	// Round trip with every slot used, the five new kinds among them
	FGSTN_CombatRequests Original;
	BitsFor(8, &Original);
	TestEqual(TEXT("eight set"), Original.Num(), 8);
	FBitWriter Writer(512, true);
	Original.NetSerialize(Writer);
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_CombatRequests Loaded;
	Loaded.NetSerialize(Reader);
	TestFalse(TEXT("the read did not fail"), Reader.IsError());
	TestTrue(TEXT("the loaded array equals the written one"), Loaded == Original);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		TestEqual(*FString::Printf(TEXT("slot %d: kind"), Index), static_cast<int32>(Loaded.Slots[Index].Type), static_cast<int32>(Kinds[Index]));
		TestEqual(*FString::Printf(TEXT("slot %d: argument"), Index), static_cast<int32>(Loaded.Slots[Index].Arg), 1000 + Index);
	}
	TestFalse(TEXT("a ninth does not fit"), Loaded.Add(EGSTN_RequestType::Cue, 400, 1));

	// The data kinds have no handler: a request of one is carried and pruned like any other
	FGSTN_CombatRequests Link;
	Link.Add(EGSTN_RequestType::EnterLinked, 10, 2);
	Link.PruneBefore(11);
	TestTrue(TEXT("a link request of a past frame is dropped like any other"), Link.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
