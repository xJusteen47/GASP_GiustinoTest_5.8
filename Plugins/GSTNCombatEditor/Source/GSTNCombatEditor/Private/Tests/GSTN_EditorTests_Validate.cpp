#include "Abilities/GameplayAbility.h"
#include "AssetRegistry/AssetData.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_CombatLockTags.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputSlot.h"
#include "Input/GSTN_InputTags.h"
#include "Input/GSTN_StandardSlots.h"
#include "Interfaces/IPluginManager.h"
#include "Kits/GSTN_CombatAssetTypes.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_SuperDefinition.h"
#include "Kits/GSTN_SuperInputScheme.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutRules.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/GSTN_EditorTestTypes.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineTrackEditor.h"
#include "UObject/StrongObjectPtr.h"
#include "Validation/GSTN_CombatValidators.h"
#include "Validation/GSTN_EditorValidators.h"
#include "Validation/GSTN_ValidationMessageLog.h"

#if WITH_DEV_AUTOMATION_TESTS

// The tests of the validation mechanism and the rules of combat-m5-editor 9 that exist with M3 (plan m5 E10). Each timeline rule has one
// failing and one passing case built in code. Kits, weapons and loadouts are transient objects: nothing is loaded from the project, so the
// run does not depend on the content of the machine it runs on. The Data Validation validators are called through their public entry
// (ValidateLoadedAsset) with a context of our own, like the subsystem does.

#define GSTN_VALIDATE_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNValidateTest
{
	using namespace GSTNValidationCodes;

	/** A timeline with nothing to complain about, to add one defect to */
	FGSTN_CombatTimelineData MakeData()
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 30;
		Data.FirstActionableFrame = 20;
		Data.OverrideFrames = 0;
		return Data;
	}

	template <typename T>
	void AddTrack(FGSTN_CombatTimelineData& Data, const T& Track)
	{
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<T>(Track));
	}

	FGSTN_TimelineTrack_Movement Move(int32 Start, int32 End, EGSTN_MoveMixMode Mix = EGSTN_MoveMixMode::Override)
	{
		FGSTN_TimelineTrack_Movement Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		Track.MixMode = Mix;
		return Track;
	}

	FGSTN_TimelineTrack_Tags TagsWindow(int32 Start, int32 End, std::initializer_list<FGameplayTag> Tags)
	{
		FGSTN_TimelineTrack_Tags Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		for (const FGameplayTag& Tag : Tags)
		{
			Track.Tags.AddTagFast(Tag);
		}
		return Track;
	}

	FGSTN_TimelineTrack_CancelWindow Cancel(int32 Start, int32 End, EGSTN_CancelCondition Condition, const FGameplayTagQuery& Into)
	{
		FGSTN_TimelineTrack_CancelWindow Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		Track.Condition = Condition;
		Track.IntoCommands = Into;
		return Track;
	}

	FGameplayTagQuery IntoSpecial()
	{
		return FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNCommandTags::Special.GetTag()));
	}

	FGSTN_TimelineRuleContext MakeContext()
	{
		FGSTN_TimelineRuleContext Context;
		Context.Core.TickRate = 60.f;
		return Context;
	}

	FGSTN_ValidationReport Run(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context = MakeContext())
	{
		FGSTN_ValidationReport Report;
		FGSTN_CombatValidators::ValidateTimeline(Data, Context, Report);
		return Report;
	}

	/** A tag the tag table does not know: the tag a loaded asset keeps when its tag was deleted or renamed */
	FGameplayTag MakeUnregisteredTag(FName Name)
	{
		FGameplayTag Tag;
		const FNameProperty* Property = CastField<FNameProperty>(FGameplayTag::StaticStruct()->FindPropertyByName(TEXT("TagName")));
		check(Property);
		Property->SetPropertyValue_InContainer(&Tag, Name);
		return Tag;
	}

	/** The input settings of the Core tests: fixed values, so a designer tuning the project does not change a test */
	TStrongObjectPtr<UGSTN_InputSettings> MakeSettings()
	{
		TStrongObjectPtr<UGSTN_InputSettings> Settings(NewObject<UGSTN_InputSettings>(GetTransientPackage()));
		Settings->DefaultBufferFrames = 8;
		Settings->bHoldBufferByDefault = true;
		Settings->MaxHoldBufferFrames = 0;
		Settings->DefaultSimultaneityFrames = 3;
		Settings->SimultaneousFrames = 2;
		Settings->BufferConflictPolicy = EGSTN_BufferConflictPolicy::MostRecent;
		Settings->bChordGraceByDefault = false;
		Settings->ChordGraceFrames = 3;
		Settings->StickDeadZone = 0.2f;
		Settings->AimZone = 0.5f;
		Settings->FlickFrames = 3;
		Settings->DiagonalHalfAngle = 22.5f;
		Settings->WorldReferenceYaw = 0.f;
		Settings->CommandTiers.Empty();
		Settings->CommandTiers.Add(GSTNCommandTags::Attack, 100);
		Settings->CommandTiers.Add(GSTNCommandTags::Strong, 200);
		Settings->CommandTiers.Add(GSTNCommandTags::Special, 300);
		Settings->CommandTiers.Add(GSTNCommandTags::Combo, 350);
		Settings->CommandTiers.Add(GSTNCommandTags::Super, 400);
		Settings->ExclusiveButtons = { GSTNCombatButtonTags::Button_Attack.GetTag(), GSTNCombatButtonTags::Button_Strong.GetTag(), GSTNCombatButtonTags::Button_Special.GetTag() };
		return Settings;
	}

	FGSTN_AbilityBinding Bind(const FGameplayTag& Command)
	{
		FGSTN_AbilityBinding Binding;
		Binding.Command = Command;
		Binding.Ability = TSoftClassPtr<UGameplayAbility>(UGameplayAbility::StaticClass());
		return Binding;
	}

	/** The standard slot at Index with another command */
	FGSTN_InputSlotSpec StandardSlot(int32 Index, const FGameplayTag& Command)
	{
		FGSTN_InputSlotSpec Spec = UGSTN_StandardSlots::Build()[Index];
		Spec.Command = Command;
		return Spec;
	}

	/**
	 * A kit whose command set is two slots: the plain attack and a twin with another command. With the twin built from the same slot they
	 * are the same input at the same rank (ambiguous); built from the directional attack they are not.
	 */
	struct FKitFixture
	{
		TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
		TStrongObjectPtr<UGSTN_InputSlotSet> EmptyBase;
		TStrongObjectPtr<UGSTN_PrimaryKitDefinition> Kit;
		FGameplayTag First = GSTNCommandTags::Attack_Neutral.GetTag();
		FGameplayTag Second = GSTNCommandTags::Attack.GetTag();

		explicit FKitFixture(bool bConflict)
		{
			// A base set of our own, so the project's default command set is not loaded into the test
			EmptyBase.Reset(NewObject<UGSTN_InputSlotSet>(GetTransientPackage()));
			Kit.Reset(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
			Kit->CommandSet.BaseSet = EmptyBase.Get();
			SetTwin(bConflict);
			Kit->KitBindings = { Bind(First), Bind(Second) };
		}

		/** The twin is the same input as the first slot (a conflict), or another one (none) */
		void SetTwin(bool bConflict)
		{
			Kit->CommandSet.AddedSlots.Reset();
			FGSTN_InputSlotEntry A;
			A.Inline = StandardSlot(0, First);
			FGSTN_InputSlotEntry B;
			B.Inline = StandardSlot(bConflict ? 0 : 1, Second);
			Kit->CommandSet.AddedSlots = { A, B };
		}

		FGSTN_ValidationReport Check() const
		{
			FGSTN_ValidationReport Report;
			FGSTN_CombatValidators::CheckKit(*Kit, *Settings, Report);
			return Report;
		}
	};

	int32 CountSlotFindings(const FGSTN_ValidationReport& Report)
	{
		return Report.CountCode(SlotAmbiguous()) + Report.CountCode(SlotShadowed()) + Report.CountCode(SlotDuplicateCommand()) + Report.CountCode(SlotInvalid());
	}

	/** Registers a track editor for the length of a test */
	struct FScopedEditor
	{
		explicit FScopedEditor(TSharedRef<const IGSTN_TrackEditor> Editor)
			: Struct(Editor->GetTrackStruct())
		{
			FGSTN_TrackEditorRegistry::Get().Register(Editor);
		}
		~FScopedEditor() { FGSTN_TrackEditorRegistry::Get().Unregister(Struct); }
		const UScriptStruct* Struct;
	};

	/** A track editor with a rule of its own */
	class FValidatingTrackEditor final : public IGSTN_TrackEditor
	{
	public:
		virtual const UScriptStruct* GetTrackStruct() const override { return FGSTN_TestKeyedTrack::StaticStruct(); }
		virtual FText GetDisplayName() const override { return FText::FromString(TEXT("Test Keyed")); }
		virtual void Validate(const FGSTN_TimelineTrack& Track, const FGSTN_CombatTimelineData& Data, FGSTN_ValidationReport& Report) const override
		{
			Report.Add(EGSTN_Severity::Warning, TEXT("Test.Custom"), FText::FromString(TEXT("a rule of the track editor")));
		}
	};

	/** The context Data Validation hands a validator */
	struct FContext
	{
		FDataValidationContext Context{ false, EDataValidationUsecase::Manual, {} };
	};
}

using namespace GSTNValidateTest;

// ---------------------------------------------------------------------------------------------------------------------------------
// The report

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateReportWidgetFreeTest, "GSTNCombat.Editor.Validate.ReportIsWidgetFree", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateReportWidgetFreeTest::RunTest(const FString& Parameters)
{
	// Read the public headers of the validation and the track extension point and look at their includes: no Slate, no widget, no module that has them
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNCombatEditor"));
	if (!TestTrue(TEXT("the plugin is found"), Plugin.IsValid()))
	{
		return false;
	}
	const FString Public = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/GSTNCombatEditor/Public"));
	for (const TCHAR* Relative : { TEXT("Validation/GSTN_ValidationReport.h"), TEXT("Timeline/GSTN_TimelineTrackEditor.h"), TEXT("Timeline/GSTN_TimelineEditContext.h") })
	{
		FString Text;
		if (!TestTrue(FString::Printf(TEXT("%s can be read"), Relative), FFileHelper::LoadFileToString(Text, *FPaths::Combine(Public, Relative))))
		{
			continue;
		}
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			if (Line.StartsWith(TEXT("#include")))
			{
				const bool bWidgetInclude = Line.Contains(TEXT("Slate")) || Line.Contains(TEXT("SWidget")) || Line.Contains(TEXT("Widgets/")) || Line.Contains(TEXT("Framework/"));
				TestFalse(FString::Printf(TEXT("%s: %s"), Relative, *Line), bWidgetInclude);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateErrorsInvalidateTest, "GSTNCombat.Editor.Validate.ErrorsInvalidateWarningsDoNot", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateErrorsInvalidateTest::RunTest(const FString& Parameters)
{
	// The report itself
	FGSTN_ValidationReport Report;
	Report.Add(EGSTN_Severity::Warning, TEXT("W"), FText::FromString(TEXT("w")));
	Report.Add(EGSTN_Severity::Info, TEXT("I"), FText::FromString(TEXT("i")));
	TestTrue(TEXT("warnings and infos leave a report valid"), Report.IsValid());
	Report.Add(EGSTN_Severity::Error, TEXT("E"), FText::FromString(TEXT("e")));
	TestFalse(TEXT("an error invalidates it"), Report.IsValid());

	// And the validator, the way Data Validation calls it: a timeline with a warning only, then with an error
	const TStrongObjectPtr<UGSTN_TimelineValidator> Validator(NewObject<UGSTN_TimelineValidator>(GetTransientPackage()));
	{
		const TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
		Timeline->Data = MakeData();
		AddTrack(Timeline->Data, FGSTN_TestKeyedTrack());		// no track editor: a warning
		Timeline->Data.Tracks[0].GetMutable<FGSTN_TestKeyedTrack>().EndFrame = 5;
		FContext Context;
		const EDataValidationResult Result = Validator->ValidateLoadedAsset(FAssetData(Timeline.Get()), Timeline.Get(), Context.Context);
		TestEqual(TEXT("a warning does not invalidate the asset"), Result, EDataValidationResult::Valid);
		TestEqual(TEXT("it is reported as a warning"), static_cast<int32>(Context.Context.GetNumWarnings()), 1);
		TestEqual(TEXT("and no error"), static_cast<int32>(Context.Context.GetNumErrors()), 0);
	}
	{
		const TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
		Timeline->Data = MakeData();
		Timeline->Data.TotalFrames = 0;
		Timeline->Data.FirstActionableFrame = 0;
		FContext Context;
		const EDataValidationResult Result = Validator->ValidateLoadedAsset(FAssetData(Timeline.Get()), Timeline.Get(), Context.Context);
		TestEqual(TEXT("an error invalidates the asset"), Result, EDataValidationResult::Invalid);
		TestTrue(TEXT("and is reported"), Context.Context.GetNumErrors() >= 1);
	}
	{
		// An asset that is not a timeline is not this validator's
		const TStrongObjectPtr<UGSTN_PrimaryKitDefinition> Kit(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
		FContext Context;
		TestEqual(TEXT("another kind of asset is not validated"), Validator->ValidateLoadedAsset(FAssetData(Kit.Get()), Kit.Get(), Context.Context), EDataValidationResult::NotValidated);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateNoDuplicateOfIsDataValidTest, "GSTNCombat.Editor.Validate.SkipsWhatIsDataValidSaid", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateNoDuplicateOfIsDataValidTest::RunTest(const FString& Parameters)
{
	// The timeline asset already reports Core's rules from its own IsDataValid, which Data Validation runs first. The validator must not say them twice
	const TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
	Timeline->Data = MakeData();
	Timeline->Data.FirstActionableFrame = 99;	// Core: FirstActionableFrame is after TotalFrames

	FContext Context;
	Timeline->IsDataValid(Context.Context);
	const int32 FromAsset = Context.Context.GetIssues().Num();
	TestTrue(TEXT("the asset reports the problem itself"), FromAsset >= 1);

	const TStrongObjectPtr<UGSTN_TimelineValidator> Validator(NewObject<UGSTN_TimelineValidator>(GetTransientPackage()));
	const EDataValidationResult Result = Validator->ValidateLoadedAsset(FAssetData(Timeline.Get()), Timeline.Get(), Context.Context);
	TestEqual(TEXT("the validator agrees it is invalid"), Result, EDataValidationResult::Invalid);
	TestEqual(TEXT("and adds nothing the asset had said"), Context.Context.GetIssues().Num(), FromAsset);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateMessageTest, "GSTNCombat.Editor.Validate.FindingMessage", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateMessageTest::RunTest(const FString& Parameters)
{
	FGSTN_ValidationFinding Finding;
	Finding.Severity = EGSTN_Severity::Warning;
	Finding.Code = TEXT("Slot.Shadowed");
	Finding.Message = FText::FromString(TEXT("A is shadowed by B"));
	TestTrue(TEXT("the text carries the code"), GSTNValidationLog::FormatFinding(Finding).ToString().Contains(TEXT("[Slot.Shadowed]")));
	TestEqual(TEXT("a warning is a warning"), GSTNValidationLog::ToMessageSeverity(Finding.Severity), EMessageSeverity::Warning);

	const auto HasAction = [](const TSharedRef<FTokenizedMessage>& Message)
	{
		return Message->GetMessageTokens().ContainsByPredicate([](const TSharedRef<IMessageToken>& Token) { return Token->GetType() == EMessageToken::Action; });
	};
	TestFalse(TEXT("no detail, no Show action"), HasAction(GSTNValidationLog::MakeMessage(Finding, false)));
	Finding.Detail = FText::FromString(TEXT("A: 0:+A\n  B: 0:+A"));
	TestTrue(TEXT("a detail gives the message a Show action"), HasAction(GSTNValidationLog::MakeMessage(Finding, false)));
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// The timeline rules: a failing and a passing case each

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateCleanTimelineTest, "GSTNCombat.Editor.Validate.CleanTimelineHasNoFindings", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateCleanTimelineTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data = MakeData();
	AddTrack(Data, Move(4, 10));
	AddTrack(Data, TagsWindow(0, 12, { GSTNCombatLockTags::Attack.GetTag() }));
	AddTrack(Data, Cancel(20, 30, EGSTN_CancelCondition::Always, IntoSpecial()));
	FGSTN_TimelineTrack_Event Event;
	Event.StartFrame = 8;
	AddTrack(Data, Event);

	const FGSTN_ValidationReport Report = Run(Data);
	TestEqual(TEXT("no finding"), Report.Num(), 0);
	TestTrue(TEXT("valid"), Report.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateFrameRangeTest, "GSTNCombat.Editor.Validate.FrameRange", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateFrameRangeTest::RunTest(const FString& Parameters)
{
	{
		FGSTN_CombatTimelineData Data = MakeData();
		Data.TotalFrames = 0;
		Data.FirstActionableFrame = 0;
		TestTrue(TEXT("a timeline of no frames is an error"), Run(Data).HasCode(FrameRange()));
	}
	{
		FGSTN_CombatTimelineData Data = MakeData();
		Data.FirstActionableFrame = 31;
		const FGSTN_ValidationReport Report = Run(Data);
		TestEqual(TEXT("FirstActionableFrame past the end is reported once"), Report.CountCode(FrameRange()), 1);
		TestFalse(TEXT("and is an error"), Report.IsValid());
	}
	{
		FGSTN_CombatTimelineData Data = MakeData();
		AddTrack(Data, Move(8, 8));
		const FGSTN_ValidationReport Report = Run(Data);
		const FGSTN_ValidationFinding* Finding = Report.FindCode(FrameRange());
		if (TestNotNull(TEXT("an empty window is reported"), Finding))
		{
			TestEqual(TEXT("on its track"), Finding->Where.TrackIndex, 0);
			TestEqual(TEXT("at its frame"), Finding->Where.Frame, 8);
		}
	}
	{
		FGSTN_CombatTimelineData Data = MakeData();
		AddTrack(Data, Move(-3, 5));
		TestTrue(TEXT("a negative start is reported"), Run(Data).HasCode(FrameRange()));
	}
	{
		FGSTN_CombatTimelineData Data = MakeData();
		AddTrack(Data, Move(0, 30));
		TestFalse(TEXT("a window that ends with the timeline is fine"), Run(Data).HasCode(FrameRange()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateMontageLengthTest, "GSTNCombat.Editor.Validate.MontageLength", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateMontageLengthTest::RunTest(const FString& Parameters)
{
	const FGSTN_CombatTimelineData Data = MakeData();	// 30 frames
	FGSTN_TimelineRuleContext Context = MakeContext();

	Context.Core.MontageLengthSeconds = 1.f;	// 60 frames at 60 Hz
	FGSTN_ValidationReport Failing = Run(Data, Context);
	TestEqual(TEXT("a montage twice as long is reported"), Failing.CountCode(MontageLength()), 1);
	TestTrue(TEXT("as a warning: the asset stays valid"), Failing.IsValid());
	TestEqual(TEXT("with severity warning"), Failing.FindCode(MontageLength())->Severity, EGSTN_Severity::Warning);

	Context.Core.MontageLengthSeconds = 0.5f;	// 30 frames
	TestFalse(TEXT("a montage of the same length is fine"), Run(Data, Context).HasCode(MontageLength()));

	Context.Core.MontageLengthSeconds = 0.5f + 1.f / 60.f;	// one frame off is allowed
	TestFalse(TEXT("one frame of difference is fine"), Run(Data, Context).HasCode(MontageLength()));

	Context.Core.MontageLengthSeconds = -1.f;
	TestFalse(TEXT("an unknown montage is not checked"), Run(Data, Context).HasCode(MontageLength()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateOverlapTest, "GSTNCombat.Editor.Validate.OverlapSameKind", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateOverlapTest::RunTest(const FString& Parameters)
{
	// Movement
	{
		FGSTN_CombatTimelineData Data = MakeData();
		AddTrack(Data, Move(2, 10));
		AddTrack(Data, Move(8, 14));
		const FGSTN_ValidationReport Report = Run(Data);
		TestEqual(TEXT("two movements that share frames"), Report.CountCode(OverlapSameKind()), 1);
		TestFalse(TEXT("are an error"), Report.IsValid());
		if (const FGSTN_ValidationFinding* Finding = Report.FindCode(OverlapSameKind()))
		{
			TestEqual(TEXT("on the second track"), Finding->Where.TrackIndex, 1);
			TestEqual(TEXT("at the first shared frame"), Finding->Where.Frame, 8);
		}
	}
	{
		FGSTN_CombatTimelineData Data = MakeData();
		AddTrack(Data, Move(2, 8));
		AddTrack(Data, Move(8, 14));
		TestFalse(TEXT("two movements that only touch do not overlap (half open windows)"), Run(Data).HasCode(OverlapSameKind()));
	}

	// Hurtbox modifiers of different states on the same part
	{
		FGSTN_TimelineTrack_HurtboxModifier A;
		A.StartFrame = 2;
		A.EndFrame = 10;
		A.Part.GroupName = TEXT("Legs");
		A.Kind = EGSTN_HurtboxModifierKind::Invincible;
		FGSTN_TimelineTrack_HurtboxModifier B = A;
		B.StartFrame = 6;
		B.EndFrame = 12;
		B.Kind = EGSTN_HurtboxModifierKind::Intangible;

		FGSTN_CombatTimelineData Data = MakeData();
		AddTrack(Data, A);
		AddTrack(Data, B);
		TestEqual(TEXT("invincible and intangible on the same group at once"), Run(Data).CountCode(OverlapSameKind()), 1);

		Data.Tracks[1].GetMutable<FGSTN_TimelineTrack_HurtboxModifier>().Kind = EGSTN_HurtboxModifierKind::Invincible;
		TestFalse(TEXT("the same state twice is redundant, not contradictory"), Run(Data).HasCode(OverlapSameKind()));

		Data.Tracks[1].GetMutable<FGSTN_TimelineTrack_HurtboxModifier>().Kind = EGSTN_HurtboxModifierKind::Intangible;
		Data.Tracks[1].GetMutable<FGSTN_TimelineTrack_HurtboxModifier>().Part.GroupName = TEXT("Arms");
		TestFalse(TEXT("another group is another hurtbox"), Run(Data).HasCode(OverlapSameKind()));
	}

	// Tags: the global lock next to a narrower one
	{
		FGSTN_CombatTimelineData Data = MakeData();
		AddTrack(Data, TagsWindow(0, 12, { GSTNInputTags::Lock_Command.GetTag() }));
		AddTrack(Data, TagsWindow(6, 18, { GSTNCombatLockTags::Attack.GetTag() }));
		const FGSTN_ValidationReport Report = Run(Data);
		TestEqual(TEXT("a narrower lock inside the global one"), Report.CountCode(OverlapSameKind()), 1);
		TestTrue(TEXT("is a warning: the asset stays valid"), Report.IsValid());

		Data.Tracks[1].GetMutable<FGSTN_TimelineTrack_Tags>().StartFrame = 12;
		TestFalse(TEXT("the same locks one after the other are fine"), Run(Data).HasCode(OverlapSameKind()));
	}
	{
		FGSTN_CombatTimelineData Data = MakeData();
		AddTrack(Data, TagsWindow(0, 12, { GSTNCombatLockTags::Special.GetTag() }));
		AddTrack(Data, TagsWindow(6, 18, { GSTNCombatLockTags::Attack.GetTag() }));
		TestFalse(TEXT("two narrow locks add up and are not a finding"), Run(Data).HasCode(OverlapSameKind()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateRootMotionMixTest, "GSTNCombat.Editor.Validate.RootMotionMix", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateRootMotionMixTest::RunTest(const FString& Parameters)
{
	FGSTN_TimelineRuleContext Context = MakeContext();
	Context.bMontageHasRootMotion = true;

	FGSTN_CombatTimelineData Data = MakeData();
	AddTrack(Data, Move(4, 10, EGSTN_MoveMixMode::Override));
	const FGSTN_ValidationReport Failing = Run(Data, Context);
	TestEqual(TEXT("root motion and a movement that overrides"), Failing.CountCode(RootMotionMix()), 1);
	TestTrue(TEXT("is a warning"), Failing.IsValid());

	Data.Tracks[0].GetMutable<FGSTN_TimelineTrack_Movement>().MixMode = EGSTN_MoveMixMode::Additive;
	TestFalse(TEXT("an additive movement is a decision made"), Run(Data, Context).HasCode(RootMotionMix()));

	Data.Tracks[0].GetMutable<FGSTN_TimelineTrack_Movement>().MixMode = EGSTN_MoveMixMode::Override;
	Context.bMontageHasRootMotion = false;
	TestFalse(TEXT("no root motion, nothing to fight"), Run(Data, Context).HasCode(RootMotionMix()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateDeferredCommitTest, "GSTNCombat.Editor.Validate.DeferredCommit", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateDeferredCommitTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data = MakeData();
	Data.OverrideFrames = 10;	// the timeline may still be replaced until frame 10
	FGSTN_TimelineTrack_Event Event;
	Event.StartFrame = 4;
	AddTrack(Data, Event);

	const FGSTN_ValidationReport Failing = Run(Data);
	TestEqual(TEXT("an event inside the override window"), Failing.CountCode(DeferredCommit()), 1);
	TestFalse(TEXT("is an error"), Failing.IsValid());
	if (const FGSTN_ValidationFinding* Finding = Failing.FindCode(DeferredCommit()))
	{
		TestEqual(TEXT("on its track"), Finding->Where.TrackIndex, 0);
	}

	Data.Tracks[0].GetMutable<FGSTN_TimelineTrack_Event>().StartFrame = 12;
	TestFalse(TEXT("an event after the commit is fine"), Run(Data).HasCode(DeferredCommit()));

	Data.OverrideFrames = 40;
	TestTrue(TEXT("an override window longer than the timeline is reported too"), Run(Data).HasCode(DeferredCommit()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateCancelWindowTest, "GSTNCombat.Editor.Validate.CancelWindowTarget", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateCancelWindowTest::RunTest(const FString& Parameters)
{
	{
		FGSTN_CombatTimelineData Data = MakeData();
		AddTrack(Data, Cancel(20, 30, EGSTN_CancelCondition::Always, FGameplayTagQuery()));
		const FGSTN_ValidationReport Report = Run(Data);
		TestEqual(TEXT("a window into nothing"), Report.CountCode(CancelWindowTarget()), 1);
		TestFalse(TEXT("is an error"), Report.IsValid());

		Data.Tracks[0].GetMutable<FGSTN_TimelineTrack_CancelWindow>().IntoCommands = IntoSpecial();
		TestFalse(TEXT("a window into the special commands is fine"), Run(Data).HasCode(CancelWindowTarget()));
	}
	{
		// On hit / on block need something that can hit
		FGSTN_CombatTimelineData Data = MakeData();
		AddTrack(Data, Cancel(20, 30, EGSTN_CancelCondition::OnHit, IntoSpecial()));
		const FGSTN_ValidationReport Report = Run(Data);
		TestEqual(TEXT("an on-hit window with no hitbox"), Report.CountCode(CancelWindowTarget()), 1);
		TestTrue(TEXT("is a warning"), Report.IsValid());

		FGSTN_TimelineTrack_Hitbox Hitbox;
		Hitbox.StartFrame = 5;
		Hitbox.EndFrame = 8;
		Hitbox.Part.GroupName = TEXT("Hands");
		AddTrack(Data, Hitbox);
		TestFalse(TEXT("with a hitbox it is fine"), Run(Data).HasCode(CancelWindowTarget()));
	}
	{
		// The commands of the kits that use the ability, when the caller has scanned them
		FGSTN_CombatTimelineData Data = MakeData();
		AddTrack(Data, Cancel(20, 30, EGSTN_CancelCondition::Always, IntoSpecial()));
		FGSTN_TimelineRuleContext Context = MakeContext();
		TArray<FGameplayTag> Commands = { GSTNCommandTags::Attack_Neutral.GetTag() };
		Context.KnownCommands = &Commands;
		TestEqual(TEXT("a window that matches no command of the kits"), Run(Data, Context).CountCode(CancelWindowTarget()), 1);

		Commands.Add(GSTNCommandTags::Special_Neutral.GetTag());
		TestFalse(TEXT("one that matches a command is fine (Special matches Special.Neutral by hierarchy)"), Run(Data, Context).HasCode(CancelWindowTarget()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateTagsTrackTest, "GSTNCombat.Editor.Validate.TagsTrack", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateTagsTrackTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Ghost = MakeUnregisteredTag(TEXT("GSTNTest.Ghost.Tag"));

	FGSTN_CombatTimelineData Data = MakeData();
	AddTrack(Data, TagsWindow(0, 12, { GSTNCombatLockTags::Attack.GetTag(), Ghost }));
	const FGSTN_ValidationReport Failing = Run(Data);
	TestEqual(TEXT("an unregistered tag in a Tags window"), Failing.CountCode(TagsTrack()), 1);
	TestFalse(TEXT("is an error"), Failing.IsValid());
	TestTrue(TEXT("and the message names the tag"), Failing.FindCode(TagsTrack())->Message.ToString().Contains(TEXT("GSTNTest.Ghost.Tag")));

	Data.Tracks[0].GetMutable<FGSTN_TimelineTrack_Tags>().Tags.RemoveTag(Ghost);
	TestFalse(TEXT("registered tags are fine"), Run(Data).HasCode(TagsTrack()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateUnregisteredTagTest, "GSTNCombat.Editor.Validate.UnregisteredTagIsError", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateUnregisteredTagTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Ghost = MakeUnregisteredTag(TEXT("GSTNTest.Ghost.Cue"));

	{
		FGSTN_CombatTimelineData Data = MakeData();
		FGSTN_TimelineTrack_Event Event;
		Event.StartFrame = 8;
		Event.CueTag = Ghost;
		AddTrack(Data, Event);
		const FGSTN_ValidationReport Report = Run(Data);
		TestEqual(TEXT("an event with an unregistered cue"), Report.CountCode(TagsTrack()), 1);
		TestFalse(TEXT("is an error"), Report.IsValid());
	}
	{
		FGSTN_CombatTimelineData Data = MakeData();
		FGSTN_TimelineTrack_Event Event;
		Event.StartFrame = 8;
		Event.GameplayEventTag = Ghost;
		AddTrack(Data, Event);
		TestEqual(TEXT("an event with an unregistered gameplay event tag"), Run(Data).CountCode(TagsTrack()), 1);
	}
	{
		FGSTN_CombatTimelineData Data = MakeData();
		Data.bHasRicochetResponse = true;
		Data.RicochetResponse.RecoilFrames = 10;
		Data.RicochetResponse.CueTag = Ghost;
		TestEqual(TEXT("a ricochet cue"), Run(Data).CountCode(TagsTrack()), 1);
	}
	{
		FGSTN_CombatTimelineData Data = MakeData();
		FGSTN_TimelineTrack_Event Event;
		Event.StartFrame = 8;
		AddTrack(Data, Event);
		TestFalse(TEXT("tags left empty are not a finding"), Run(Data).HasCode(TagsTrack()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateAimTurnTest, "GSTNCombat.Editor.Validate.AimTurn", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateAimTurnTest::RunTest(const FString& Parameters)
{
	{
		FGSTN_CombatTimelineData Data = MakeData();
		Data.AimPolicy = EGSTN_AimPolicy::Camera;
		Data.TurnUntilFrame = 31;
		TestEqual(TEXT("TurnUntilFrame past the end"), Run(Data).CountCode(AimTurn()), 1);
		Data.TurnUntilFrame = 12;
		TestFalse(TEXT("inside the timeline is fine"), Run(Data).HasCode(AimTurn()));
	}
	{
		FGSTN_CombatTimelineData Data = MakeData();
		Data.AimPolicy = EGSTN_AimPolicy::SnapToStick;
		Data.MaxTurnDegrees = 0.f;
		const FGSTN_ValidationReport Report = Run(Data);
		TestEqual(TEXT("a policy that turns with no degrees to turn"), Report.CountCode(AimTurn()), 1);
		TestFalse(TEXT("is an error"), Report.IsValid());
		Data.AimPolicy = EGSTN_AimPolicy::KeepFacing;
		TestFalse(TEXT("a move that keeps its facing needs none"), Run(Data).HasCode(AimTurn()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateUnknownTrackTest, "GSTNCombat.Editor.Validate.UnknownTrack", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateUnknownTrackTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data = MakeData();
	AddTrack(Data, Move(4, 10));
	FGSTN_TestKeyedTrack Unknown;
	Unknown.StartFrame = 2;
	Unknown.EndFrame = 6;
	AddTrack(Data, Unknown);

	const FGSTN_ValidationReport Report = Run(Data);
	TestEqual(TEXT("one track without a track editor"), Report.CountCode(UnknownTrack()), 1);
	TestTrue(TEXT("is a warning"), Report.IsValid());
	if (const FGSTN_ValidationFinding* Finding = Report.FindCode(UnknownTrack()))
	{
		TestEqual(TEXT("and it is the second track"), Finding->Where.TrackIndex, 1);
	}

	{
		FScopedEditor Scoped(MakeShared<FValidatingTrackEditor>());
		TestFalse(TEXT("once a track editor is registered the track is known"), Run(Data).HasCode(UnknownTrack()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateTrackEditorHookTest, "GSTNCombat.Editor.Validate.TrackEditorValidate", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateTrackEditorHookTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data = MakeData();
	AddTrack(Data, Move(4, 10));
	FGSTN_TestKeyedTrack Keyed;
	Keyed.StartFrame = 7;
	Keyed.EndFrame = 9;
	AddTrack(Data, Keyed);

	TestFalse(TEXT("without the editor there is no custom finding"), Run(Data).HasCode(TEXT("Test.Custom")));

	FScopedEditor Scoped(MakeShared<FValidatingTrackEditor>());
	const FGSTN_ValidationReport Report = Run(Data);
	TestEqual(TEXT("the track editor's own rule runs"), Report.CountCode(TEXT("Test.Custom")), 1);
	if (const FGSTN_ValidationFinding* Finding = Report.FindCode(TEXT("Test.Custom")))
	{
		TestEqual(TEXT("it is given the index of the track"), Finding->Where.TrackIndex, 1);
		TestEqual(TEXT("and its first frame"), Finding->Where.Frame, 7);
	}

	// The built-in editors add nothing by default
	FGSTN_CombatTimelineData Only = MakeData();
	AddTrack(Only, Move(4, 10));
	TestEqual(TEXT("the default Validate adds nothing"), Run(Only).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateCoreMessagesTest, "GSTNCombat.Editor.Validate.CoreMessagesClassified", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateCoreMessagesTest::RunTest(const FString& Parameters)
{
	// Every message of Core's timeline validation lands in a code of the table. The classification reads Core's wording, so a changed
	// message makes this test fail instead of the finding silently becoming Timeline.Core
	FGSTN_CombatTimelineData Data = MakeData();
	Data.FirstActionableFrame = 40;											// FirstActionableFrame is after TotalFrames
	Data.Tail.bHasTail = true;
	Data.Tail.StartFrame = 99;												// Tail start is after TotalFrames
	Data.OverrideFrames = 50;												// The override window is longer than the timeline
	AddTrack(Data, Move(8, 8));												// empty range
	AddTrack(Data, Move(5, 40));											// ends after TotalFrames (Core warns)
	FGSTN_TimelineTrack_Event Late;
	Late.StartFrame = 35;													// fires outside the timeline
	AddTrack(Data, Late);
	AddTrack(Data, FGSTN_TimelineTrack_Hitbox());							// addresses no part, group or tag
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>());				// an empty track
	Data.bHasRicochetResponse = true;										// enabled, but no recoil frames
	Data.RicochetResponse.RecoilFrames = 0;

	FGSTN_TimelineRuleContext Context = MakeContext();
	Context.Core.MontageLengthSeconds = 4.f;								// The montage lasts

	FGSTN_ValidationReport Report;
	FGSTN_CombatValidators::CheckCoreRules(Data, Context, Report);
	TestEqual(TEXT("nothing falls through to the generic code"), Report.CountCode(CoreOther()), 0);
	TestTrue(TEXT("frame range"), Report.HasCode(FrameRange()));
	TestTrue(TEXT("montage length"), Report.HasCode(MontageLength()));
	TestTrue(TEXT("deferred commit"), Report.HasCode(DeferredCommit()));
	TestTrue(TEXT("part selector"), Report.HasCode(PartSelector()));
	TestTrue(TEXT("empty track"), Report.HasCode(EmptyTrack()));
	TestTrue(TEXT("ricochet"), Report.HasCode(Ricochet()));

	// The track of a message is found in its text
	bool bFoundHitbox = false;
	for (const FGSTN_ValidationFinding& Finding : Report.Findings)
	{
		bFoundHitbox |= Finding.Code == PartSelector() && Finding.Where.TrackIndex == 3;
	}
	TestTrue(TEXT("the finding about the hitbox points at track 3"), bFoundHitbox);

	// And the severities are Core's: the empty range is an error, the track that ends late a warning
	int32 Errors = 0;
	int32 Warnings = 0;
	for (const FGSTN_ValidationFinding& Finding : Report.Findings)
	{
		Errors += Finding.Severity == EGSTN_Severity::Error ? 1 : 0;
		Warnings += Finding.Severity == EGSTN_Severity::Warning ? 1 : 0;
	}
	TestTrue(TEXT("errors and warnings both come through"), Errors >= 1 && Warnings >= 1);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Kit, weapon, preset

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateKitConflictTest, "GSTNCombat.Editor.Validate.KitConflictFlagsBothSlotNames", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateKitConflictTest::RunTest(const FString& Parameters)
{
	FKitFixture Fixture(true);
	const FGSTN_ValidationReport Report = Fixture.Check();

	TestEqual(TEXT("the two twins are one ambiguity"), Report.CountCode(SlotAmbiguous()), 1);
	const FGSTN_ValidationFinding* Finding = Report.FindCode(SlotAmbiguous());
	if (!TestNotNull(TEXT("and it is reported"), Finding))
	{
		return false;
	}
	TestEqual(TEXT("an error"), Finding->Severity, EGSTN_Severity::Error);
	const FString Message = Finding->Message.ToString();
	TestTrue(TEXT("the message names the first slot"), Message.Contains(Fixture.First.ToString()));
	TestTrue(TEXT("and the second"), Message.Contains(Fixture.Second.ToString()));

	// The Show action: both slots with their input in the notation of M1 section 4 (a press of the first button is "+A")
	const FString Detail = Finding->Detail.ToString();
	TestTrue(TEXT("the detail lists the first slot"), Detail.Contains(Fixture.First.ToString() + TEXT(":")));
	TestTrue(TEXT("and the second"), Detail.Contains(Fixture.Second.ToString() + TEXT(":")));
	TestTrue(TEXT("with the press of the attack button in the notation"), Detail.Contains(TEXT(":+A")));
	TestTrue(TEXT("and says which button A is"), Detail.Contains(TEXT("A=") + GSTNCombatButtonTags::Button_Attack.GetTag().ToString()));
	TestFalse(TEXT("a kit is not told where its slots come from"), Message.Contains(TEXT("comes from")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateFixClearsTheErrorTest, "GSTNCombat.Editor.Validate.FixClearsTheError", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateFixClearsTheErrorTest::RunTest(const FString& Parameters)
{
	FKitFixture Fixture(true);
	TestTrue(TEXT("the conflicting kit is flagged"), Fixture.Check().HasCode(SlotAmbiguous()));

	Fixture.SetTwin(false);
	const FGSTN_ValidationReport Fixed = Fixture.Check();
	TestEqual(TEXT("giving the twin another input clears it"), CountSlotFindings(Fixed), 0);

	// A slot with no binding never fires and is not compared: the table is what the game runs
	Fixture.SetTwin(true);
	Fixture.Kit->KitBindings = { Bind(Fixture.First) };
	TestEqual(TEXT("an unbound twin is not a conflict in play"), CountSlotFindings(Fixture.Check()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateWeaponOnKitTest, "GSTNCombat.Editor.Validate.WeaponReportsWhatItAdds", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateWeaponOnKitTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Attack = GSTNCommandTags::Attack.GetTag();
	{
		// The kit has its own conflict; a weapon that adds no slot is not blamed for it
		FKitFixture Fixture(true);
		const TStrongObjectPtr<UGSTN_WeaponDefinition> Weapon(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
		FGSTN_ValidationReport Report;
		FGSTN_CombatValidators::CheckWeaponOnKit(*Weapon, *Fixture.Kit, *Fixture.Settings, Report);
		TestEqual(TEXT("the kit's own conflict is the kit's finding"), CountSlotFindings(Report), 0);
	}
	{
		// The kit is clean; the weapon brings a slot that is the same input as the kit's
		FKitFixture Fixture(false);
		Fixture.Kit->CommandSet.AddedSlots.RemoveAt(1);
		Fixture.Kit->KitBindings = { Bind(Fixture.First) };
		const TStrongObjectPtr<UGSTN_WeaponDefinition> Weapon(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
		FGSTN_AbilityBinding Binding = Bind(Attack);
		Binding.SlotOverride.Inline = StandardSlot(0, Attack);
		Weapon->Bindings = { Binding };

		FGSTN_ValidationReport Report;
		FGSTN_CombatValidators::CheckWeaponOnKit(*Weapon, *Fixture.Kit, *Fixture.Settings, Report);
		TestEqual(TEXT("the weapon's slot collides with the kit's"), Report.CountCode(SlotAmbiguous()), 1);
		if (const FGSTN_ValidationFinding* Finding = Report.FindCode(SlotAmbiguous()))
		{
			const FString Message = Finding->Message.ToString();
			TestTrue(TEXT("the message starts with the kit"), Message.StartsWith(TEXT("On kit ")));
			TestTrue(TEXT("and says the second slot comes from the weapon"), Message.Contains(TEXT("comes from weapon")) && Message.Contains(Weapon->GetName()));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateKitCarriesWeaponTest, "GSTNCombat.Editor.Validate.KitCanCarryWeapon", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateKitCarriesWeaponTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_PrimaryKitDefinition> Kit(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
	const TStrongObjectPtr<UGSTN_WeaponDefinition> Primary(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
	const TStrongObjectPtr<UGSTN_WeaponDefinition> Other(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));

	TestFalse(TEXT("an unrelated weapon"), FGSTN_CombatValidators::KitCanCarryWeapon(*Kit, *Other));
	Kit->PrimaryWeapon = Primary.Get();
	TestTrue(TEXT("the primary weapon"), FGSTN_CombatValidators::KitCanCarryWeapon(*Kit, *Primary));
	TestFalse(TEXT("another weapon is still not carried"), FGSTN_CombatValidators::KitCanCarryWeapon(*Kit, *Other));

	const FGameplayTag Role = GSTNCommandTags::Special.GetTag();	// any registered tag stands for the weapon's family
	Other->WeaponTags.AddTag(Role);
	Kit->AllowedAdditionalWeapons = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(Role));
	TestTrue(TEXT("an additional weapon the query allows"), FGSTN_CombatValidators::KitCanCarryWeapon(*Kit, *Other));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidatePresetConflictTest, "GSTNCombat.Editor.Validate.PresetConflictNamesWeaponOrSuper", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidatePresetConflictTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Attack = GSTNCommandTags::Attack.GetTag();

	// A clean kit with one slot, and a loadout that names it
	FKitFixture Fixture(false);
	Fixture.Kit->CommandSet.AddedSlots.RemoveAt(1);
	Fixture.Kit->KitBindings = { Bind(Fixture.First) };
	Fixture.Kit->SuperSlotCount = 0;

	FGSTN_ResolvedLoadout Resolved;
	Resolved.Kit = Fixture.Kit.Get();
	Resolved.Loadout.PrimaryKit = FPrimaryAssetId(GSTNCombatAssetTypes::PrimaryKit(), TEXT("TestKit"));

	const UGSTN_LoadoutRules& Rules = *GetDefault<UGSTN_LoadoutRules>();
	{
		FGSTN_ValidationReport Report;
		FGSTN_CombatValidators::CheckLoadout(Resolved, *Fixture.Settings, Rules, nullptr, Report);
		TestEqual(TEXT("the clean loadout has no finding"), Report.Num(), 0);
	}

	// Weapon: its slot is the kit's input under another command
	const TStrongObjectPtr<UGSTN_WeaponDefinition> Weapon(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
	FGSTN_AbilityBinding WeaponBinding = Bind(Attack);
	WeaponBinding.SlotOverride.Inline = StandardSlot(0, Attack);
	Weapon->Bindings = { WeaponBinding };
	Resolved.Weapons = { Weapon.Get() };
	{
		FGSTN_ValidationReport Report;
		FGSTN_CombatValidators::CheckLoadout(Resolved, *Fixture.Settings, Rules, nullptr, Report);
		TestEqual(TEXT("the weapon makes the loadout ambiguous"), Report.CountCode(SlotAmbiguous()), 1);
		TestEqual(TEXT("which the rules do not repeat"), Report.CountCode(LoadoutRule()), 0);
		TestFalse(TEXT("and it is an error"), Report.IsValid());
		if (const FGSTN_ValidationFinding* Finding = Report.FindCode(SlotAmbiguous()))
		{
			const FString Message = Finding->Message.ToString();
			TestTrue(TEXT("the finding names both slots"), Message.Contains(Fixture.First.ToString()) && Message.Contains(Attack.ToString()));
			TestTrue(TEXT("and the weapon that brings the second"), Message.Contains(TEXT("comes from weapon 0")) && Message.Contains(Weapon->GetName()));
		}
	}

	// Super: the scheme's fixed slot is the kit's input too; the super outranks it, so the kit's slot is shadowed
	Resolved.Weapons.Reset();
	const TStrongObjectPtr<UGSTN_SuperInputScheme> Scheme(NewObject<UGSTN_SuperInputScheme>(GetTransientPackage()));
	Scheme->Mode = EGSTN_SuperInputMode::FixedSlots;
	FGSTN_InputSlotSpec SuperSlot = StandardSlot(0, FGameplayTag());	// no command: the compiler gives it the super slot's
	Scheme->FixedSlots = { SuperSlot };
	const TStrongObjectPtr<UGSTN_SuperDefinition> Super(NewObject<UGSTN_SuperDefinition>(GetTransientPackage()));
	Super->Ability = TSoftClassPtr<UGameplayAbility>(UGameplayAbility::StaticClass());
	const FGameplayTag SuperFamily = GSTNCommandTags::Super.GetTag();	// any registered tag stands for the super's family
	Super->SuperTags.AddTag(SuperFamily);
	Fixture.Kit->AllowedSupers = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(SuperFamily));
	Fixture.Kit->SuperSlotCount = 1;
	Resolved.SuperScheme = Scheme.Get();
	Resolved.Supers = { Super.Get() };
	Resolved.Loadout.Supers = { FPrimaryAssetId(GSTNCombatAssetTypes::Super(), TEXT("TestSuper")) };
	{
		FGSTN_ValidationReport Report;
		FGSTN_CombatValidators::CheckLoadout(Resolved, *Fixture.Settings, Rules, nullptr, Report);
		TestEqual(TEXT("the super shadows the kit's slot"), Report.CountCode(SlotShadowed()), 1);
		TestTrue(TEXT("a shadowed slot is a warning: the loadout stays valid"), Report.IsValid());
		if (const FGSTN_ValidationFinding* Finding = Report.FindCode(SlotShadowed()))
		{
			TestTrue(TEXT("and the message names the super slot"), Finding->Message.ToString().Contains(TEXT("comes from super slot 1")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidatePresetValidatorTest, "GSTNCombat.Editor.Validate.PresetValidatorTakesAnyLoadoutHolder", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidatePresetValidatorTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_TestLoadoutHolder> Holder(NewObject<UGSTN_TestLoadoutHolder>(GetTransientPackage()));
	TArray<TPair<FName, const FGSTN_LoadoutData*>> Loadouts;
	UGSTN_LoadoutPresetValidator::GatherLoadouts(*Holder, Loadouts);
	if (!TestEqual(TEXT("the loadout property is found"), Loadouts.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("by its name"), Loadouts[0].Key, FName(TEXT("Loadout")));
	TestTrue(TEXT("and points at the value in the object"), Loadouts[0].Value == &Holder->Loadout);

	// An empty loadout names no kit: the rules call that an error
	const TStrongObjectPtr<UGSTN_LoadoutPresetValidator> Validator(NewObject<UGSTN_LoadoutPresetValidator>(GetTransientPackage()));
	FContext Context;
	TestEqual(TEXT("an empty preset is invalid"), Validator->ValidateLoadedAsset(FAssetData(Holder.Get()), Holder.Get(), Context.Context), EDataValidationResult::Invalid);
	TestTrue(TEXT("and says why"), Context.Context.GetNumErrors() >= 1);

	// An asset with no loadout is not its business
	const TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
	FContext Other;
	TestEqual(TEXT("another kind of asset is not validated"), Validator->ValidateLoadedAsset(FAssetData(Timeline.Get()), Timeline.Get(), Other.Context), EDataValidationResult::NotValidated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateNotationTest, "GSTNCombat.Editor.Validate.SlotInputNotation", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateNotationTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const FGSTN_InputSlotSpec Slot = StandardSlot(0, GSTNCommandTags::Attack_Neutral.GetTag());
	const FGSTN_InputSlotSpec Directional = StandardSlot(1, GSTNCommandTags::Attack_Directional.GetTag());

	TArray<const FGSTN_InputSlotSpec*> Specs = { &Slot, &Directional };
	TArray<FGameplayTag> Buttons;
	FGSTN_CombatValidators::GatherButtonTags(Specs, Buttons);
	TestEqual(TEXT("both slots use the attack button"), Buttons.Num(), 1);

	const FString Neutral = FGSTN_CombatValidators::DescribeSlotInput(Slot, Buttons, *Settings);
	const FString Tilted = FGSTN_CombatValidators::DescribeSlotInput(Directional, Buttons, *Settings);
	TestTrue(TEXT("a plain attack is a press of A"), Neutral.Contains(TEXT("+A")));
	TestTrue(TEXT("a directional attack also tilts the stick"), Tilted.Contains(TEXT("t")) && Tilted.Contains(TEXT("+A")));
	TestNotEqual(TEXT("the two read differently"), Neutral, Tilted);
	TestTrue(TEXT("frames count from the first event"), Neutral.StartsWith(TEXT("0:")));

	// A malformed pattern has no notation
	FGSTN_InputSlotSpec Empty;
	TestTrue(TEXT("a slot without steps has none"), FGSTN_CombatValidators::DescribeSlotInput(Empty, Buttons, *Settings).IsEmpty());
	return true;
}

// Review 2026-10-23 C.2: a weapon switch whose StartFrame drifted from its SwapFrame is a stale copy the designer should hear about
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateWeaponSwitchFrameTest, "GSTNCombat.Editor.Validate.WeaponSwitchFrameMismatch", GSTN_VALIDATE_TEST_FLAGS)
bool FGSTN_ValidateWeaponSwitchFrameTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data = GSTNValidateTest::MakeData();
	FGSTN_TimelineTrack_WeaponSwitch Switch;
	Switch.StartFrame = 4;
	Switch.EndFrame = 5;
	Switch.SwapFrame = 4;
	GSTNValidateTest::AddTrack(Data, Switch);
	TestFalse(TEXT("a switch whose two frames agree has no finding"), GSTNValidateTest::Run(Data).HasCode(TEXT("WeaponSwitch.FrameMismatch")));

	Data.Tracks[0].GetMutable<FGSTN_TimelineTrack_WeaponSwitch>().SwapFrame = 18;
	const FGSTN_ValidationReport Report = GSTNValidateTest::Run(Data);
	TestEqual(TEXT("a switch that starts at 4 and swaps at 18 is reported once"), Report.CountCode(TEXT("WeaponSwitch.FrameMismatch")), 1);
	if (const FGSTN_ValidationFinding* Finding = Report.FindCode(TEXT("WeaponSwitch.FrameMismatch")))
	{
		TestTrue(TEXT("it is a warning"), Finding->Severity == EGSTN_Severity::Warning);
		TestEqual(TEXT("it points at the track"), Finding->Where.TrackIndex, 0);
		TestEqual(TEXT("and at the frame the switch really swaps"), Finding->Where.Frame, 18);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
