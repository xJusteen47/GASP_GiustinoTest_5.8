#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Abilities/GameplayAbility.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Settings/GSTN_CombatEditorSettings.h"
#include "Timeline/Details/GSTN_TimelineDataCustomization.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_InlineTimeline.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineUndo.h"
#include "Timeline/Widgets/SGSTN_InlineTimelineBar.h"
#include "UObject/StrongObjectPtr.h"
#include "Validation/GSTN_CombatValidators.h"
#include "Validation/GSTN_EditorValidators.h"

#if WITH_DEV_AUTOMATION_TESTS

// The tests of inline mode (plan m5 E13): the timeline of an ability Blueprint edited through the class default object, the validator for
// it, and the extraction into an asset. The Blueprints are made here, in a temporary package, from the real ability class
// (UGSTN_GA_BaseCombat) and compiled for real; no project content is loaded, and BP_GASNPP_TestCharacter is never touched. The toolkit
// window needs a Slate application, which a -nullrhi run does not have: what the toolkit decides is a plain function (DecideAfterCompile)
// and is tested here, the window itself is in the smoke steps of the commit.

#define GSTN_INLINE_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNInlineTest
{
	using FTrackInstance = TInstancedStruct<FGSTN_TimelineTrack>;

	/** An ability Blueprint of the real class, compiled, in a package of its own so it can be dirty and clean like a saved asset */
	struct FBlueprintFixture
	{
		TStrongObjectPtr<UBlueprint> Blueprint;
		TStrongObjectPtr<UPackage> Package;

		explicit FBlueprintFixture(UClass* Parent = UGSTN_GA_BaseCombat::StaticClass())
		{
			static int32 Counter = 0;
			const FString Name = FString::Printf(TEXT("GSTNInlineTest_%d_%s"), ++Counter, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
			Package.Reset(CreatePackage(*(FString(TEXT("/Temp/")) + Name)));
			Blueprint.Reset(FKismetEditorUtilities::CreateBlueprint(Parent, Package.Get(), FName(*Name), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName(TEXT("GSTNInlineTest"))));
			if (Blueprint.IsValid() && Blueprint->GeneratedClass == nullptr)
			{
				FKismetEditorUtilities::CompileBlueprint(Blueprint.Get());
			}
			MakeClean();
		}

		~FBlueprintFixture()
		{
			if (Package.IsValid())
			{
				Package->ClearDirtyFlag();
			}
			if (Blueprint.IsValid())
			{
				Blueprint->ClearFlags(RF_Standalone | RF_Public);
			}
		}

		bool IsOk() const { return Blueprint.IsValid() && Blueprint->GeneratedClass != nullptr; }

		/** What a freshly saved Blueprint looks like: compiled and not dirty */
		void MakeClean()
		{
			if (Blueprint.IsValid())
			{
				Blueprint->Status = BS_UpToDate;
			}
			if (Package.IsValid())
			{
				Package->ClearDirtyFlag();
			}
		}

		void Compile() { FKismetEditorUtilities::CompileBlueprint(Blueprint.Get()); }

		/** The live default object: a compile makes a new one, so ask every time */
		UGSTN_GA_BaseCombat* Ability() const
		{
			return (Blueprint.IsValid() && Blueprint->GeneratedClass) ? Cast<UGSTN_GA_BaseCombat>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
		}

		FGSTN_TimelineEditContext Context() const { return FGSTN_TimelineEditContext::ForBlueprint(Blueprint.Get()); }
	};

	/** The whole data as text: what two copies must share to be "exactly the same data" */
	FString Export(const FGSTN_CombatTimelineData& Data)
	{
		FString Text;
		FGSTN_CombatTimelineData::StaticStruct()->ExportText(Text, &Data, nullptr, nullptr, PPF_None, nullptr);
		return Text;
	}

	/** 30 frames, actionable from 20, one movement window [4, 10) and one tags window [0, 12) */
	void FillData(FGSTN_CombatTimelineData& Data)
	{
		Data.TotalFrames = 30;
		Data.FirstActionableFrame = 20;
		Data.OverrideFrames = 0;
		FGSTN_TimelineTrack_Movement Movement;
		Movement.StartFrame = 4;
		Movement.EndFrame = 10;
		Data.Tracks.Add(FTrackInstance::Make<FGSTN_TimelineTrack_Movement>(Movement));
		FGSTN_TimelineTrack_Tags Tags;
		Tags.StartFrame = 0;
		Tags.EndFrame = 12;
		Data.Tracks.Add(FTrackInstance::Make<FGSTN_TimelineTrack_Tags>(Tags));
	}

	/** Steps of the undo history that are currently applied */
	int32 AppliedSteps()
	{
		return (GEditor && GEditor->Trans) ? GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() : 0;
	}
}

using namespace GSTNInlineTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InlineResolveTest, "GSTNCombat.Editor.Inline.ContextResolvesCdoTimeline", GSTN_INLINE_TEST_FLAGS)
bool FGSTN_InlineResolveTest::RunTest(const FString& Parameters)
{
	FBlueprintFixture Fixture;
	if (!TestTrue(TEXT("the test Blueprint of the ability class was made and compiled"), Fixture.IsOk()))
	{
		return false;
	}
	UGSTN_GA_BaseCombat* CDO = Fixture.Ability();
	TestNotNull(TEXT("the generated class has a default object"), CDO);
	if (!CDO)
	{
		return false;
	}
	FillData(CDO->Timeline);

	const FGSTN_TimelineEditContext Context = Fixture.Context();
	TestTrue(TEXT("a Blueprint of the ability can hold an inline timeline"), FGSTN_TimelineEditContext::CanEditInline(Fixture.Blueprint.Get()));
	TestTrue(TEXT("the context is inline"), Context.IsInline());
	TestTrue(TEXT("and resolves"), Context.IsValid());
	TestTrue(TEXT("to the Timeline property of the default object"), Context.Get() == &CDO->Timeline);
	TestTrue(TEXT("the owner is the default object"), Context.GetOwner() == CDO);
	TestTrue(TEXT("the target is the Blueprint, which is what Save saves"), Context.GetTarget() == Fixture.Blueprint.Get());
	TestTrue(TEXT("the Blueprint is reachable from the context"), Context.GetBlueprint() == Fixture.Blueprint.Get());
	TestFalse(TEXT("no timeline asset is named, so the inline one plays"), Context.IsInlineShadowedByAsset());
	TestEqual(TEXT("the display name is the Blueprint's"), Context.GetDisplayName().ToString(), Fixture.Blueprint->GetName());

	// A model on it reads the same data the default object holds
	FGSTN_TimelineEditorModel Model(Context);
	TestEqual(TEXT("the model reads the inline total"), Model.GetTotalFrames(), 30);
	TestEqual(TEXT("and the tracks"), Model.GetTrackCount(), 2);

	// What a details row of the property resolves to
	const FGSTN_TimelineEditContext FromCdo = FGSTN_TimelineEditContext::FromOwnerObject(CDO);
	TestTrue(TEXT("the live default object resolves from a details row"), FromCdo.IsValid() && FromCdo == Context);
	if (Fixture.Blueprint->SkeletonGeneratedClass)
	{
		TestFalse(TEXT("the skeleton class default object does not"), FGSTN_TimelineEditContext::FromOwnerObject(Fixture.Blueprint->SkeletonGeneratedClass->GetDefaultObject(false)).IsValid());
	}
	TestFalse(TEXT("an object that is no default object does not"), FGSTN_TimelineEditContext::FromOwnerObject(Fixture.Blueprint.Get()).IsValid());
	TestFalse(TEXT("nothing does not"), FGSTN_TimelineEditContext::FromOwnerObject(nullptr).IsValid());
	const TStrongObjectPtr<UGSTN_CombatTimeline> Asset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
	const FGSTN_TimelineEditContext FromAsset = FGSTN_TimelineEditContext::FromOwnerObject(Asset.Get());
	TestTrue(TEXT("a timeline asset resolves from a details row"), FromAsset.IsValid() && !FromAsset.IsInline() && FromAsset.Get() == &Asset->Data);

	// The row is registered for the struct the ability and the asset keep their data in
	TestEqual(TEXT("the customization is for the timeline data struct"), FGSTN_TimelineDataCustomization::GetStructName(), FName(TEXT("GSTN_CombatTimelineData")));

	// A Blueprint that is not an ability does not resolve
	FBlueprintFixture Other(UObject::StaticClass());
	if (TestTrue(TEXT("a Blueprint of another class was made"), Other.IsOk()))
	{
		TestFalse(TEXT("it cannot hold an inline timeline"), FGSTN_TimelineEditContext::CanEditInline(Other.Blueprint.Get()));
		TestFalse(TEXT("and its context is not valid"), Other.Context().IsValid());
	}
	TestFalse(TEXT("no Blueprint is no context"), FGSTN_TimelineEditContext::ForBlueprint(nullptr).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InlineCompileTest, "GSTNCombat.Editor.Inline.SurvivesBlueprintCompile", GSTN_INLINE_TEST_FLAGS)
bool FGSTN_InlineCompileTest::RunTest(const FString& Parameters)
{
	FBlueprintFixture Fixture;
	if (!TestTrue(TEXT("the test Blueprint was made"), Fixture.IsOk()) || !TestNotNull(TEXT("with a default object"), Fixture.Ability()))
	{
		return false;
	}
	FillData(Fixture.Ability()->Timeline);
	const FString Before = Export(Fixture.Ability()->Timeline);

	const FGSTN_TimelineEditContext Context = Fixture.Context();
	FGSTN_TimelineEditorModel Model(Context);
	Model.SetTickRate(62.5f);
	Model.SetSelection({ FGSTN_Selection::Window(1) });

	// What the toolkit sees when the compile finishes: the context has to resolve already, or it would close an editor that is fine
	int32 Calls = 0;
	bool bValidInCallback = false;
	GSTNInlineTimeline::EAfterCompile DecisionInCallback = GSTNInlineTimeline::EAfterCompile::Close;
	const FDelegateHandle Handle = Fixture.Blueprint->OnCompiled().AddLambda([&](UBlueprint*)
	{
		++Calls;
		bValidInCallback = Context.IsValid();
		DecisionInCallback = GSTNInlineTimeline::DecideAfterCompile(Context);
	});

	const UObject* OwnerBefore = Context.GetOwner();
	Fixture.Compile();
	Fixture.Blueprint->OnCompiled().Remove(Handle);

	TestTrue(TEXT("the compile announced itself"), Calls >= 1);
	TestTrue(TEXT("the context resolves inside the announcement"), bValidInCallback);
	TestTrue(TEXT("and the editor refreshes, it does not close"), DecisionInCallback == GSTNInlineTimeline::EAfterCompile::Refresh);
	TestTrue(TEXT("the context resolves after the compile"), Context.IsValid());
	TestTrue(TEXT("to the default object that is live now"), Context.GetOwner() == Fixture.Ability());
	UE_LOG(LogTemp, Log, TEXT("GSTN inline test: the default object %s by the compile"), OwnerBefore == Context.GetOwner() ? TEXT("was kept") : TEXT("was replaced"));
	TestEqual(TEXT("the values were carried over"), Export(*Context.Get()), Before);

	// The model keeps working on the new default object
	Model.HandleUndoRedo();
	TestEqual(TEXT("the selection is still valid"), Model.GetSelection().Num(), 1);
	TestTrue(TEXT("an edit after the compile reaches the live default object"), Model.SetTotalFrames(40));
	TestEqual(TEXT("the live default object has it"), Fixture.Ability()->Timeline.TotalFrames, 40);

	// And a second compile with the edit in place
	Fixture.Compile();
	TestEqual(TEXT("the edit survives another compile"), Context.Get() ? Context.Get()->TotalFrames : -1, 40);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InlineGoneTest, "GSTNCombat.Editor.Inline.ToolkitClosesWhenPropertyGone", GSTN_INLINE_TEST_FLAGS)
bool FGSTN_InlineGoneTest::RunTest(const FString& Parameters)
{
	FBlueprintFixture Fixture;
	if (!TestTrue(TEXT("the test Blueprint was made"), Fixture.IsOk()) || !TestNotNull(TEXT("with a default object"), Fixture.Ability()))
	{
		return false;
	}
	FillData(Fixture.Ability()->Timeline);
	const FGSTN_TimelineEditContext Context = Fixture.Context();
	FGSTN_TimelineEditorModel Model(Context);
	TestTrue(TEXT("an inline editor is fine while the property is there"), GSTNInlineTimeline::DecideAfterCompile(Context) == GSTNInlineTimeline::EAfterCompile::Refresh);

	// The Blueprint is reparented to a class without the property and compiled
	GSTNInlineTimeline::EAfterCompile DecisionInCallback = GSTNInlineTimeline::EAfterCompile::Refresh;
	const FDelegateHandle Handle = Fixture.Blueprint->OnCompiled().AddLambda([&](UBlueprint*) { DecisionInCallback = GSTNInlineTimeline::DecideAfterCompile(Context); });
	Fixture.Blueprint->ParentClass = UGameplayAbility::StaticClass();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Fixture.Blueprint.Get());
	Fixture.Compile();
	Fixture.Blueprint->OnCompiled().Remove(Handle);

	TestFalse(TEXT("the Blueprint cannot hold an inline timeline any more"), FGSTN_TimelineEditContext::CanEditInline(Fixture.Blueprint.Get()));
	TestFalse(TEXT("the context no longer resolves"), Context.IsValid());
	TestTrue(TEXT("the editor is told to close, inside the compile announcement"), DecisionInCallback == GSTNInlineTimeline::EAfterCompile::Close);
	TestTrue(TEXT("and also after it"), GSTNInlineTimeline::DecideAfterCompile(Context) == GSTNInlineTimeline::EAfterCompile::Close);

	// Until the window is gone the model and the views read nothing instead of crashing, and an edit does nothing
	TestEqual(TEXT("the model reads no frames"), Model.GetTotalFrames(), 0);
	TestEqual(TEXT("and no tracks"), Model.GetTrackCount(), 0);
	Model.HandleUndoRedo();
	const int32 Steps = AppliedSteps();
	TestFalse(TEXT("an edit on a lost property does nothing"), Model.SetTotalFrames(50));
	TestEqual(TEXT("and leaves no undo step"), AppliedSteps(), Steps);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InlineModifiedTest, "GSTNCombat.Editor.Inline.EditMarksBlueprintModified", GSTN_INLINE_TEST_FLAGS)
bool FGSTN_InlineModifiedTest::RunTest(const FString& Parameters)
{
	FBlueprintFixture Fixture;
	if (!TestTrue(TEXT("the test Blueprint was made"), Fixture.IsOk()) || !TestNotNull(TEXT("with a default object"), Fixture.Ability()))
	{
		return false;
	}
	FillData(Fixture.Ability()->Timeline);
	Fixture.MakeClean();
	TestFalse(TEXT("the package starts clean"), Fixture.Package->IsDirty());
	TestTrue(TEXT("and the Blueprint up to date"), Fixture.Blueprint->Status == BS_UpToDate);

	FGSTN_TimelineEditorModel Model(Fixture.Context());
	TestTrue(TEXT("an edit through the model happens"), Model.SetFirstActionableFrame(18));
	TestEqual(TEXT("it is in the default object"), Fixture.Ability()->Timeline.FirstActionableFrame, 18);
	TestTrue(TEXT("the Blueprint's package is dirty, so Save has something to write"), Fixture.Package->IsDirty());
	TestTrue(TEXT("the Blueprint is marked out of date"), Fixture.Blueprint->Status == BS_Dirty);

	// An edit that changes nothing marks nothing
	Fixture.MakeClean();
	TestFalse(TEXT("an edit that changes nothing is refused"), Model.SetFirstActionableFrame(18));
	TestFalse(TEXT("and leaves the package clean"), Fixture.Package->IsDirty());

	// Every kind of mutation marks it: a window edit goes through the same notification
	TestTrue(TEXT("a window move happens"), Model.MoveWindow(0, 2));
	TestTrue(TEXT("and marks the package"), Fixture.Package->IsDirty());

	// The editors save what GetObjectsCurrentlyBeingEdited returns
	TestTrue(TEXT("what the editor saves is the Blueprint"), Fixture.Context().GetTarget() == Fixture.Blueprint.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InlineUndoTest, "GSTNCombat.Editor.Inline.UndoRestoresCdoValue", GSTN_INLINE_TEST_FLAGS)
bool FGSTN_InlineUndoTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), GEditor != nullptr && GEditor->Trans != nullptr))
	{
		return false;
	}
	FBlueprintFixture Fixture;
	if (!TestTrue(TEXT("the test Blueprint was made"), Fixture.IsOk()) || !TestNotNull(TEXT("with a default object"), Fixture.Ability()))
	{
		return false;
	}
	FillData(Fixture.Ability()->Timeline);

	const FGSTN_TimelineEditContext Context = Fixture.Context();
	FGSTN_TimelineEditorModel Model(Context);
	FGSTN_TimelineUndoClient UndoClient(Model);
	const int32 StartSteps = AppliedSteps();
	const FString Before = Export(*Context.Get());

	TestTrue(TEXT("an edit happens"), Model.MoveWindow(1, 3));
	const FString After = Export(*Context.Get());
	TestNotEqual(TEXT("it changed the default object"), After, Before);
	TestEqual(TEXT("as one undo step"), AppliedSteps(), StartSteps + 1);

	TestTrue(TEXT("undo"), GEditor->UndoTransaction());
	TestEqual(TEXT("undo restores the default object exactly"), Export(*Context.Get()), Before);
	TestTrue(TEXT("redo"), GEditor->RedoTransaction());
	TestEqual(TEXT("redo restores the edit exactly"), Export(*Context.Get()), After);

	// A drag on the inline timeline is one step too, and escape leaves none
	TestTrue(TEXT("a drag opens"), Model.BeginDrag(FText::FromString(TEXT("Drag"))));
	Model.MoveWindow(0, 1);
	Model.MoveWindow(0, 1);
	Model.EndDrag();
	TestEqual(TEXT("a drag is one undo step"), AppliedSteps(), StartSteps + 2);
	const FString AfterDrag = Export(*Context.Get());
	TestTrue(TEXT("a second drag opens"), Model.BeginDrag(FText::FromString(TEXT("Cancelled drag"))));
	Model.MoveWindow(0, 2);
	Model.CancelDrag();
	TestEqual(TEXT("a cancelled drag restores the data"), Export(*Context.Get()), AfterDrag);
	TestEqual(TEXT("and leaves no step"), AppliedSteps(), StartSteps + 2);

	// A default object made by a compile is transactional too, or this undo would have nothing to restore
	Fixture.Compile();
	const FString AfterCompile = Export(*Context.Get());
	TestTrue(TEXT("an edit after a compile happens"), Model.SetTotalFrames(44));
	TestTrue(TEXT("undo after a compile"), GEditor->UndoTransaction());
	TestEqual(TEXT("restores the new default object"), Export(*Context.Get()), AfterCompile);

	while (AppliedSteps() > StartSteps && GEditor->UndoTransaction())
	{
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InlineExtractTest, "GSTNCombat.Editor.Inline.ExtractCreatesAssetAndClearsInline", GSTN_INLINE_TEST_FLAGS)
bool FGSTN_InlineExtractTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), GEditor != nullptr && GEditor->Trans != nullptr))
	{
		return false;
	}
	FBlueprintFixture Fixture;
	if (!TestTrue(TEXT("the test Blueprint was made"), Fixture.IsOk()) || !TestNotNull(TEXT("with a default object"), Fixture.Ability()))
	{
		return false;
	}
	FillData(Fixture.Ability()->Timeline);
	Fixture.Ability()->SetFlags(RF_Transactional);
	Fixture.MakeClean();
	const FGSTN_TimelineEditContext Context = Fixture.Context();
	const FString Inline = Export(Fixture.Ability()->Timeline);
	const int32 StartSteps = AppliedSteps();

	const TStrongObjectPtr<UGSTN_CombatTimeline> Asset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
	TestFalse(TEXT("a null asset is refused"), GSTNInlineTimeline::ExtractToAsset(Context, nullptr));
	TestFalse(TEXT("an asset context has nothing to extract"), GSTNInlineTimeline::ExtractToAsset(FGSTN_TimelineEditContext::ForAsset(Asset.Get()), Asset.Get()));
	TestEqual(TEXT("a refusal changes nothing"), Export(Fixture.Ability()->Timeline), Inline);

	TestTrue(TEXT("the extraction happens"), GSTNInlineTimeline::ExtractToAsset(Context, Asset.Get()));
	TestEqual(TEXT("the asset holds the inline data"), Export(Asset->Data), Inline);
	TestEqual(TEXT("the inline timeline is empty"), Export(Fixture.Ability()->Timeline), Export(FGSTN_CombatTimelineData()));
	TestTrue(TEXT("the ability names the asset"), Fixture.Ability()->TimelineAsset.Get() == Asset.Get());
	TestTrue(TEXT("so the inline timeline is shadowed"), Context.IsInlineShadowedByAsset());
	TestTrue(TEXT("the Blueprint is marked modified"), Fixture.Package->IsDirty() && Fixture.Blueprint->Status == BS_Dirty);
	TestEqual(TEXT("the ability plays the asset's data"), Export(Fixture.Ability()->GetTimelineData()), Inline);
	TestEqual(TEXT("it is one undo step"), AppliedSteps(), StartSteps + 1);

	TestTrue(TEXT("undo"), GEditor->UndoTransaction());
	TestEqual(TEXT("undo brings the inline timeline back"), Export(Fixture.Ability()->Timeline), Inline);
	TestTrue(TEXT("and clears the asset reference"), Fixture.Ability()->TimelineAsset.IsNull());
	TestTrue(TEXT("redo"), GEditor->RedoTransaction());
	TestTrue(TEXT("redo points at the asset again"), Fixture.Ability()->TimelineAsset.Get() == Asset.Get());
	TestEqual(TEXT("and empties the inline data"), Export(Fixture.Ability()->Timeline), Export(FGSTN_CombatTimelineData()));

	while (AppliedSteps() > StartSteps && GEditor->UndoTransaction())
	{
	}

	// The names offered to the save dialog: next to the Blueprint
	FString Path;
	FString Name;
	GSTNInlineTimeline::GetExtractDefaults(Context, Path, Name);
	TestEqual(TEXT("the asset goes next to the Blueprint"), Path, FString(TEXT("/Temp")));
	TestEqual(TEXT("and is named after it"), Name, Fixture.Blueprint->GetName() + TEXT("_Timeline"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InlineOneEditorTest, "GSTNCombat.Editor.Inline.TwoToolkitsSameBlueprintRefused", GSTN_INLINE_TEST_FLAGS)
bool FGSTN_InlineOneEditorTest::RunTest(const FString& Parameters)
{
	FBlueprintFixture First;
	FBlueprintFixture Second;
	if (!TestTrue(TEXT("two Blueprints were made"), First.IsOk() && Second.IsOk()))
	{
		return false;
	}

	int32 FocusRequests = 0;
	TSharedPtr<FGSTN_TimelineContextLease> Lease = FGSTN_TimelineEditContextRegistry::Acquire(First.Context(), [&FocusRequests]() { ++FocusRequests; });
	TestTrue(TEXT("the first editor gets the Blueprint"), Lease.IsValid());
	TestTrue(TEXT("the Blueprint is open"), FGSTN_TimelineEditContextRegistry::IsOpen(First.Context()));
	TestFalse(TEXT("another Blueprint is not"), FGSTN_TimelineEditContextRegistry::IsOpen(Second.Context()));

	TestFalse(TEXT("a second editor on the same Blueprint is refused"), FGSTN_TimelineEditContextRegistry::Acquire(First.Context()).IsValid());
	TestEqual(TEXT("and the open one is asked to focus"), FocusRequests, 1);
	TestFalse(TEXT("through the compile too: a new default object is the same target"), (First.Compile(), FGSTN_TimelineEditContextRegistry::Acquire(First.Context()).IsValid()));
	TestEqual(TEXT("which focuses again"), FocusRequests, 2);

	TSharedPtr<FGSTN_TimelineContextLease> Other = FGSTN_TimelineEditContextRegistry::Acquire(Second.Context());
	TestTrue(TEXT("a different Blueprint opens freely"), Other.IsValid());

	// The asset a Blueprint extracts to is its own target
	const TStrongObjectPtr<UGSTN_CombatTimeline> Asset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
	TSharedPtr<FGSTN_TimelineContextLease> AssetLease = FGSTN_TimelineEditContextRegistry::Acquire(FGSTN_TimelineEditContext::ForAsset(Asset.Get()));
	TestTrue(TEXT("an asset opens while the Blueprint is open"), AssetLease.IsValid());

	Lease.Reset();
	TestFalse(TEXT("closing the editor frees the Blueprint"), FGSTN_TimelineEditContextRegistry::IsOpen(First.Context()));
	TestTrue(TEXT("and it can be opened again"), FGSTN_TimelineEditContextRegistry::Acquire(First.Context()).IsValid());

	// A Blueprint that cannot resolve is not opened at all
	FBlueprintFixture NotAnAbility(UObject::StaticClass());
	TestFalse(TEXT("a Blueprint without an inline timeline cannot be claimed"), FGSTN_TimelineEditContextRegistry::Acquire(NotAnAbility.Context()).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InlineValidatorTest, "GSTNCombat.Editor.Validate.AbilityInlineTimeline", GSTN_INLINE_TEST_FLAGS)
bool FGSTN_InlineValidatorTest::RunTest(const FString& Parameters)
{
	FBlueprintFixture Fixture;
	if (!TestTrue(TEXT("the test Blueprint was made"), Fixture.IsOk()) || !TestNotNull(TEXT("with a default object"), Fixture.Ability()))
	{
		return false;
	}
	UGSTN_GA_BaseCombat* Ability = Fixture.Ability();
	const TStrongObjectPtr<UGSTN_AbilityTimelineValidator> Validator(NewObject<UGSTN_AbilityTimelineValidator>(GetTransientPackage()));
	UBlueprint* Blueprint = Fixture.Blueprint.Get();
	const FAssetData AssetData(Blueprint);
	TestTrue(TEXT("the validator takes a Blueprint of the ability"), UGSTN_AbilityTimelineValidator::IsAbilityBlueprint(Blueprint));

	// The same rules as the timeline asset validator: the same codes for the same data
	auto CodesOf = [](const FGSTN_ValidationReport& Report)
	{
		TArray<FName> Codes;
		for (const FGSTN_ValidationFinding& Finding : Report.Findings)
		{
			Codes.Add(Finding.Code);
		}
		return Codes;
	};

	// A clean inline timeline
	FillData(Ability->Timeline);
	{
		FDataValidationContext Context;
		TestEqual(TEXT("a clean inline timeline is valid"), Validator->ValidateLoadedAsset(AssetData, Blueprint, Context), EDataValidationResult::Valid);
		TestEqual(TEXT("with no finding"), Context.GetIssues().Num(), 0);
	}

	// A defect: the first actionable frame is after the end
	Ability->Timeline.FirstActionableFrame = 99;
	{
		FDataValidationContext Context;
		TestEqual(TEXT("a broken inline timeline is invalid"), Validator->ValidateLoadedAsset(AssetData, Blueprint, Context), EDataValidationResult::Invalid);
		TestTrue(TEXT("and says why"), Context.GetNumErrors() >= 1);

		FGSTN_ValidationReport Direct;
		FGSTN_CombatValidators::ValidateTimeline(Ability->Timeline, FGSTN_CombatValidators::MakeTimelineContext(Ability->Timeline, Blueprint, UGSTN_CombatEditorSettings::GetSimulationTickRate(), true), Direct);
		FGSTN_ValidationReport ViaAbility;
		FGSTN_CombatValidators::ValidateAbilityTimeline(*Ability, Blueprint, UGSTN_CombatEditorSettings::GetSimulationTickRate(), true, ViaAbility);
		TestTrue(TEXT("the ability check is the timeline check on the inline data"), CodesOf(Direct) == CodesOf(ViaAbility) && !Direct.Findings.IsEmpty());
		TestTrue(TEXT("the findings belong to the Blueprint"), ViaAbility.Findings.Num() > 0 && ViaAbility.Findings[0].Asset == Blueprint);
	}

	// An empty inline timeline (no length) is an error like an empty asset
	Ability->Timeline = FGSTN_CombatTimelineData();
	{
		FDataValidationContext Context;
		TestEqual(TEXT("an empty inline timeline is invalid"), Validator->ValidateLoadedAsset(AssetData, Blueprint, Context), EDataValidationResult::Invalid);
	}

	// A Timeline Asset wins: the inline data is not checked, and dead inline data is a warning
	const TStrongObjectPtr<UGSTN_CombatTimeline> Asset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
	Ability->TimelineAsset = Asset.Get();
	{
		FDataValidationContext Context;
		TestEqual(TEXT("an empty inline timeline next to an asset is valid"), Validator->ValidateLoadedAsset(AssetData, Blueprint, Context), EDataValidationResult::Valid);
		TestEqual(TEXT("with no finding"), Context.GetIssues().Num(), 0);
	}
	FillData(Ability->Timeline);
	Ability->Timeline.FirstActionableFrame = 99;
	{
		FDataValidationContext Context;
		TestEqual(TEXT("a broken inline timeline that does not play is not an error"), Validator->ValidateLoadedAsset(AssetData, Blueprint, Context), EDataValidationResult::Valid);
		TestEqual(TEXT("it is one warning"), static_cast<int32>(Context.GetNumWarnings()), 1);
		TestEqual(TEXT("and no error"), static_cast<int32>(Context.GetNumErrors()), 0);

		FGSTN_ValidationReport Report;
		FGSTN_CombatValidators::ValidateAbilityTimeline(*Ability, Blueprint, 62.5f, false, Report);
		TestTrue(TEXT("the warning has its code"), Report.Findings.Num() == 1 && Report.Findings[0].Code == GSTNValidationCodes::AbilityInlineIgnored());
	}

	// Another Blueprint is not this validator's
	FBlueprintFixture Other(UObject::StaticClass());
	if (TestTrue(TEXT("a Blueprint of another class was made"), Other.IsOk()))
	{
		FDataValidationContext Context;
		TestFalse(TEXT("another Blueprint is not taken"), UGSTN_AbilityTimelineValidator::IsAbilityBlueprint(Other.Blueprint.Get()));
		TestEqual(TEXT("and not validated"), Validator->ValidateLoadedAsset(FAssetData(Other.Blueprint.Get()), Other.Blueprint.Get(), Context), EDataValidationResult::NotValidated);
	}
	const TStrongObjectPtr<UGSTN_CombatTimeline> NotABlueprint(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
	TestFalse(TEXT("a timeline asset is the timeline validator's, not this one's"), UGSTN_AbilityTimelineValidator::IsAbilityBlueprint(NotABlueprint.Get()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InlineBarTextTest, "GSTNCombat.Editor.Inline.BarWording", GSTN_INLINE_TEST_FLAGS)
bool FGSTN_InlineBarTextTest::RunTest(const FString& Parameters)
{
	FBlueprintFixture Fixture;
	if (!TestTrue(TEXT("the test Blueprint was made"), Fixture.IsOk()) || !TestNotNull(TEXT("with a default object"), Fixture.Ability()))
	{
		return false;
	}
	const TStrongObjectPtr<UGSTN_CombatTimeline> Asset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
	TestTrue(TEXT("an asset editor has no strip text"), SGSTN_InlineTimelineBar::MakeBarText(FGSTN_TimelineEditContext::ForAsset(Asset.Get())).IsEmpty());

	const FString Plain = SGSTN_InlineTimelineBar::MakeBarText(Fixture.Context()).ToString();
	TestTrue(TEXT("the strip names the Blueprint"), Plain.Contains(Fixture.Blueprint->GetName()));
	TestFalse(TEXT("and does not warn while the inline timeline plays"), Plain.Contains(TEXT("does not play")));

	Fixture.Ability()->TimelineAsset = Asset.Get();
	TestTrue(TEXT("it says so when a Timeline Asset wins"), SGSTN_InlineTimelineBar::MakeBarText(Fixture.Context()).ToString().Contains(TEXT("does not play")));
	return true;
}

// Review 2026-10-23 C.3: Extract silently replaced the Timeline Asset an ability already named, which changed what runs
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InlineExtractReplaceNeedsConfirmTest, "GSTNCombat.Editor.Inline.ExtractNeverReplacesANamedAssetWithoutConfirmation", GSTN_INLINE_TEST_FLAGS)
bool FGSTN_InlineExtractReplaceNeedsConfirmTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), GEditor != nullptr && GEditor->Trans != nullptr))
	{
		return false;
	}
	FBlueprintFixture Fixture;
	if (!TestTrue(TEXT("the test Blueprint was made"), Fixture.IsOk()) || !TestNotNull(TEXT("with a default object"), Fixture.Ability()))
	{
		return false;
	}
	FillData(Fixture.Ability()->Timeline);
	Fixture.Ability()->SetFlags(RF_Transactional);

	// The ability already names an asset, so the inline data above is shadowed and does not play
	const TStrongObjectPtr<UGSTN_CombatTimeline> Existing(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
	Existing->Data.TotalFrames = 77;
	Fixture.Ability()->TimelineAsset = Existing.Get();
	Fixture.MakeClean();
	const FGSTN_TimelineEditContext Context = Fixture.Context();
	TestTrue(TEXT("the extraction would replace the asset"), GSTNInlineTimeline::ExtractWouldReplaceAsset(Context));

	const FString Inline = Export(Fixture.Ability()->Timeline);
	const int32 StartSteps = AppliedSteps();
	const TStrongObjectPtr<UGSTN_CombatTimeline> Asset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
	auto ExpectUntouched = [&](const TCHAR* When)
	{
		TestTrue(FString::Printf(TEXT("%s: the ability still names the old asset"), When), Fixture.Ability()->TimelineAsset.Get() == Existing.Get());
		TestEqual(FString::Printf(TEXT("%s: the inline data is as it was"), When), Export(Fixture.Ability()->Timeline), Inline);
		TestEqual(FString::Printf(TEXT("%s: the new asset is empty"), When), Asset->Data.TotalFrames, 0);
		TestEqual(FString::Printf(TEXT("%s: the old asset is as it was"), When), Existing->Data.TotalFrames, 77);
		TestEqual(FString::Printf(TEXT("%s: no undo step was left"), When), AppliedSteps(), StartSteps);
	};

	TestFalse(TEXT("without a confirmation it is refused"), GSTNInlineTimeline::ExtractToAsset(Context, Asset.Get()));
	ExpectUntouched(TEXT("no callback"));

	int32 Asked = 0;
	FSoftObjectPath AskedAbout;
	TestFalse(TEXT("a no is refused"), GSTNInlineTimeline::ExtractToAsset(Context, Asset.Get(), [&](const FSoftObjectPath& Path)
	{
		++Asked;
		AskedAbout = Path;
		return false;
	}));
	TestEqual(TEXT("the callback was asked once"), Asked, 1);
	TestTrue(TEXT("... about the asset the ability names"), AskedAbout == FSoftObjectPath(Existing.Get()));
	ExpectUntouched(TEXT("a no"));

	TestTrue(TEXT("a yes goes through"), GSTNInlineTimeline::ExtractToAsset(Context, Asset.Get(), [](const FSoftObjectPath&) { return true; }));
	TestTrue(TEXT("the ability names the new asset"), Fixture.Ability()->TimelineAsset.Get() == Asset.Get());
	TestEqual(TEXT("the new asset holds the inline data"), Export(Asset->Data), Inline);
	TestEqual(TEXT("the old asset was not edited"), Existing->Data.TotalFrames, 77);
	TestEqual(TEXT("it is one undo step"), AppliedSteps(), StartSteps + 1);

	TestTrue(TEXT("undo"), GEditor->UndoTransaction());
	TestTrue(TEXT("undo puts the old asset back"), Fixture.Ability()->TimelineAsset.Get() == Existing.Get());

	// With no asset named nothing is asked, as before
	Fixture.Ability()->TimelineAsset = nullptr;
	TestFalse(TEXT("nothing to replace"), GSTNInlineTimeline::ExtractWouldReplaceAsset(Context));
	int32 AskedAgain = 0;
	TestTrue(TEXT("the plain extraction needs no answer"), GSTNInlineTimeline::ExtractToAsset(Context, Asset.Get(), [&](const FSoftObjectPath&) { ++AskedAgain; return false; }));
	TestEqual(TEXT("and asks nothing"), AskedAgain, 0);

	while (AppliedSteps() > StartSteps && GEditor->UndoTransaction())
	{
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
