#include "AssetDefinitionRegistry.h"
#include "Assets/GSTN_AssetDefinition_CombatTimeline.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Settings/GSTN_CombatEditorSettings.h"
#include "Tests/GSTN_EditorTestTypes.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_KeyedTrackAccess.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineEditorToolkit.h"
#include "Timeline/GSTN_TimelineGesture.h"
#include "Timeline/GSTN_TimelineSelectionProxy.h"
#include "Timeline/GSTN_TimelineTrackEditor.h"
#include "Timeline/GSTN_TimelineUndo.h"
#include "Timeline/GSTN_TrackEditors.h"
#include "Timeline/Widgets/SGSTN_MontageLengthBar.h"
#include "Timeline/Widgets/SGSTN_TimelineDetails.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// The tests of the track editor registry, the built-in track editors, the details proxy and the toolkit's logic (plan m5 E5). They run
// headless: opening the toolkit needs a window, so what the toolkit decides (one editor per asset, the layout, the asset definition)
// is tested where it lives, and the window itself is the manual smoke test listed in the commit message.

#define GSTN_REGISTRY_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNEditorRegistryTest
{
	/** A track editor for the test struct: a name, a default length and a default the model has to apply inside the add transaction */
	class FTestDashTrackEditor final : public IGSTN_TrackEditor
	{
	public:
		virtual const UScriptStruct* GetTrackStruct() const override { return FGSTN_TestDerivedMovement::StaticStruct(); }
		virtual FText GetDisplayName() const override { return FText::FromString(TEXT("Test Dash")); }
		virtual int32 GetDefaultLength() const override { return 9; }
		virtual FName GetIconBrushName() const override { return TEXT("Icons.Event"); }

		virtual void InitNewTrack(FGSTN_TimelineTrack& Track, int32 AddedAtFrame, const FGSTN_CombatTimelineData& Data) const override
		{
			FGSTN_TestDerivedMovement& Dash = static_cast<FGSTN_TestDerivedMovement&>(Track);
			Dash.GravityScale = 0.25f;
			Dash.ExtraFrames = AddedAtFrame;
		}

		virtual FText GetRowLabel(const FGSTN_TimelineTrack& Track) const override
		{
			return FText::FromString(FString::Printf(TEXT("Dash +%d"), static_cast<const FGSTN_TestDerivedMovement&>(Track).ExtraFrames));
		}
	};

	/** Registers a track editor for the length of a test, whatever the test does */
	struct FScopedTrackEditor
	{
		explicit FScopedTrackEditor(TSharedRef<const IGSTN_TrackEditor> Editor)
			: Struct(Editor->GetTrackStruct())
		{
			FGSTN_TrackEditorRegistry::Get().Register(Editor);
		}
		~FScopedTrackEditor() { FGSTN_TrackEditorRegistry::Get().Unregister(Struct); }
		const UScriptStruct* Struct;
	};

	/** A transient timeline, a model on it and an undo client: what the toolkit would own */
	struct FFixture
	{
		TStrongObjectPtr<UGSTN_CombatTimeline> Timeline;
		TSharedPtr<FGSTN_TimelineEditorModel> Model;
		TUniquePtr<FGSTN_TimelineUndoClient> UndoClient;

		FFixture()
		{
			Timeline.Reset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
			Timeline->Data.TotalFrames = 40;
			Timeline->Data.FirstActionableFrame = 20;
			FGSTN_TimelineTrack_Movement Move;
			Move.StartFrame = 4;
			Move.EndFrame = 10;
			Timeline->Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Movement>(Move));
			FGSTN_TimelineTrack_Tags TagsTrack;
			TagsTrack.StartFrame = 0;
			TagsTrack.EndFrame = 12;
			Timeline->Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Tags>(TagsTrack));
			Model = MakeShared<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
			Model->SetTickRate(62.5f);
			UndoClient = MakeUnique<FGSTN_TimelineUndoClient>(*Model);
		}

		~FFixture()
		{
			UndoClient.Reset();
			Model.Reset();
		}

		FGSTN_CombatTimelineData& Data() { return Timeline->Data; }
		FGSTN_TimelineTrack_Movement& Movement() { return Timeline->Data.Tracks[0].GetMutable<FGSTN_TimelineTrack_Movement>(); }
	};

	int32 AppliedSteps()
	{
		return (GEditor && GEditor->Trans) ? GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() : 0;
	}

	bool HasEditorContext()
	{
		return GEditor != nullptr && GEditor->Trans != nullptr;
	}
}

using namespace GSTNEditorRegistryTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RegistryListedTest, "GSTNCombat.Editor.Registry.RegisteredTrackListed", GSTN_REGISTRY_TEST_FLAGS)
bool FGSTN_RegistryListedTest::RunTest(const FString& Parameters)
{
	FGSTN_TrackEditorRegistry& Registry = FGSTN_TrackEditorRegistry::Get();
	const int32 Before = Registry.Num();
	int32 Changes = 0;
	const FDelegateHandle Handle = Registry.OnChanged.AddLambda([&Changes]() { ++Changes; });

	{
		FScopedTrackEditor Scoped(MakeShared<FTestDashTrackEditor>());
		TestEqual(TEXT("one more editor"), Registry.Num(), Before + 1);
		TestEqual(TEXT("the registry announced it"), Changes, 1);
		TestTrue(TEXT("the struct is registered"), Registry.IsRegistered(FGSTN_TestDerivedMovement::StaticStruct()));

		TArray<TSharedRef<const IGSTN_TrackEditor>> All;
		Registry.GetAll(All);
		TestEqual(TEXT("Add Track lists it with the others"), All.Num(), Before + 1);
		const bool bListed = All.ContainsByPredicate([](const TSharedRef<const IGSTN_TrackEditor>& Editor) { return Editor->GetTrackStruct() == FGSTN_TestDerivedMovement::StaticStruct(); });
		TestTrue(TEXT("and it is in the list"), bListed);
		bool bSorted = true;
		for (int32 Index = 1; Index < All.Num(); ++Index)
		{
			bSorted &= All[Index - 1]->GetDisplayName().CompareTo(All[Index]->GetDisplayName()) <= 0;
		}
		TestTrue(TEXT("by display name"), bSorted);

		// A second editor for the same struct replaces the first, it does not add a row
		Registry.Register(MakeShared<FTestDashTrackEditor>());
		TestEqual(TEXT("registering twice keeps one"), Registry.Num(), Before + 1);

		// Only tracks: the base itself and a struct that is not a track are refused
		struct FNotATrackEditor final : public IGSTN_TrackEditor
		{
			virtual const UScriptStruct* GetTrackStruct() const override { return FGSTN_TimelineTrack::StaticStruct(); }
			virtual FText GetDisplayName() const override { return FText::GetEmpty(); }
		};
		Registry.Register(MakeShared<FNotATrackEditor>());
		TestEqual(TEXT("the base track struct is not a track kind"), Registry.Num(), Before + 1);
	}

	TestEqual(TEXT("unregistering removes it"), Registry.Num(), Before);
	const TSharedPtr<const IGSTN_TrackEditor> AfterwardsEditor = Registry.Find(FGSTN_TestDerivedMovement::StaticStruct());
	TestTrue(TEXT("the derived struct falls back to the movement editor"), AfterwardsEditor.IsValid() && AfterwardsEditor->GetTrackStruct() == FGSTN_TimelineTrack_Movement::StaticStruct());
	TestFalse(TEXT("removing what is not there says so"), Registry.Unregister(FGSTN_TestDerivedMovement::StaticStruct()));
	Registry.OnChanged.Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RegistryGenericRowTest, "GSTNCombat.Editor.Registry.UnknownTrackGetsGenericRow", GSTN_REGISTRY_TEST_FLAGS)
bool FGSTN_RegistryGenericRowTest::RunTest(const FString& Parameters)
{
	const FGSTN_TrackEditorRegistry& Registry = FGSTN_TrackEditorRegistry::Get();
	const UScriptStruct* Unknown = FGSTN_TestKeyedTrack::StaticStruct();

	TestFalse(TEXT("nobody registered the test track"), Registry.Find(Unknown).IsValid());
	const TSharedRef<const IGSTN_TrackEditor> Generic = Registry.FindOrGeneric(Unknown);
	TestTrue(TEXT("the generic row is for that struct"), Generic->GetTrackStruct() == Unknown);
	TestEqual(TEXT("named by the struct"), Generic->GetDisplayName().ToString(), Unknown->GetDisplayNameText().ToString());

	FGSTN_TestKeyedTrack Track;
	TestEqual(TEXT("its bar says the same"), Generic->GetRowLabel(Track).ToString(), Generic->GetDisplayName().ToString());
	TestEqual(TEXT("in the generic colour of the settings"), Generic->GetColor(), UGSTN_CombatEditorSettings::Get().GetTrackColor(Unknown->GetFName()));
	TestEqual(TEXT("a window of the fallback length"), Generic->GetDefaultLength(), IGSTN_TrackEditor::FallbackDefaultLength);
	TestEqual(TEXT("a neutral default icon"), Generic->GetIconBrushName(), FName(TEXT("Icons.Box")));
	TestTrue(TEXT("no gizmo"), Generic->AsGizmoProvider() == nullptr);
	TestFalse(TEXT("no keys until a keyed access is registered"), Generic->HasKeys());
	TestTrue(TEXT("one instance per struct, not one per repaint"), &Registry.FindOrGeneric(Unknown).Get() == &Generic.Get());

	// A keyed access registered later makes the same row keyed: the keyed registry is the single source of truth
	FGSTN_KeyedTrackAccessRegistry::Register(Unknown, MakeShared<FGSTN_TestKeyedAccess>());
	TestTrue(TEXT("keys follow the keyed access registry"), Generic->HasKeys());
	FGSTN_KeyedTrackAccessRegistry::Unregister(Unknown);

	// A track the game derived from a built-in keeps the built-in's row until it registers its own
	const UScriptStruct* Derived = FGSTN_TestDerivedMovement::StaticStruct();
	const TSharedPtr<const IGSTN_TrackEditor> ForDerived = Registry.Find(Derived);
	TestTrue(TEXT("the derived track finds its base's editor"), ForDerived.IsValid() && ForDerived->GetTrackStruct() == FGSTN_TimelineTrack_Movement::StaticStruct());

	// Add Track offers the tracks that have no editor, as generic rows, and not the ones that have
	TArray<const UScriptStruct*> Others;
	Registry.GetUnregisteredTrackStructs(Others);
	TestFalse(TEXT("the hitbox track has its editor since E5b"), Others.Contains(FGSTN_TimelineTrack_Hitbox::StaticStruct()));
	TestFalse(TEXT("so has the hurtbox modifier"), Others.Contains(FGSTN_TimelineTrack_HurtboxModifier::StaticStruct()));
	TestFalse(TEXT("a track with an editor is not offered twice"), Others.Contains(FGSTN_TimelineTrack_Movement::StaticStruct()));
	TestFalse(TEXT("hidden test tracks are not offered"), Others.Contains(Unknown));
	TestFalse(TEXT("the base struct is not a track kind"), Others.Contains(FGSTN_TimelineTrack::StaticStruct()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RegistryBuiltInsTest, "GSTNCombat.Editor.Registry.BuiltInsAllRegistered", GSTN_REGISTRY_TEST_FLAGS)
bool FGSTN_RegistryBuiltInsTest::RunTest(const FString& Parameters)
{
	const FGSTN_TrackEditorRegistry& Registry = FGSTN_TrackEditorRegistry::Get();
	const UScriptStruct* Kinds[] = {
		FGSTN_TimelineTrack_CancelWindow::StaticStruct(), FGSTN_TimelineTrack_Movement::StaticStruct(), FGSTN_TimelineTrack_Tags::StaticStruct(),
		FGSTN_TimelineTrack_Event::StaticStruct(), FGSTN_TimelineTrack_WeaponSwitch::StaticStruct() };
	for (const UScriptStruct* Kind : Kinds)
	{
		const TSharedPtr<const IGSTN_TrackEditor> Editor = Registry.Find(Kind);
		if (!TestTrue(FString::Printf(TEXT("%s has a track editor"), *Kind->GetName()), Editor.IsValid()))
		{
			continue;
		}
		TestTrue(TEXT("for its own struct"), Editor->GetTrackStruct() == Kind);
		TestFalse(TEXT("with a name"), Editor->GetDisplayName().IsEmpty());
		TestFalse(TEXT("an icon"), Editor->GetIconBrushName().IsNone());
		TestEqual(TEXT("and the colour of the settings"), Editor->GetColor(), UGSTN_CombatEditorSettings::Get().GetTrackColor(Kind->GetFName()));
		TestFalse(TEXT("no keys: the keyed tracks arrive with the hitbox"), Editor->HasKeys());
	}

	TestEqual(TEXT("a point track takes one frame (event)"), Registry.FindOrGeneric(FGSTN_TimelineTrack_Event::StaticStruct())->GetDefaultLength(), 1);
	TestEqual(TEXT("(weapon switch)"), Registry.FindOrGeneric(FGSTN_TimelineTrack_WeaponSwitch::StaticStruct())->GetDefaultLength(), 1);
	TestEqual(TEXT("a window track takes the fallback"), Registry.FindOrGeneric(FGSTN_TimelineTrack_Movement::StaticStruct())->GetDefaultLength(), IGSTN_TrackEditor::FallbackDefaultLength);

	// What the bars say
	const FGameplayTag Head = FGameplayTag::RequestGameplayTag(TEXT("Combat.Hurtbox.Head"), false);
	const FGameplayTag Torso = FGameplayTag::RequestGameplayTag(TEXT("Combat.Hurtbox.Torso"), false);
	if (TestTrue(TEXT("the native hurtbox tags exist to build labels with"), Head.IsValid() && Torso.IsValid()))
	{
		FGSTN_TimelineTrack_CancelWindow Cancel;
		TestEqual(TEXT("a cancel window with no target says so"), Registry.FindOrGeneric(FGSTN_TimelineTrack_CancelWindow::StaticStruct())->GetRowLabel(Cancel).ToString(), FString(TEXT("(no target)")));
		FGameplayTagContainer Targets;
		Targets.AddTag(Head);
		Targets.AddTag(Torso);
		Cancel.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(Targets);
		const FString Plain = Registry.FindOrGeneric(FGSTN_TimelineTrack_CancelWindow::StaticStruct())->GetRowLabel(Cancel).ToString();
		TestTrue(TEXT("the label names the leaves of the query"), Plain.Contains(TEXT("Head")) && Plain.Contains(TEXT("Torso")));
		Cancel.Condition = EGSTN_CancelCondition::OnHit;
		TestTrue(TEXT("and the condition when it is not 'always'"), Registry.FindOrGeneric(FGSTN_TimelineTrack_CancelWindow::StaticStruct())->GetRowLabel(Cancel).ToString().Contains(TEXT("on hit")));

		FGSTN_TimelineTrack_Tags TagsTrack;
		TagsTrack.Tags.AddTag(Head);
		TestEqual(TEXT("a tags track names its tag by its last segment"), Registry.FindOrGeneric(FGSTN_TimelineTrack_Tags::StaticStruct())->GetRowLabel(TagsTrack).ToString(), FString(TEXT("Head")));

		TArray<FGameplayTag> Many = { Head, Torso, Head, Torso, Head };
		const FString Joined = GSTNTrackEditors::JoinTagLeaves(Many).ToString();
		TestTrue(TEXT("a long list is cut with a count"), Joined.EndsWith(TEXT("+2")));
	}

	FGSTN_TimelineTrack_WeaponSwitch Switch;
	TestEqual(TEXT("next weapon"), Registry.FindOrGeneric(FGSTN_TimelineTrack_WeaponSwitch::StaticStruct())->GetRowLabel(Switch).ToString(), FString(TEXT("Next weapon")));
	Switch.Target = EGSTN_SwitchTarget::Index;
	Switch.Index = 2;
	TestEqual(TEXT("weapon by index"), Registry.FindOrGeneric(FGSTN_TimelineTrack_WeaponSwitch::StaticStruct())->GetRowLabel(Switch).ToString(), FString(TEXT("Weapon 2")));

	FGSTN_TimelineTrack_Movement Move;
	TestTrue(TEXT("a movement without a curve says so"), Registry.FindOrGeneric(FGSTN_TimelineTrack_Movement::StaticStruct())->GetRowLabel(Move).ToString().Contains(TEXT("no curve")));

	TestTrue(TEXT("the hitbox track has an editor (E5b)"), Registry.IsRegistered(FGSTN_TimelineTrack_Hitbox::StaticStruct()));
	TestTrue(TEXT("and the hurtbox modifier"), Registry.IsRegistered(FGSTN_TimelineTrack_HurtboxModifier::StaticStruct()));

	// The test tab and its console command are gone
	TestNull(TEXT("no panel test command"), IConsoleManager::Get().FindConsoleObject(TEXT("gstn.Combat.OpenTimelinePanelTest")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RegistryDetailsProxyTest, "GSTNCombat.Editor.Registry.DetailsProxyAppliesEditThroughModel", GSTN_REGISTRY_TEST_FLAGS)
bool FGSTN_RegistryDetailsProxyTest::RunTest(const FString& Parameters)
{
	if (!HasEditorContext())
	{
		AddInfo(TEXT("no transaction buffer in this editor, skipped"));
		return true;
	}
	FFixture F;
	TStrongObjectPtr<UGSTN_TimelineSelectionProxy> Proxy(NewObject<UGSTN_TimelineSelectionProxy>(GetTransientPackage()));

	TestFalse(TEXT("an unbound proxy edits nothing"), Proxy->IsBound());
	TestFalse(TEXT("and commits nothing"), Proxy->Commit());
	Proxy->Bind(F.Model, 99);
	TestFalse(TEXT("a track that does not exist does not bind"), Proxy->IsBound());

	Proxy->Bind(F.Model, 0);
	if (!TestTrue(TEXT("bound to the movement track"), Proxy->IsBound() && Proxy->GetTrackStruct() == FGSTN_TimelineTrack_Movement::StaticStruct()))
	{
		return false;
	}
	TestFalse(TEXT("right after binding the copy is the track"), Proxy->Pull());

	// The details view edits the memory of the struct it was given
	const int32 StepsBefore = AppliedSteps();
	TSharedPtr<FStructOnScope> Shown = Proxy->MakeStructData();
	FGSTN_TimelineTrack_Movement* Copy = reinterpret_cast<FGSTN_TimelineTrack_Movement*>(Shown->GetStructMemory());
	Copy->GravityScale = 0.25f;
	Copy->bLockFacing = false;
	TestEqual(TEXT("the data does not change until the edit is committed"), F.Movement().GravityScale, 1.f);

	TestTrue(TEXT("the commit changed the data"), Proxy->Commit());
	TestEqual(TEXT("through the model: the gravity"), F.Movement().GravityScale, 0.25f);
	TestFalse(TEXT("the facing"), F.Movement().bLockFacing);
	TestEqual(TEXT("in one undo step"), AppliedSteps(), StepsBefore + 1);
	TestEqual(TEXT("the window was not touched"), F.Movement().StartFrame, 4);
	TestFalse(TEXT("a second commit has nothing to write"), Proxy->Commit());
	TestEqual(TEXT("and leaves no step"), AppliedSteps(), StepsBefore + 1);

	GEditor->UndoTransaction();
	TestEqual(TEXT("undo restores the gravity"), F.Movement().GravityScale, 1.f);
	TestTrue(TEXT("and the facing"), F.Movement().bLockFacing);
	TestTrue(TEXT("the copy is stale after the undo and Pull refreshes it"), Proxy->Pull());
	Shown = Proxy->MakeStructData();
	TestEqual(TEXT("the refreshed copy shows the restored value"), reinterpret_cast<FGSTN_TimelineTrack_Movement*>(Shown->GetStructMemory())->GravityScale, 1.f);

	// A slider drag: interactive changes join one drag, the committed one closes it. One undo step for the lot
	const int32 DragBase = AppliedSteps();
	Copy = reinterpret_cast<FGSTN_TimelineTrack_Movement*>(Shown->GetStructMemory());
	Copy->GravityScale = 0.5f;
	TestTrue(TEXT("interactive change applied"), Proxy->Commit(EPropertyChangeType::Interactive));
	TestTrue(TEXT("a drag is open"), F.Model->IsDragging());
	Copy->GravityScale = 0.75f;
	TestTrue(TEXT("the next one joins it"), Proxy->Commit(EPropertyChangeType::Interactive));
	TestEqual(TEXT("the data follows the slider"), F.Movement().GravityScale, 0.75f);
	TestFalse(TEXT("the release has nothing new to write"), Proxy->Commit(EPropertyChangeType::ValueSet));
	TestFalse(TEXT("but closes the drag"), F.Model->IsDragging());
	TestEqual(TEXT("one undo step for the whole slider drag"), AppliedSteps(), DragBase + 1);
	GEditor->UndoTransaction();
	TestEqual(TEXT("undone in one go"), F.Movement().GravityScale, 1.f);

	// The proxy follows its index, like the selection: when a track is removed the one that moves into its place is what it shows, and
	// when the index no longer exists it lets go instead of writing somewhere else
	F.Model->RemoveTracks({ 0 });
	TestTrue(TEXT("the track at the index changed, Pull refreshes"), Proxy->Pull());
	TestTrue(TEXT("now it shows the tags track"), Proxy->IsBound() && Proxy->GetTrackStruct() == FGSTN_TimelineTrack_Tags::StaticStruct());
	F.Model->RemoveTracks({ 0 });
	TestTrue(TEXT("no track left at the index, the proxy lets go"), Proxy->Pull());
	TestFalse(TEXT("unbound"), Proxy->IsBound());
	return true;
}

// Review 2026-10-23 nit: the details panel wrote SwapFrame and left the window's StartFrame where it was
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RegistryDetailsSwapFrameTest, "GSTNCombat.Editor.Registry.DetailsSwapFrameKeepsStartFrameInSync", GSTN_REGISTRY_TEST_FLAGS)
bool FGSTN_RegistryDetailsSwapFrameTest::RunTest(const FString& Parameters)
{
	if (!HasEditorContext())
	{
		AddInfo(TEXT("no transaction buffer in this editor, skipped"));
		return true;
	}
	FFixture F;
	FGSTN_TimelineTrack_WeaponSwitch Switch;
	Switch.StartFrame = 6;
	Switch.EndFrame = 7;
	Switch.SwapFrame = 6;
	F.Data().Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_WeaponSwitch>(Switch));
	const int32 SwitchIndex = F.Data().Tracks.Num() - 1;

	TStrongObjectPtr<UGSTN_TimelineSelectionProxy> Proxy(NewObject<UGSTN_TimelineSelectionProxy>(GetTransientPackage()));
	Proxy->Bind(F.Model, SwitchIndex);
	if (!TestTrue(TEXT("bound to the weapon switch"), Proxy->IsBound() && Proxy->GetTrackStruct() == FGSTN_TimelineTrack_WeaponSwitch::StaticStruct()))
	{
		return false;
	}

	TSharedPtr<FStructOnScope> Shown = Proxy->MakeStructData();
	reinterpret_cast<FGSTN_TimelineTrack_WeaponSwitch*>(Shown->GetStructMemory())->SwapFrame = 15;
	TestTrue(TEXT("the commit changed the data"), Proxy->Commit());
	const FGSTN_TimelineTrack_WeaponSwitch& Written = F.Data().Tracks[SwitchIndex].Get<FGSTN_TimelineTrack_WeaponSwitch>();
	TestEqual(TEXT("the swap frame is what was typed"), Written.SwapFrame, 15);
	TestEqual(TEXT("the window followed it"), Written.StartFrame, 15);

	// Another field of the switch leaves the window alone
	Proxy->Pull();
	Shown = Proxy->MakeStructData();
	reinterpret_cast<FGSTN_TimelineTrack_WeaponSwitch*>(Shown->GetStructMemory())->EndFrame = 17;
	TestTrue(TEXT("a second edit commits"), Proxy->Commit());
	TestEqual(TEXT("the start frame did not move again"), F.Data().Tracks[SwitchIndex].Get<FGSTN_TimelineTrack_WeaponSwitch>().StartFrame, 15);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RegistryDetailsWidgetsTest, "GSTNCombat.Editor.Registry.DetailsFollowsSelection", GSTN_REGISTRY_TEST_FLAGS)
bool FGSTN_RegistryDetailsWidgetsTest::RunTest(const FString& Parameters)
{
	// Which track the panel shows: one row, whatever parts of it are selected
	TestEqual(TEXT("nothing selected shows nothing"), SGSTN_TimelineDetails::ResolveShownTrack({}), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("one window"), SGSTN_TimelineDetails::ResolveShownTrack({ FGSTN_Selection::Window(3) }), 3);
	TestEqual(TEXT("a window and a key of the same track"), SGSTN_TimelineDetails::ResolveShownTrack({ FGSTN_Selection::Window(3), FGSTN_Selection::Key(3, 1) }), 3);
	TestEqual(TEXT("two rows show nothing"), SGSTN_TimelineDetails::ResolveShownTrack({ FGSTN_Selection::Window(3), FGSTN_Selection::Window(4) }), static_cast<int32>(INDEX_NONE));

	if (!HasEditorContext() || !FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("no Slate application or transaction buffer in this run, the widgets are not constructed"));
		return true;
	}
	FFixture F;
	const TSharedRef<SGSTN_TimelineDetails> Details = SNew(SGSTN_TimelineDetails).Model(F.Model);
	TestFalse(TEXT("nothing selected: nothing shown"), Details->GetProxy()->IsBound());
	F.Model->SetSelection({ FGSTN_Selection::Window(1) });
	TestTrue(TEXT("selecting the tags track binds the proxy"), Details->GetProxy()->IsBound() && Details->GetProxy()->GetTrackStruct() == FGSTN_TimelineTrack_Tags::StaticStruct());
	F.Model->SetSelection({ FGSTN_Selection::Window(0), FGSTN_Selection::Window(1) });
	TestFalse(TEXT("two rows: nothing shown"), Details->GetProxy()->IsBound());

	const TSharedRef<SGSTN_MontageLengthBar> Bar = SNew(SGSTN_MontageLengthBar).Model(F.Model);
	TestFalse(TEXT("no montage, no warning"), Bar->IsMismatched());
	TestEqual(TEXT("and no length to compare"), Bar->GetMontageFrames(), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RegistryAddTrackTest, "GSTNCombat.Editor.Registry.AddTrackInitializesDefaults", GSTN_REGISTRY_TEST_FLAGS)
bool FGSTN_RegistryAddTrackTest::RunTest(const FString& Parameters)
{
	if (!HasEditorContext())
	{
		AddInfo(TEXT("no transaction buffer in this editor, skipped"));
		return true;
	}
	FFixture F;
	const int32 StepsBefore = AppliedSteps();

	// The weapon switch keeps its own copy of the frame: its track editor sets it in the add transaction
	const int32 SwitchIndex = F.Model->AddTrack(FGSTN_TimelineTrack_WeaponSwitch::StaticStruct(), 12, 1);
	const FGSTN_TimelineTrack_WeaponSwitch* Switch = F.Data().Tracks[SwitchIndex].GetPtr<FGSTN_TimelineTrack_WeaponSwitch>();
	if (!TestNotNull(TEXT("a weapon switch was added"), Switch))
	{
		return false;
	}
	TestEqual(TEXT("it swaps on the frame it was added at"), Switch->SwapFrame, 12);
	TestEqual(TEXT("it starts there"), Switch->StartFrame, 12);
	TestEqual(TEXT("in one undo step"), AppliedSteps(), StepsBefore + 1);

	// A track editor's defaults come with the track, in the same step, and its length is what a double-click uses
	{
		FScopedTrackEditor Scoped(MakeShared<FTestDashTrackEditor>());
		const int32 Steps = AppliedSteps();
		const int32 DashIndex = F.Model->AddTrack(FGSTN_TestDerivedMovement::StaticStruct(), 5, 9);
		const FGSTN_TestDerivedMovement* Dash = F.Data().Tracks[DashIndex].GetPtr<FGSTN_TestDerivedMovement>();
		if (!TestNotNull(TEXT("the registered track was added"), Dash))
		{
			return false;
		}
		TestEqual(TEXT("the editor's default was applied"), Dash->GravityScale, 0.25f);
		TestEqual(TEXT("with the frame it was added at"), Dash->ExtraFrames, 5);
		TestEqual(TEXT("a track editor's label is the bar's text"), FGSTN_TrackEditorRegistry::Get().FindOrGeneric(FGSTN_TestDerivedMovement::StaticStruct())->GetRowLabel(*Dash).ToString(), FString(TEXT("Dash +5")));
		TestEqual(TEXT("still one step"), AppliedSteps(), Steps + 1);

		TestTrue(TEXT("a double-click on the dash lane adds another"), FGSTN_TimelineActions::AddAtFrame(*F.Model, DashIndex, 20));
		const FGSTN_TimelineTrack* Added = F.Model->GetTrack(F.Model->GetTrackCount() - 1);
		TestEqual(TEXT("as long as its track editor says"), Added ? Added->EndFrame - Added->StartFrame : -1, 9);
		TestEqual(TEXT("and initialised the same way"), F.Data().Tracks.Last().GetPtr<FGSTN_TestDerivedMovement>() ? F.Data().Tracks.Last().Get<FGSTN_TestDerivedMovement>().ExtraFrames : -1, 20);

		GEditor->UndoTransaction();
		TestEqual(TEXT("undo takes the double-click's track away"), F.Model->GetTrackCount(), 4);
	}

	// Without an editor a track is added as the struct's own defaults
	const int32 HitboxIndex = F.Model->AddTrack(FGSTN_TestKeyedTrack::StaticStruct(), 7, 3);
	const FGSTN_TimelineTrack* Hitbox = F.Model->GetTrack(HitboxIndex);
	TestTrue(TEXT("a track nobody has an editor for is still added"), Hitbox != nullptr && Hitbox->StartFrame == 7 && Hitbox->EndFrame == 10);

	// And the double-click on a built-in lane uses the length of its track editor (the movement lane)
	const int32 Before = F.Model->GetTrackCount();
	TestTrue(TEXT("movement lane"), FGSTN_TimelineActions::AddAtFrame(*F.Model, 0, 30));
	TestEqual(TEXT("added"), F.Model->GetTrackCount(), Before + 1);
	const FGSTN_TimelineTrack* Moved = F.Model->GetTrack(Before);
	TestEqual(TEXT("with the default length of the movement editor"), Moved ? Moved->EndFrame - Moved->StartFrame : -1, FGSTN_TrackEditorRegistry::Get().FindOrGeneric(FGSTN_TimelineTrack_Movement::StaticStruct())->GetDefaultLength());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RegistryOneToolkitPerAssetTest, "GSTNCombat.Editor.Registry.TwoToolkitsOneAssetFocusesOpen", GSTN_REGISTRY_TEST_FLAGS)
bool FGSTN_RegistryOneToolkitPerAssetTest::RunTest(const FString& Parameters)
{
	// The toolkit's InitEditor claims its target through the context registry before it builds a window, and gives the callback below
	// to the claim. A headless run cannot build the window, so this tests the claim the toolkit makes, in the shape the toolkit makes it:
	// the second claim is refused, the first claimant is asked to come to the front, and closing the first frees the asset
	TStrongObjectPtr<UGSTN_CombatTimeline> Asset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
	const FGSTN_TimelineEditContext Context = FGSTN_TimelineEditContext::ForAsset(Asset.Get());

	int32 Focused = 0;
	TSharedPtr<FGSTN_TimelineContextLease> FirstToolkit = FGSTN_TimelineEditContextRegistry::Acquire(Context, [&Focused]() { ++Focused; });
	TestTrue(TEXT("the first toolkit opens"), FirstToolkit.IsValid());

	TestFalse(TEXT("a second toolkit on the same asset is refused"), FGSTN_TimelineEditContextRegistry::Acquire(Context).IsValid());
	TestEqual(TEXT("the open toolkit is brought to the front"), Focused, 1);
	TestTrue(TEXT("the asset has an editor"), FGSTN_TimelineEditContextRegistry::IsOpen(Context));

	FirstToolkit.Reset();
	TestFalse(TEXT("closing the toolkit frees the asset"), FGSTN_TimelineEditContextRegistry::IsOpen(Context));
	TestTrue(TEXT("and it opens again"), FGSTN_TimelineEditContextRegistry::Acquire(Context).IsValid());

	// A toolkit is never opened on a context that resolves to nothing
	TestFalse(TEXT("an invalid context is not opened"), FGSTN_TimelineEditContextRegistry::Acquire(FGSTN_TimelineEditContext()).IsValid());

	AddInfo(TEXT("headless: the toolkit window itself is not built (needs a rendering editor); see the manual smoke steps in the commit message"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RegistryToolkitShapeTest, "GSTNCombat.Editor.Registry.ToolkitLayoutAndAssetDefinition", GSTN_REGISTRY_TEST_FLAGS)
bool FGSTN_RegistryToolkitShapeTest::RunTest(const FString& Parameters)
{
	// The layout: the name carries the version, and the four tabs of combat-m5-editor 5.1 are in it
	const TSharedRef<FTabManager::FLayout> Layout = FGSTN_TimelineEditorToolkit::MakeDefaultLayout();
	TestEqual(TEXT("the layout is the versioned one"), Layout->GetLayoutName(), FName(TEXT("GSTN_TimelineEditor_Layout_v1")));
	const FString Text = Layout->ToString();
	for (const FName TabId : { FGSTN_TimelineEditorToolkit::ViewportTabId, FGSTN_TimelineEditorToolkit::TimelineTabId, FGSTN_TimelineEditorToolkit::DetailsTabId, FGSTN_TimelineEditorToolkit::FrameDataTabId })
	{
		TestTrue(*FString::Printf(TEXT("the layout holds %s"), *TabId.ToString()), Text.Contains(TabId.ToString()));
	}

	// The Content Browser opens timelines with the toolkit: the asset definition for the class is ours
	const UAssetDefinition* Definition = UAssetDefinitionRegistry::Get()->GetAssetDefinitionForClass(UGSTN_CombatTimeline::StaticClass());
	TestTrue(TEXT("a timeline asset has the combat timeline definition"), Definition != nullptr && Definition->IsA<UGSTN_AssetDefinition_CombatTimeline>());
	if (Definition != nullptr)
	{
		TestFalse(TEXT("with a name"), Definition->GetAssetDisplayName().IsEmpty());
		TestTrue(TEXT("for the timeline class"), Definition->GetAssetClass().Get() == UGSTN_CombatTimeline::StaticClass());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RegistryPlayRangeTest, "GSTNCombat.Editor.Registry.PlayRangeIsEditorStateOnly", GSTN_REGISTRY_TEST_FLAGS)
bool FGSTN_RegistryPlayRangeTest::RunTest(const FString& Parameters)
{
	if (!HasEditorContext())
	{
		AddInfo(TEXT("no transaction buffer in this editor, skipped"));
		return true;
	}
	FFixture F;
	FGSTN_TimelineEditorModel& M = *F.Model;
	const int32 Steps = AppliedSteps();
	int32 Repaints = 0;
	const FDelegateHandle Handle = M.OnViewChanged.AddLambda([&Repaints]() { ++Repaints; });

	int32 First = -1;
	int32 End = -1;
	M.GetEffectivePlayRange(First, End);
	TestTrue(TEXT("no range loops the whole timeline"), First == 0 && End == 40 && !M.HasPlayRange());

	M.SetPlayRange(10, 25);
	M.GetEffectivePlayRange(First, End);
	TestTrue(TEXT("a loop range"), M.HasPlayRange() && First == 10 && End == 25);
	TestEqual(TEXT("repainted"), Repaints, 1);
	M.SetPlayRange(10, 25);
	TestEqual(TEXT("the same range again changes nothing"), Repaints, 1);

	M.SetPlayRange(30, 500);
	M.GetEffectivePlayRange(First, End);
	TestTrue(TEXT("cut to the timeline"), First == 30 && End == 40);
	M.SetPlayRange(20, 20);
	TestFalse(TEXT("an empty range clears it"), M.HasPlayRange());
	M.SetPlayRange(10, 25);
	M.SetTotalFrames(15);
	M.GetEffectivePlayRange(First, End);
	TestTrue(TEXT("a timeline shortened under the range cuts it"), First == 10 && End == 15);
	M.SetTotalFrames(8);
	M.GetEffectivePlayRange(First, End);
	TestTrue(TEXT("one shortened past it loops the whole timeline"), First == 0 && End == 8);

	// Playback over the range wraps inside it
	TestEqual(TEXT("steps wrap inside the range"), FGSTN_PlaybackClock::StepLooped(24, 3, 10, 25), 12);

	// Not data: the only undo steps are the one SetTotalFrames made
	TestEqual(TEXT("setting the range is no undo step"), AppliedSteps(), Steps + 2);
	M.OnViewChanged.Remove(Handle);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
