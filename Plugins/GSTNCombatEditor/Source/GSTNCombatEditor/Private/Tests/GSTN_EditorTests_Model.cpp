#include "Animation/AnimMontage.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Misc/AutomationTest.h"
#include "Tests/GSTN_EditorTestTypes.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_KeyedTrackAccess.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineUndo.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

/** Flags of every model test: they need GEditor (the transaction buffer), so the editor context only */
#define GSTN_EDITOR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNEditorModelTest
{
	using FTrackInstance = TInstancedStruct<FGSTN_TimelineTrack>;

	/** The whole data as text: what two copies must share to be "exactly the same data" */
	FString Export(const FGSTN_CombatTimelineData& Data)
	{
		FString Text;
		FGSTN_CombatTimelineData::StaticStruct()->ExportText(Text, &Data, nullptr, nullptr, PPF_None, nullptr);
		return Text;
	}

	template <typename T>
	FTrackInstance MakeTrack(int32 Start, int32 End)
	{
		T Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		return FTrackInstance::Make<T>(Track);
	}

	FTrackInstance MakeSwitch(int32 SwapFrame)
	{
		FGSTN_TimelineTrack_WeaponSwitch Track;
		Track.StartFrame = SwapFrame;
		Track.EndFrame = SwapFrame + 1;
		Track.SwapFrame = SwapFrame;
		return FTrackInstance::Make<FGSTN_TimelineTrack_WeaponSwitch>(Track);
	}

	FGSTN_TestKey MakeKey(int32 Frame, float X, float Radius)
	{
		FGSTN_TestKey Key;
		Key.Frame = Frame;
		Key.Location = FVector(X, 0.0, 0.0);
		Key.Radius = Radius;
		return Key;
	}

	/** Track indices of the fixture */
	constexpr int32 TagsTrack = 0;
	constexpr int32 CancelTrack = 1;
	constexpr int32 MovementTrack = 2;
	constexpr int32 EventTrack = 3;
	constexpr int32 SwitchTrack = 4;
	constexpr int32 KeyedTrack = 5;

	/** 30 frames, actionable from 12; tags [0,12), cancel [8,14), movement [4,10), event at 5, switch at 6, keyed [2,20) with keys at 2, 10 and 18 */
	void FillData(FGSTN_CombatTimelineData& Data)
	{
		Data.TotalFrames = 30;
		Data.FirstActionableFrame = 12;
		Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_Tags>(0, 12));
		Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_CancelWindow>(8, 14));
		Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_Movement>(4, 10));
		Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_Event>(5, 6));
		Data.Tracks.Add(MakeSwitch(6));
		FGSTN_TestKeyedTrack Keyed;
		Keyed.StartFrame = 2;
		Keyed.EndFrame = 20;
		Keyed.Keys = { MakeKey(2, 0.f, 10.f), MakeKey(10, 100.f, 20.f), MakeKey(18, 200.f, 30.f) };
		Data.Tracks.Add(FTrackInstance::Make<FGSTN_TestKeyedTrack>(Keyed));
	}

	/** A transient timeline, a model on it, the undo client the toolkit will own, and the test keyed track's adapter registered */
	struct FFixture
	{
		TStrongObjectPtr<UGSTN_CombatTimeline> Timeline;
		TSharedRef<FGSTN_TestKeyedAccess> Access = MakeShared<FGSTN_TestKeyedAccess>();
		TUniquePtr<FGSTN_TimelineEditorModel> Model;
		TUniquePtr<FGSTN_TimelineUndoClient> UndoClient;

		/** Fixtures nest (a test pastes from one timeline into another): the adapter stays registered until the last one is gone */
		static inline int32 LiveFixtures = 0;

		FFixture()
		{
			// Transactional, or Modify() records nothing and undo has nothing to restore
			Timeline.Reset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
			FillData(Timeline->Data);
			++LiveFixtures;
			FGSTN_KeyedTrackAccessRegistry::Register(FGSTN_TestKeyedTrack::StaticStruct(), Access);
			Model = MakeUnique<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
			Model->SetTickRate(62.5f);
			UndoClient = MakeUnique<FGSTN_TimelineUndoClient>(*Model);
		}

		~FFixture()
		{
			UndoClient.Reset();
			Model.Reset();
			if (--LiveFixtures == 0)
			{
				FGSTN_KeyedTrackAccessRegistry::Unregister(FGSTN_TestKeyedTrack::StaticStruct());
			}
		}

		FGSTN_CombatTimelineData& Data() { return Timeline->Data; }

		FGSTN_TestKeyedTrack& Keyed() { return Data().Tracks[KeyedTrack].GetMutable<FGSTN_TestKeyedTrack>(); }
	};

	/** Steps of the undo history that are currently applied: grows by one per finished transaction, shrinks per undo */
	int32 AppliedSteps()
	{
		return (GEditor && GEditor->Trans) ? GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() : 0;
	}

	bool HasEditorHistory()
	{
		return GEditor != nullptr && GEditor->Trans != nullptr;
	}
}

using namespace GSTNEditorModelTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ModelOnlyChangesWhatItSaysTest, "GSTNCombat.Editor.Model.EachMutationOnlyChangesWhatItSays", GSTN_EDITOR_TEST_FLAGS)
bool FGSTN_ModelOnlyChangesWhatItSaysTest::RunTest(const FString& Parameters)
{
	// Every mutation on a fresh timeline, compared as text with the same change applied by hand to a copy: any other field
	// that moved (a neighbouring track, the other end of a window, the total) makes the texts differ
	auto Expect = [this](const TCHAR* Name, TFunctionRef<void(FFixture&)> Mutate, TFunctionRef<void(FGSTN_CombatTimelineData&)> ApplyByHand)
	{
		FFixture Fixture;
		FGSTN_CombatTimelineData Expected = Fixture.Data();
		const FString Before = Export(Fixture.Data());
		ApplyByHand(Expected);
		Mutate(Fixture);
		TestEqual(Name, Export(Fixture.Data()), Export(Expected));
		TestNotEqual(*FString::Printf(TEXT("%s changed something"), Name), Export(Fixture.Data()), Before);
	};

	Expect(TEXT("AddTrack"),
		[](FFixture& F) { F.Model->AddTrack(FGSTN_TimelineTrack_Tags::StaticStruct(), 3, 4); },
		[](FGSTN_CombatTimelineData& D) { D.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_Tags>(3, 7)); });
	Expect(TEXT("AddTrack weapon switch carries its swap frame"),
		[](FFixture& F) { F.Model->AddTrack(FGSTN_TimelineTrack_WeaponSwitch::StaticStruct(), 9, 1); },
		[](FGSTN_CombatTimelineData& D) { D.Tracks.Add(MakeSwitch(9)); });
	Expect(TEXT("RemoveTracks"),
		[](FFixture& F) { F.Model->RemoveTracks({ CancelTrack, EventTrack }); },
		[](FGSTN_CombatTimelineData& D) { D.Tracks.RemoveAt(EventTrack); D.Tracks.RemoveAt(CancelTrack); });
	Expect(TEXT("MoveWindow"),
		[](FFixture& F) { F.Model->MoveWindow(TagsTrack, 5); },
		[](FGSTN_CombatTimelineData& D) { D.Tracks[TagsTrack] = MakeTrack<FGSTN_TimelineTrack_Tags>(5, 17); });
	Expect(TEXT("MoveWindow of a weapon switch moves its swap frame"),
		[](FFixture& F) { F.Model->MoveWindow(SwitchTrack, 3); },
		[](FGSTN_CombatTimelineData& D) { D.Tracks[SwitchTrack] = MakeSwitch(9); });
	Expect(TEXT("ResizeWindow end"),
		[](FFixture& F) { F.Model->ResizeWindow(CancelTrack, EGSTN_WindowEdge::End, 20); },
		[](FGSTN_CombatTimelineData& D) { D.Tracks[CancelTrack] = MakeTrack<FGSTN_TimelineTrack_CancelWindow>(8, 20); });
	Expect(TEXT("ResizeWindow start"),
		[](FFixture& F) { F.Model->ResizeWindow(CancelTrack, EGSTN_WindowEdge::Start, 2); },
		[](FGSTN_CombatTimelineData& D) { D.Tracks[CancelTrack] = MakeTrack<FGSTN_TimelineTrack_CancelWindow>(2, 14); });
	Expect(TEXT("SetTotalFrames"),
		[](FFixture& F) { F.Model->SetTotalFrames(40); },
		[](FGSTN_CombatTimelineData& D) { D.TotalFrames = 40; });
	Expect(TEXT("SetFirstActionableFrame"),
		[](FFixture& F) { F.Model->SetFirstActionableFrame(15); },
		[](FGSTN_CombatTimelineData& D) { D.FirstActionableFrame = 15; });
	Expect(TEXT("ApplyTrackEdit"),
		[](FFixture& F) { F.Model->ApplyTrackEdit(CancelTrack, [](FGSTN_TimelineTrack& T) { static_cast<FGSTN_TimelineTrack_CancelWindow&>(T).Condition = EGSTN_CancelCondition::OnHit; }, FText::GetEmpty()); },
		[](FGSTN_CombatTimelineData& D) { D.Tracks[CancelTrack].GetMutable<FGSTN_TimelineTrack_CancelWindow>().Condition = EGSTN_CancelCondition::OnHit; });
	Expect(TEXT("ApplyDataEdit"),
		[](FFixture& F) { F.Model->ApplyDataEdit([](FGSTN_CombatTimelineData& D) { D.AimPolicy = EGSTN_AimPolicy::Camera; }, FText::GetEmpty()); },
		[](FGSTN_CombatTimelineData& D) { D.AimPolicy = EGSTN_AimPolicy::Camera; });

	// Keys. The added key lies on the curve between its neighbours, so it adds a key and changes no value
	Expect(TEXT("AddKey interpolates"),
		[](FFixture& F) { F.Model->AddKey(KeyedTrack, 6); },
		[](FGSTN_CombatTimelineData& D) { D.Tracks[KeyedTrack].GetMutable<FGSTN_TestKeyedTrack>().Keys = { MakeKey(2, 0.f, 10.f), MakeKey(6, 50.f, 15.f), MakeKey(10, 100.f, 20.f), MakeKey(18, 200.f, 30.f) }; });
	Expect(TEXT("MoveKeys"),
		[](FFixture& F) { F.Model->MoveKeys({ FGSTN_Selection::Key(KeyedTrack, 1) }, 3); },
		[](FGSTN_CombatTimelineData& D) { D.Tracks[KeyedTrack].GetMutable<FGSTN_TestKeyedTrack>().Keys[1].Frame = 13; });
	Expect(TEXT("MoveKeys past a neighbour re-sorts"),
		[](FFixture& F) { F.Model->MoveKeys({ FGSTN_Selection::Key(KeyedTrack, 0) }, 12); },
		[](FGSTN_CombatTimelineData& D) { D.Tracks[KeyedTrack].GetMutable<FGSTN_TestKeyedTrack>().Keys = { MakeKey(10, 100.f, 20.f), MakeKey(14, 0.f, 10.f), MakeKey(18, 200.f, 30.f) }; });
	Expect(TEXT("DuplicateKeys"),
		[](FFixture& F) { F.Model->DuplicateKeys({ FGSTN_Selection::Key(KeyedTrack, 0) }, 4); },
		[](FGSTN_CombatTimelineData& D) { D.Tracks[KeyedTrack].GetMutable<FGSTN_TestKeyedTrack>().Keys = { MakeKey(2, 0.f, 10.f), MakeKey(6, 0.f, 10.f), MakeKey(10, 100.f, 20.f), MakeKey(18, 200.f, 30.f) }; });
	Expect(TEXT("RemoveKeys"),
		[](FFixture& F) { F.Model->RemoveKeys({ FGSTN_Selection::Key(KeyedTrack, 2) }); },
		[](FGSTN_CombatTimelineData& D) { D.Tracks[KeyedTrack].GetMutable<FGSTN_TestKeyedTrack>().Keys.RemoveAt(2); });
	Expect(TEXT("SetKeyTransform"),
		[](FFixture& F) { F.Model->SetKeyTransform(KeyedTrack, 1, FTransform(FQuat::Identity, FVector(7.0, 8.0, 9.0), FVector(33.0))); },
		[](FGSTN_CombatTimelineData& D) { FGSTN_TestKey& Key = D.Tracks[KeyedTrack].GetMutable<FGSTN_TestKeyedTrack>().Keys[1]; Key.Location = FVector(7.0, 8.0, 9.0); Key.Radius = 33.f; });

	// A track without keys has no key gestures: they do nothing and say so
	{
		FFixture Fixture;
		const FString Before = Export(Fixture.Data());
		TestEqual(TEXT("AddKey on a track without keys"), Fixture.Model->AddKey(TagsTrack, 3), static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("MoveKeys on a track without keys"), Fixture.Model->MoveKeys({ FGSTN_Selection::Key(TagsTrack, 0) }, 2), 0);
		TestEqual(TEXT("data untouched"), Export(Fixture.Data()), Before);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ModelClampsTest, "GSTNCombat.Editor.Model.ClampsToTotalFrames", GSTN_EDITOR_TEST_FLAGS)
bool FGSTN_ModelClampsTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), HasEditorHistory()))
	{
		return false;
	}
	FFixture Fixture;
	FGSTN_TimelineEditorModel& Model = *Fixture.Model;

	// A window keeps its length and stays inside [0, Total]
	Model.MoveWindow(CancelTrack, 100);
	TestEqual(TEXT("moved to the end: start"), Fixture.Data().Tracks[CancelTrack].Get<FGSTN_TimelineTrack>().StartFrame, 24);
	TestEqual(TEXT("moved to the end: end"), Fixture.Data().Tracks[CancelTrack].Get<FGSTN_TimelineTrack>().EndFrame, 30);
	Model.MoveWindow(CancelTrack, -100);
	TestEqual(TEXT("moved to the start: start"), Fixture.Data().Tracks[CancelTrack].Get<FGSTN_TimelineTrack>().StartFrame, 0);
	TestEqual(TEXT("moved to the start: end"), Fixture.Data().Tracks[CancelTrack].Get<FGSTN_TimelineTrack>().EndFrame, 6);

	// An edge cannot cross the other one, or leave the timeline
	Model.ResizeWindow(CancelTrack, EGSTN_WindowEdge::End, 100);
	TestEqual(TEXT("end edge stops at the total"), Fixture.Data().Tracks[CancelTrack].Get<FGSTN_TimelineTrack>().EndFrame, 30);
	Model.ResizeWindow(CancelTrack, EGSTN_WindowEdge::Start, 100);
	TestEqual(TEXT("start edge stops one frame before the end"), Fixture.Data().Tracks[CancelTrack].Get<FGSTN_TimelineTrack>().StartFrame, 29);
	Model.ResizeWindow(CancelTrack, EGSTN_WindowEdge::End, -5);
	TestEqual(TEXT("end edge keeps one frame"), Fixture.Data().Tracks[CancelTrack].Get<FGSTN_TimelineTrack>().EndFrame, 30);
	Model.ResizeWindow(CancelTrack, EGSTN_WindowEdge::Start, -5);
	TestEqual(TEXT("start edge stops at zero"), Fixture.Data().Tracks[CancelTrack].Get<FGSTN_TimelineTrack>().StartFrame, 0);

	// A track that fires on one frame stays on a real frame and has no edges
	Model.MoveWindow(EventTrack, 100);
	TestEqual(TEXT("event stops on the last frame"), Fixture.Data().Tracks[EventTrack].Get<FGSTN_TimelineTrack>().StartFrame, 29);
	TestFalse(TEXT("an event has no edge to resize"), Model.ResizeWindow(EventTrack, EGSTN_WindowEdge::End, 3));
	Model.MoveWindow(SwitchTrack, 100);
	TestEqual(TEXT("switch window stops on the last frame"), Fixture.Data().Tracks[SwitchTrack].Get<FGSTN_TimelineTrack>().StartFrame, 29);
	TestEqual(TEXT("the swap frame travelled with it"), Fixture.Data().Tracks[SwitchTrack].Get<FGSTN_TimelineTrack_WeaponSwitch>().SwapFrame, 29);

	// New tracks land inside the timeline
	const int32 Added = Model.AddTrack(FGSTN_TimelineTrack_Movement::StaticStruct(), 100, 50);
	const FGSTN_TimelineTrack& AddedTrack = Fixture.Data().Tracks[Added].Get<FGSTN_TimelineTrack>();
	TestEqual(TEXT("added track starts on the last frame"), AddedTrack.StartFrame, 29);
	TestEqual(TEXT("added track ends at the total"), AddedTrack.EndFrame, 30);
	TestEqual(TEXT("a struct that is not a track is refused"), Model.AddTrack(FGSTN_CombatTimelineData::StaticStruct()), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("so is no struct"), Model.AddTrack(nullptr), static_cast<int32>(INDEX_NONE));

	// First actionable frame and playhead
	Model.SetFirstActionableFrame(100);
	TestEqual(TEXT("first actionable frame stops at the total"), Fixture.Data().FirstActionableFrame, 30);
	Model.SetFirstActionableFrame(-3);
	TestEqual(TEXT("first actionable frame stops at zero"), Fixture.Data().FirstActionableFrame, 0);
	Model.SetPlayhead(100);
	TestEqual(TEXT("playhead stops on the last frame"), Model.GetPlayhead(), 29);
	Model.SetPlayhead(-4);
	TestEqual(TEXT("playhead stops at zero"), Model.GetPlayhead(), 0);

	// Keys stay on [0, Total - 1], and the group keeps its shape (the nearest key to the wall decides)
	Model.MoveKeys({ FGSTN_Selection::Key(KeyedTrack, 0), FGSTN_Selection::Key(KeyedTrack, 2) }, 100);
	TestEqual(TEXT("the last key stops on the last frame"), Fixture.Keyed().Keys[2].Frame, 29);
	TestEqual(TEXT("the first key kept its distance"), Fixture.Keyed().Keys[1].Frame, 13);
	TestEqual(TEXT("the untouched key stays"), Fixture.Keyed().Keys[0].Frame, 10);
	TestEqual(TEXT("AddKey outside the timeline is refused"), Model.AddKey(KeyedTrack, 30), static_cast<int32>(INDEX_NONE));

	// A request that clamps to nothing is not a change: no data change, no undo step
	const int32 StepsBefore = AppliedSteps();
	const FString DataBefore = Export(Fixture.Data());
	TestFalse(TEXT("a window against the wall does not move"), Model.MoveWindow(EventTrack, 5));
	TestFalse(TEXT("a key group against the wall does not move"), Model.MoveKeys({ FGSTN_Selection::Key(KeyedTrack, 2) }, 5) > 0);
	TestFalse(TEXT("setting the same total is not a change"), Model.SetTotalFrames(30));
	TestEqual(TEXT("no undo step for requests that change nothing"), AppliedSteps(), StepsBefore);
	TestEqual(TEXT("data unchanged"), Export(Fixture.Data()), DataBefore);

	// A timeline without a length cannot clamp to one, but never goes below zero
	{
		FFixture Empty;
		Empty.Data().Tracks.Reset();
		Empty.Data().TotalFrames = 0;
		Empty.Data().FirstActionableFrame = 0;
		const int32 Index = Empty.Model->AddTrack(FGSTN_TimelineTrack_Tags::StaticStruct(), 3, 5);
		TestEqual(TEXT("unbounded timeline: start"), Empty.Data().Tracks[Index].Get<FGSTN_TimelineTrack>().StartFrame, 3);
		TestEqual(TEXT("unbounded timeline: end"), Empty.Data().Tracks[Index].Get<FGSTN_TimelineTrack>().EndFrame, 8);
		Empty.Model->MoveWindow(Index, -10);
		TestEqual(TEXT("unbounded timeline still stops at zero"), Empty.Data().Tracks[Index].Get<FGSTN_TimelineTrack>().StartFrame, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ModelUndoRedoTest, "GSTNCombat.Editor.Model.UndoRedoRestoresExactData", GSTN_EDITOR_TEST_FLAGS)
bool FGSTN_ModelUndoRedoTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), HasEditorHistory()))
	{
		return false;
	}

	FFixture Fixture;
	FGSTN_TimelineEditorModel& Model = *Fixture.Model;
	const int32 StartSteps = AppliedSteps();

	// The fit command needs a montage with a length; the data gets it directly so the setup is not part of the history
	TStrongObjectPtr<UAnimMontage> Montage(NewObject<UAnimMontage>(GetTransientPackage()));
	Montage->SetCompositeLength(1.0f);
	Fixture.Data().Montage = Montage.Get();
	TestTrue(TEXT("the test montage has a length"), Montage->GetPlayLength() > 0.f);

	struct FStep
	{
		const TCHAR* Name;
		TFunction<bool()> Run;
	};
	const TArray<FStep> Steps = {
		{ TEXT("AddTrack"), [&]() { return Model.AddTrack(FGSTN_TimelineTrack_Event::StaticStruct(), 9, 1) != INDEX_NONE; } },
		{ TEXT("MoveWindow"), [&]() { return Model.MoveWindow(TagsTrack, 4); } },
		{ TEXT("ResizeWindow"), [&]() { return Model.ResizeWindow(CancelTrack, EGSTN_WindowEdge::End, 22); } },
		{ TEXT("SetTotalFrames"), [&]() { return Model.SetTotalFrames(44); } },
		{ TEXT("SetFirstActionableFrame"), [&]() { return Model.SetFirstActionableFrame(17); } },
		{ TEXT("FitTotalFramesToMontage"), [&]() { return Model.FitTotalFramesToMontage(); } },
		{ TEXT("ApplyTrackEdit"), [&]() { return Model.ApplyTrackEdit(CancelTrack, [](FGSTN_TimelineTrack& T) { static_cast<FGSTN_TimelineTrack_CancelWindow&>(T).Condition = EGSTN_CancelCondition::OnBlock; }, FText::GetEmpty()); } },
		{ TEXT("ApplyDataEdit"), [&]() { return Model.ApplyDataEdit([](FGSTN_CombatTimelineData& D) { D.MontagePlayRate = 1.5f; }, FText::GetEmpty()); } },
		{ TEXT("AddKey"), [&]() { return Model.AddKey(KeyedTrack, 14) != INDEX_NONE; } },
		{ TEXT("MoveKeys"), [&]() { return Model.MoveKeys({ FGSTN_Selection::Key(KeyedTrack, 0) }, 5) > 0; } },
		{ TEXT("DuplicateKeys"), [&]() { return Model.DuplicateKeys({ FGSTN_Selection::Key(KeyedTrack, 0) }, 3) > 0; } },
		{ TEXT("SetKeyTransform"), [&]() { return Model.SetKeyTransform(KeyedTrack, 1, FTransform(FQuat::Identity, FVector(1.0, 2.0, 3.0), FVector(5.0))); } },
		{ TEXT("RemoveKeys"), [&]() { return Model.RemoveKeys({ FGSTN_Selection::Key(KeyedTrack, 1) }) > 0; } },
		{ TEXT("PasteKeys"), [&]() { return Model.PasteKeysFromText(KeyedTrack, 20, Model.CopyKeysToText({ FGSTN_Selection::Key(KeyedTrack, 0) })) > 0; } },
		{ TEXT("RemoveTracks"), [&]() { return Model.RemoveTracks({ MovementTrack, EventTrack }) == 2; } },
	};

	for (const FStep& Step : Steps)
	{
		const FString Before = Export(Fixture.Data());
		const int32 StepsBefore = AppliedSteps();
		if (!TestTrue(FString::Printf(TEXT("%s runs"), Step.Name), Step.Run()))
		{
			continue;
		}
		const FString After = Export(Fixture.Data());
		TestNotEqual(*FString::Printf(TEXT("%s changed the data"), Step.Name), After, Before);
		TestEqual(FString::Printf(TEXT("%s is one undo step"), Step.Name), AppliedSteps(), StepsBefore + 1);

		TestTrue(FString::Printf(TEXT("%s: undo"), Step.Name), GEditor->UndoTransaction());
		TestEqual(*FString::Printf(TEXT("%s: undo restores the exact data"), Step.Name), Export(Fixture.Data()), Before);
		TestTrue(FString::Printf(TEXT("%s: redo"), Step.Name), GEditor->RedoTransaction());
		TestEqual(*FString::Printf(TEXT("%s: redo restores the exact data"), Step.Name), Export(Fixture.Data()), After);
	}

	// Undo everything that is left of this test's steps, so the editor's history ends where it began
	while (AppliedSteps() > StartSteps && GEditor->UndoTransaction())
	{
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ModelDragTest, "GSTNCombat.Editor.Model.DragIsOneUndoStep", GSTN_EDITOR_TEST_FLAGS)
bool FGSTN_ModelDragTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), HasEditorHistory()))
	{
		return false;
	}
	FFixture Fixture;
	FGSTN_TimelineEditorModel& Model = *Fixture.Model;

	const FString Before = Export(Fixture.Data());
	const int32 StepsBefore = AppliedSteps();

	int32 DataChangedEvents = 0;
	Model.OnDataChanged.AddLambda([&DataChangedEvents]() { ++DataChangedEvents; });

	TestTrue(TEXT("the drag begins"), Model.BeginDrag(INVTEXT("Drag window")));
	TestFalse(TEXT("a second drag cannot begin inside the first"), Model.BeginDrag(INVTEXT("Nested")));
	TestEqual(TEXT("mouse-down alone opens no step"), AppliedSteps(), StepsBefore);
	for (int32 Move = 0; Move < 6; ++Move)
	{
		Model.MoveWindow(CancelTrack, 1);
	}
	Model.ResizeWindow(CancelTrack, EGSTN_WindowEdge::End, 25);
	TestEqual(TEXT("the live drag is applied to the data"), Fixture.Data().Tracks[CancelTrack].Get<FGSTN_TimelineTrack>().StartFrame, 14);
	TestTrue(TEXT("every mouse move told the listeners"), DataChangedEvents >= 7);
	Model.EndDrag();
	TestFalse(TEXT("the drag is over"), Model.IsDragging());

	TestEqual(TEXT("seven edits are one undo step"), AppliedSteps(), StepsBefore + 1);
	const FString After = Export(Fixture.Data());
	TestNotEqual(TEXT("the drag changed the data"), After, Before);
	TestTrue(TEXT("one undo"), GEditor->UndoTransaction());
	TestEqual(TEXT("one undo restores the data from before the drag"), Export(Fixture.Data()), Before);
	TestTrue(TEXT("one redo"), GEditor->RedoTransaction());
	TestEqual(TEXT("one redo restores the end of the drag"), Export(Fixture.Data()), After);
	GEditor->UndoTransaction();

	// The edits after a drag are ordinary steps again
	const int32 StepsAfterDrag = AppliedSteps();
	Model.MoveWindow(TagsTrack, 1);
	TestEqual(TEXT("an edit after the drag is its own step"), AppliedSteps(), StepsAfterDrag + 1);
	GEditor->UndoTransaction();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ModelCancelledDragTest, "GSTNCombat.Editor.Model.CancelledDragLeavesNoStep", GSTN_EDITOR_TEST_FLAGS)
bool FGSTN_ModelCancelledDragTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), HasEditorHistory()))
	{
		return false;
	}
	FFixture Fixture;
	FGSTN_TimelineEditorModel& Model = *Fixture.Model;
	const FString Before = Export(Fixture.Data());
	const int32 StepsBefore = AppliedSteps();

	// Escape: the data goes back and the history is untouched
	Model.BeginDrag(INVTEXT("Drag window"));
	Model.MoveWindow(CancelTrack, 7);
	Model.ResizeWindow(CancelTrack, EGSTN_WindowEdge::End, 29);
	TestNotEqual(TEXT("the drag changed the live data"), Export(Fixture.Data()), Before);
	Model.CancelDrag();
	TestFalse(TEXT("the drag is over"), Model.IsDragging());
	TestEqual(TEXT("cancel restores the data exactly"), Export(Fixture.Data()), Before);
	TestEqual(TEXT("cancel leaves no undo step"), AppliedSteps(), StepsBefore);

	// A click that moves nothing leaves no step either, and neither does a drag that only meets the clamp
	Model.BeginDrag(INVTEXT("Click"));
	Model.EndDrag();
	TestEqual(TEXT("a click without a change leaves no step"), AppliedSteps(), StepsBefore);
	Model.BeginDrag(INVTEXT("Push the wall"));
	Model.MoveWindow(EventTrack, 0);
	Model.MoveWindow(EventTrack, 0);
	Model.EndDrag();
	TestEqual(TEXT("a drag that changes nothing leaves no step"), AppliedSteps(), StepsBefore);
	TestEqual(TEXT("and no change"), Export(Fixture.Data()), Before);

	// The history is usable afterwards: a normal edit is one step that undoes
	Model.MoveWindow(TagsTrack, 2);
	TestEqual(TEXT("an edit after a cancel is a normal step"), AppliedSteps(), StepsBefore + 1);
	TestTrue(TEXT("undo"), GEditor->UndoTransaction());
	TestEqual(TEXT("undo restores"), Export(Fixture.Data()), Before);

	// A drag abandoned by destroying the model must not leave a transaction open
	{
		FFixture Abandoned;
		const FString AbandonedBefore = Export(Abandoned.Data());
		Abandoned.Model->BeginDrag(INVTEXT("Abandoned"));
		Abandoned.Model->MoveWindow(CancelTrack, 3);
		Abandoned.UndoClient.Reset();
		Abandoned.Model.Reset();
		TestEqual(TEXT("an abandoned drag restores the data"), Export(Abandoned.Data()), AbandonedBefore);
		TestFalse(TEXT("an abandoned drag leaves no open transaction"), GEditor->IsTransactionActive());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ModelSelectionTest, "GSTNCombat.Editor.Model.SelectionRevalidatedAfterUndo", GSTN_EDITOR_TEST_FLAGS)
bool FGSTN_ModelSelectionTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), HasEditorHistory()))
	{
		return false;
	}
	FFixture Fixture;
	FGSTN_TimelineEditorModel& Model = *Fixture.Model;

	int32 SelectionEvents = 0;
	int32 PlayheadEvents = 0;
	int32 KeyEvents = 0;
	Model.OnSelectionChanged.AddLambda([&SelectionEvents]() { ++SelectionEvents; });
	Model.OnPlayheadChanged.AddLambda([&PlayheadEvents](int32) { ++PlayheadEvents; });
	Model.OnKeySelected.AddLambda([&KeyEvents](const FGSTN_Selection&) { ++KeyEvents; });

	// A selected track that an undo removes
	const int32 StepsBefore = AppliedSteps();
	const int32 Added = Model.AddTrack(FGSTN_TimelineTrack_Tags::StaticStruct(), 1, 3);
	TestEqual(TEXT("a new track is selected"), Model.GetSelection().Num(), 1);
	TestTrue(TEXT("undo of the add"), GEditor->UndoTransaction());
	TestEqual(TEXT("the removed track is no longer selected"), Model.GetSelection().Num(), 0);
	TestEqual(TEXT("the track count is back"), Model.GetTrackCount(), Added);

	// A selected key that an undo removes
	const int32 NewKey = Model.AddKey(KeyedTrack, 25);
	TestEqual(TEXT("the new key is last"), NewKey, 3);
	TestTrue(TEXT("a key selection was announced"), KeyEvents > 0);
	TestTrue(TEXT("the new key is selected"), Model.IsSelected(FGSTN_Selection::Key(KeyedTrack, NewKey)));
	TestTrue(TEXT("undo of the add key"), GEditor->UndoTransaction());
	TestEqual(TEXT("the removed key is no longer selected"), Model.GetSelection().Num(), 0);

	// A selection that is still valid survives an unrelated undo
	Model.SelectKey(KeyedTrack, 1);
	Model.SetSelection({ FGSTN_Selection::Key(KeyedTrack, 1), FGSTN_Selection::Window(TagsTrack), FGSTN_Selection::Window(99), FGSTN_Selection::Key(KeyedTrack, 40) });
	TestEqual(TEXT("entries that do not exist are not selected"), Model.GetSelection().Num(), 2);
	Model.MoveWindow(CancelTrack, 1);
	TestTrue(TEXT("undo of an unrelated move"), GEditor->UndoTransaction());
	TestEqual(TEXT("the valid selection survives"), Model.GetSelection().Num(), 2);

	// The playhead follows the length
	Model.SetPlayhead(25);
	TestEqual(TEXT("playhead"), Model.GetPlayhead(), 25);
	Model.SetTotalFrames(10);
	TestEqual(TEXT("shortening the timeline clamps the playhead"), Model.GetPlayhead(), 9);
	TestTrue(TEXT("undo of the shortening"), GEditor->UndoTransaction());
	Model.SetPlayhead(25);
	TestTrue(TEXT("redo of the shortening"), GEditor->RedoTransaction());
	TestEqual(TEXT("redo clamps the playhead again"), Model.GetPlayhead(), 9);
	TestTrue(TEXT("the playhead moved were announced"), PlayheadEvents >= 3);
	TestTrue(TEXT("the selection changes were announced"), SelectionEvents >= 4);

	// The selection follows its keys through a re-sort
	GEditor->UndoTransaction();
	Model.SelectKey(KeyedTrack, 0);
	Model.MoveKeys({ FGSTN_Selection::Key(KeyedTrack, 0) }, 12);
	TestTrue(TEXT("the moved key is selected at its new place"), Model.IsSelected(FGSTN_Selection::Key(KeyedTrack, 1)));
	TestEqual(TEXT("it is the same key"), Fixture.Keyed().Keys[1].Frame, 14);

	// Removing a track renumbers the selection of the tracks after it
	Model.SetSelection({ FGSTN_Selection::Window(KeyedTrack), FGSTN_Selection::Window(CancelTrack) });
	Model.RemoveTracks({ TagsTrack });
	TestTrue(TEXT("the keyed track moved up and stays selected"), Model.IsSelected(FGSTN_Selection::Window(KeyedTrack - 1)));
	TestTrue(TEXT("the cancel track moved up and stays selected"), Model.IsSelected(FGSTN_Selection::Window(CancelTrack - 1)));

	// Unwind this test's steps
	while (AppliedSteps() > StepsBefore && GEditor->UndoTransaction())
	{
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ModelClipboardTest, "GSTNCombat.Editor.Model.ClipboardRoundTrip", GSTN_EDITOR_TEST_FLAGS)
bool FGSTN_ModelClipboardTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), HasEditorHistory()))
	{
		return false;
	}
	FFixture Source;
	const FString Text = Source.Model->CopyKeysToText({ FGSTN_Selection::Key(KeyedTrack, 2), FGSTN_Selection::Key(KeyedTrack, 0) });
	TestTrue(TEXT("the text names the format"), Text.StartsWith(TEXT("GSTNKeys1\t")));
	TestTrue(TEXT("the text names the track struct"), Text.Contains(TEXT("GSTN_TestKeyedTrack")));
	TestTrue(TEXT("the text names the key struct"), Text.Contains(TEXT("GSTN_TestKey")));
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	TestEqual(TEXT("a header and one line per key"), Lines.Num(), 3);
	TestTrue(TEXT("the first key is at relative frame 0, whatever the order they were selected in"), Lines.IsValidIndex(1) && Lines[1].StartsWith(TEXT("0\t")));
	TestTrue(TEXT("the second key is 16 frames later"), Lines.IsValidIndex(2) && Lines[2].StartsWith(TEXT("16\t")));

	// Back into the same timeline, the earliest at frame 5
	const FString SourceBefore = Export(Source.Data());
	TestEqual(TEXT("two keys pasted"), Source.Model->PasteKeysFromText(KeyedTrack, 5, Text), 2);
	TestEqual(TEXT("five keys now"), Source.Keyed().Keys.Num(), 5);
	TestEqual(TEXT("sorted: first pasted key"), Source.Keyed().Keys[1].Frame, 5);
	TestTrue(TEXT("first pasted key has the copied values"), Source.Keyed().Keys[1].Location.Equals(FVector(0.0, 0.0, 0.0)) && FMath::IsNearlyEqual(Source.Keyed().Keys[1].Radius, 10.f));
	TestEqual(TEXT("sorted: second pasted key"), Source.Keyed().Keys[4].Frame, 21);
	TestTrue(TEXT("second pasted key has the copied values"), Source.Keyed().Keys[4].Location.Equals(FVector(200.0, 0.0, 0.0)) && FMath::IsNearlyEqual(Source.Keyed().Keys[4].Radius, 30.f));
	TestEqual(TEXT("the pasted keys are selected"), Source.Model->GetSelection().Num(), 2);
	GEditor->UndoTransaction();
	TestEqual(TEXT("undo of the paste"), Export(Source.Data()), SourceBefore);

	// Into another timeline (a different asset, a different session): the text is all that travels
	{
		FFixture Other;
		Other.Keyed().Keys.Reset();
		TestEqual(TEXT("pasted into another timeline"), Other.Model->PasteKeysFromText(KeyedTrack, 3, Text), 2);
		TestEqual(TEXT("keys arrived"), Other.Keyed().Keys.Num(), 2);
		TestEqual(TEXT("at the frame asked"), Other.Keyed().Keys[0].Frame, 3);
		TestEqual(TEXT("keeping the gap"), Other.Keyed().Keys[1].Frame, 19);
		TestTrue(TEXT("keeping the values"), Other.Keyed().Keys[1].Location.Equals(FVector(200.0, 0.0, 0.0)));
		GEditor->UndoTransaction();
	}

	// Clamping, and refusal of anything that is not exactly ours
	{
		FFixture Other;
		const FString OtherBefore = Export(Other.Data());
		TestEqual(TEXT("keys past the end are clamped, not dropped"), Other.Model->PasteKeysFromText(KeyedTrack, 25, Text), 2);
		TestEqual(TEXT("the late key lands on the last frame"), Other.Keyed().Keys.Last().Frame, 29);
		GEditor->UndoTransaction();
		TestEqual(TEXT("undo"), Export(Other.Data()), OtherBefore);

		const int32 Steps = AppliedSteps();
		TestEqual(TEXT("pasting onto a track without keys is refused"), Other.Model->PasteKeysFromText(TagsTrack, 3, Text), 0);
		TestEqual(TEXT("garbage is refused"), Other.Model->PasteKeysFromText(KeyedTrack, 3, TEXT("hello\nworld")), 0);
		TestEqual(TEXT("an empty clipboard is refused"), Other.Model->PasteKeysFromText(KeyedTrack, 3, FString()), 0);
		TestEqual(TEXT("another format version is refused"), Other.Model->PasteKeysFromText(KeyedTrack, 3, Text.Replace(TEXT("GSTNKeys1"), TEXT("GSTNKeys2"))), 0);
		TestEqual(TEXT("another track type is refused"), Other.Model->PasteKeysFromText(KeyedTrack, 3, Text.Replace(TEXT("GSTN_TestKeyedTrack"), TEXT("GSTN_Other"))), 0);
		TestEqual(TEXT("a damaged key line pastes nothing at all"), Other.Model->PasteKeysFromText(KeyedTrack, 3, Text + TEXT("\nnot-a-frame\t(Frame=1)")), 0);
		TestEqual(TEXT("windows line ends are accepted"), Other.Model->PasteKeysFromText(KeyedTrack, 3, Text.Replace(TEXT("\n"), TEXT("\r\n"))), 2);
		GEditor->UndoTransaction();
		TestEqual(TEXT("refusals leave no step"), AppliedSteps(), Steps);
		TestEqual(TEXT("refusals leave the data alone"), Export(Other.Data()), OtherBefore);
	}

	// Nothing to copy: no keys selected, or a track without keys
	TestTrue(TEXT("no selection copies nothing"), Source.Model->CopyKeysToText({}).IsEmpty());
	TestTrue(TEXT("a window is not a key"), Source.Model->CopyKeysToText({ FGSTN_Selection::Window(KeyedTrack) }).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ModelFitTotalFramesTest, "GSTNCombat.Editor.Model.FitTotalFramesDoesNotRunSilently", GSTN_EDITOR_TEST_FLAGS)
bool FGSTN_ModelFitTotalFramesTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), HasEditorHistory()))
	{
		return false;
	}
	FFixture Fixture;
	FGSTN_TimelineEditorModel& Model = *Fixture.Model;
	const int32 StartSteps = AppliedSteps();

	TStrongObjectPtr<UAnimMontage> Montage(NewObject<UAnimMontage>(GetTransientPackage()));
	Montage->SetCompositeLength(1.0f);
	if (!TestTrue(TEXT("the test montage has a length"), Montage->GetPlayLength() > 0.f))
	{
		return false;
	}

	// A montage that does not match the length changes nothing by itself: not when it is set, not by any other edit
	Model.ApplyDataEdit([&Montage](FGSTN_CombatTimelineData& D) { D.Montage = Montage.Get(); }, INVTEXT("Set montage"));
	TestEqual(TEXT("setting the montage leaves the total alone"), Fixture.Data().TotalFrames, 30);
	Model.AddTrack(FGSTN_TimelineTrack_Tags::StaticStruct(), 1, 2);
	Model.MoveWindow(TagsTrack, 1);
	Model.SetFirstActionableFrame(10);
	Model.SetPlayhead(3);
	Model.Revalidate();
	TestEqual(TEXT("no other edit adjusts the total"), Fixture.Data().TotalFrames, 30);

	// The length the command will use is the one the validator compares against
	const int32 Expected = FGSTN_TimelineEditorModel::ComputeMontageFrames(Fixture.Data(), Montage.Get(), 62.5f);
	TestEqual(TEXT("one second at 62.5 Hz is 62 or 63 frames"), Expected, FMath::RoundToInt(Montage->GetPlayLength() * 62.5f));
	TestNotEqual(TEXT("the montage does not match the total"), Expected, Fixture.Data().TotalFrames);

	// The command, once: undoable, and idempotent
	const int32 StepsBefore = AppliedSteps();
	const FString Before = Export(Fixture.Data());
	TestTrue(TEXT("the command fits the total"), Model.FitTotalFramesToMontage());
	TestEqual(TEXT("total equals the montage length in frames"), Fixture.Data().TotalFrames, Expected);
	TestEqual(TEXT("one undo step"), AppliedSteps(), StepsBefore + 1);
	TestFalse(TEXT("fitting again changes nothing"), Model.FitTotalFramesToMontage());
	TestEqual(TEXT("and adds no step"), AppliedSteps(), StepsBefore + 1);
	TestTrue(TEXT("undo"), GEditor->UndoTransaction());
	TestEqual(TEXT("undo restores the total"), Export(Fixture.Data()), Before);

	// The play rate scales it
	Model.ApplyDataEdit([](FGSTN_CombatTimelineData& D) { D.MontagePlayRate = 2.f; }, INVTEXT("Double speed"));
	TestEqual(TEXT("double speed halves the length"), FGSTN_TimelineEditorModel::ComputeMontageFrames(Fixture.Data(), Montage.Get(), 62.5f), FMath::RoundToInt(Montage->GetPlayLength() / 2.f * 62.5f));
	TestEqual(TEXT("no montage, no length"), FGSTN_TimelineEditorModel::ComputeMontageFrames(Fixture.Data(), nullptr, 62.5f), static_cast<int32>(INDEX_NONE));

	// Without a montage the command does nothing
	{
		FFixture Bare;
		TestFalse(TEXT("no montage: the fit does nothing"), Bare.Model->FitTotalFramesToMontage());
		TestEqual(TEXT("total unchanged"), Bare.Data().TotalFrames, 30);
	}

	while (AppliedSteps() > StartSteps && GEditor->UndoTransaction())
	{
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ModelContextRegistryTest, "GSTNCombat.Editor.Model.TwoToolkitsOneTargetRefused", GSTN_EDITOR_TEST_FLAGS)
bool FGSTN_ModelContextRegistryTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_CombatTimeline> A(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
	TStrongObjectPtr<UGSTN_CombatTimeline> B(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
	const FGSTN_TimelineEditContext ContextA = FGSTN_TimelineEditContext::ForAsset(A.Get());
	const FGSTN_TimelineEditContext ContextB = FGSTN_TimelineEditContext::ForAsset(B.Get());

	int32 FocusRequests = 0;
	TSharedPtr<FGSTN_TimelineContextLease> First = FGSTN_TimelineEditContextRegistry::Acquire(ContextA, [&FocusRequests]() { ++FocusRequests; });
	TestTrue(TEXT("the first editor gets the target"), First.IsValid());
	TestTrue(TEXT("the target is open"), FGSTN_TimelineEditContextRegistry::IsOpen(ContextA));
	TestFalse(TEXT("another target is not"), FGSTN_TimelineEditContextRegistry::IsOpen(ContextB));

	TestFalse(TEXT("the second editor on the same target is refused"), FGSTN_TimelineEditContextRegistry::Acquire(ContextA).IsValid());
	TestEqual(TEXT("the open editor is asked to focus"), FocusRequests, 1);
	TestFalse(TEXT("a second context object for the same asset is refused too"), FGSTN_TimelineEditContextRegistry::Acquire(FGSTN_TimelineEditContext::ForAsset(A.Get())).IsValid());
	TestEqual(TEXT("and focuses again"), FocusRequests, 2);

	TSharedPtr<FGSTN_TimelineContextLease> Second = FGSTN_TimelineEditContextRegistry::Acquire(ContextB);
	TestTrue(TEXT("a different target opens freely"), Second.IsValid());

	First.Reset();
	TestFalse(TEXT("closing the editor frees the target"), FGSTN_TimelineEditContextRegistry::IsOpen(ContextA));
	TSharedPtr<FGSTN_TimelineContextLease> Again = FGSTN_TimelineEditContextRegistry::Acquire(ContextA);
	TestTrue(TEXT("and it can be opened again"), Again.IsValid());

	TestFalse(TEXT("an invalid context cannot be opened"), FGSTN_TimelineEditContextRegistry::Acquire(FGSTN_TimelineEditContext()).IsValid());
	TestFalse(TEXT("an inline context is not resolvable before E13"), FGSTN_TimelineEditContext::ForBlueprint(nullptr).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ModelViewTransformTest, "GSTNCombat.Editor.Model.ViewTransformRoundTrip", GSTN_EDITOR_TEST_FLAGS)
bool FGSTN_ModelViewTransformTest::RunTest(const FString& Parameters)
{
	FGSTN_TimelineViewTransform View;
	for (const double PixelsPerFrame : { 1.0, 4.5, 12.0, 64.0, 256.0 })
	{
		View.PixelsPerFrame = PixelsPerFrame;
		View.FirstVisibleFrame = 7.25;
		for (const double Frame : { 0.0, 3.0, 29.0, 61.5 })
		{
			TestTrue(TEXT("frame to pixel and back"), FMath::IsNearlyEqual(View.PixelToFrame(View.FrameToPixel(Frame)), Frame, 1e-9));
		}
		TestEqual(TEXT("the pixel at the start of a frame is that frame"), View.PixelToFrameIndex(View.FrameToPixel(5.0)), 5);
		TestEqual(TEXT("the pixel just before the next frame is still this one"), View.PixelToFrameIndex(View.FrameToPixel(6.0) - 0.01), 5);
	}

	// Zooming keeps the frame under the cursor where it is
	View.PixelsPerFrame = 12.0;
	View.FirstVisibleFrame = 3.0;
	const double Cursor = 240.0;
	const double Under = View.PixelToFrame(Cursor);
	View.ZoomAround(Cursor, 2.0);
	TestTrue(TEXT("zoom keeps the frame under the cursor"), FMath::IsNearlyEqual(View.PixelToFrame(Cursor), Under, 1e-9));
	TestTrue(TEXT("zoom changed the scale"), FMath::IsNearlyEqual(View.PixelsPerFrame, 24.0));
	View.ZoomAround(Cursor, 1000.0);
	TestTrue(TEXT("zoom is clamped"), View.PixelsPerFrame <= FGSTN_TimelineViewTransform::MaxPixelsPerFrame);
	View.ZoomAround(Cursor, 1e-9);
	TestTrue(TEXT("zoom out is clamped"), View.PixelsPerFrame >= FGSTN_TimelineViewTransform::MinPixelsPerFrame);

	View.FitRange(0.0, 30.0, 600.0);
	TestTrue(TEXT("fit shows the whole range"), FMath::IsNearlyEqual(View.FrameToPixel(30.0), 600.0, 1e-6) && FMath::IsNearlyEqual(View.FrameToPixel(0.0), 0.0, 1e-6));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
