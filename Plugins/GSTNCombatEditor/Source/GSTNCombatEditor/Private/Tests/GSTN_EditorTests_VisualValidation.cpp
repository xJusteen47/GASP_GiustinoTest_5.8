#include "Framework/Application/SlateApplication.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Tests/GSTN_EditorTestTypes.h"
#include "Misc/AutomationTest.h"
#include "Styling/AppStyle.h"
#include "Tests/GSTN_VisualSnapshot.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_FindingsStore.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineLayout.h"
#include "Timeline/Widgets/SGSTN_TimelinePanel.h"
#include "UObject/StrongObjectPtr.h"
#include "Validation/GSTN_CombatValidators.h"
#include "Validation/GSTN_ValidationPanelModel.h"
#include "Validation/SGSTN_ValidationPanel.h"
#include "Widgets/Layout/SBorder.h"

#if WITH_DEV_AUTOMATION_TESTS

// Offscreen snapshots of the E11 widgets (GSTN.Visual.Validation.*): the validation panel with a result in it, and the timeline with the lane
// markers of a validation. Run like the other GSTN.Visual tests (Tests/GSTN_VisualSnapshot.h); they assert nothing about the look.

namespace
{
	TSharedRef<SWidget> OnEditorPanel(const TSharedRef<SWidget>& Content)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(0.f)
			[
				Content
			];
	}

	GSTNValidateAll::FAssetResult MakeAssetResult(const TCHAR* Name, std::initializer_list<TTuple<EGSTN_Severity, const TCHAR*, const TCHAR*>> Findings)
	{
		GSTNValidateAll::FAssetResult Asset;
		Asset.bValidated = true;
		Asset.AssetData.AssetName = Name;
		for (const TTuple<EGSTN_Severity, const TCHAR*, const TCHAR*>& Finding : Findings)
		{
			Asset.Report.Add(Finding.Get<0>(), FName(Finding.Get<1>()), FText::FromString(Finding.Get<2>()));
		}
		return Asset;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VisualValidationPanelTest, "GSTN.Visual.Validation.Panel", GSTN_VISUAL_TEST_FLAGS)
bool FGSTN_VisualValidationPanelTest::RunTest(const FString& Parameters)
{
	if (!GSTNVisualSnapshot::CanRender(*this))
	{
		return true;
	}

	GSTNValidateAll::FResult Result;
	Result.Assets.Add(MakeAssetResult(TEXT("TL_LightPunch"), {
		{ EGSTN_Severity::Error, TEXT("Timeline.HitData"), TEXT("Track 1, hitbox 0: Damage is -3, which is negative.") },
		{ EGSTN_Severity::Warning, TEXT("Timeline.HitboxKeys"), TEXT("Hitboxes 1.0 and 2.0 are in hit group 0, are open together and have the same Priority (0) with different hit data.") } }));
	Result.Assets.Add(MakeAssetResult(TEXT("HS_Humanoid"), { { EGSTN_Severity::Warning, TEXT("Hurtbox.Tag"), TEXT("Hurtbox 4 (Tail) has no Combat.Hurtbox tag: a modifier track cannot address it by tag") } }));
	Result.Assets.Add(MakeAssetResult(TEXT("TL_Clean"), {}));
	Result.Assets.Add(MakeAssetResult(TEXT("TL_Sweep"), { { EGSTN_Severity::Info, TEXT("Timeline.BakedStale"), TEXT("The socket 'foot_l' is baked but no hitbox uses it.") } }));

	const TSharedRef<FGSTN_ValidationPanelModel> Model = MakeShared<FGSTN_ValidationPanelModel>();
	Model->SetResult(MoveTemp(Result));
	const TSharedRef<SGSTN_ValidationPanel> Panel = SNew(SGSTN_ValidationPanel).Model(Model);
	TestFalse(TEXT("a snapshot was written"), GSTNVisualSnapshot::Render(*this, OnEditorPanel(Panel), FIntPoint(720, 360), TEXT("validation-panel")).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VisualLaneMarkersTest, "GSTN.Visual.Validation.LaneMarkers", GSTN_VISUAL_TEST_FLAGS)
bool FGSTN_VisualLaneMarkersTest::RunTest(const FString& Parameters)
{
	if (!GSTNVisualSnapshot::CanRender(*this))
	{
		return true;
	}

	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
	FGSTN_CombatTimelineData& Data = Timeline->Data;
	Data.TotalFrames = 40;
	Data.FirstActionableFrame = 30;
	FGSTN_TimelineTrack_Hitbox Hitbox;
	Hitbox.StartFrame = 6;
	Hitbox.EndFrame = 12;
	Hitbox.Part.GroupName = TEXT("Hands");
	FGSTN_HitboxSpec Spec;
	Spec.Data.Damage = -2.f;
	Spec.Keys.Add(FGSTN_HitboxKey());
	Hitbox.Hitboxes.Add(Spec);
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Hitbox));
	FGSTN_TimelineTrack_HurtboxModifier Armor;
	Armor.StartFrame = 2;
	Armor.EndFrame = 14;
	Armor.Kind = EGSTN_HurtboxModifierKind::Armor;
	Armor.AffectedHurtboxes.Add(TEXT("Torso"));
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_HurtboxModifier>(Armor));

	const TSharedRef<FGSTN_TimelineEditorModel> Model = MakeShared<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
	Model->SetTickRate(62.5f);
	const TSharedRef<FGSTN_FindingsStore> Findings = MakeShared<FGSTN_FindingsStore>();
	FGSTN_ValidationReport Report;
	FGSTN_CombatValidators::ValidateTimeline(Data, FGSTN_CombatValidators::MakeTimelineContext(Data, Timeline.Get(), 62.5f, false), Report);
	Findings->SetReport(Report);
	TestTrue(TEXT("the timeline has findings to mark"), Report.Num() >= 2);

	const TSharedRef<SGSTN_TimelinePanel> Panel = SNew(SGSTN_TimelinePanel).Model(Model).Findings(Findings);
	TestFalse(TEXT("a snapshot was written"), GSTNVisualSnapshot::Render(*this, OnEditorPanel(Panel), FIntPoint(1000, 300), TEXT("validation-lane-markers"), [&](int32)
	{
		const double LaneWidth = Panel->GetLaneWidth();
		if (LaneWidth > 0.0)
		{
			FGSTN_TimelineLayout::FrameAll(Model->ViewTransform, LaneWidth, Model->GetPlayableFrames());
			Model->NotifyViewChanged();
		}
	}).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VisualHitboxRowsTest, "GSTN.Visual.Timeline.HitboxRows", GSTN_VISUAL_TEST_FLAGS)
bool FGSTN_VisualHitboxRowsTest::RunTest(const FString& Parameters)
{
	if (!GSTNVisualSnapshot::CanRender(*this))
	{
		return true;
	}

	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
	FGSTN_CombatTimelineData& Data = Timeline->Data;
	Data.TotalFrames = 40;
	Data.FirstActionableFrame = 28;

	auto Key = [](int32 Frame, double X)
	{
		FGSTN_HitboxKey Result;
		Result.Frame = Frame;
		Result.Offset = FVector(X, 0.0, 0.0);
		return Result;
	};
	FGSTN_TimelineTrack_Hitbox Hitbox;
	Hitbox.StartFrame = 8;
	Hitbox.EndFrame = 20;
	Hitbox.Part.GroupName = TEXT("Hands");
	FGSTN_HitboxSpec Blade;
	Blade.HitGroup = 0;
	Blade.Keys = { Key(0, 0.0), Key(4, 40.0), Key(9, 90.0) };
	FGSTN_HitboxSpec Tip;
	Tip.HitGroup = 1;
	Tip.Space = EGSTN_HitboxSpace::BakedSocket;
	Tip.Socket = TEXT("blade_tip");
	Tip.Zone = EGSTN_HitboxZone::Sweet;
	Tip.Keys = { Key(2, 0.0), Key(6, 10.0) };
	FGSTN_HitboxSpec Guard;
	Guard.HitGroup = 2;
	Guard.Keys = { Key(0, 0.0), Key(11, 0.0) };
	Hitbox.Hitboxes = { Blade, Tip, Guard };
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Hitbox));

	for (const EGSTN_HurtboxModifierKind Kind : { EGSTN_HurtboxModifierKind::Invincible, EGSTN_HurtboxModifierKind::Intangible, EGSTN_HurtboxModifierKind::Armor })
	{
		FGSTN_TimelineTrack_HurtboxModifier Modifier;
		Modifier.StartFrame = 2;
		Modifier.EndFrame = 14;
		Modifier.Kind = Kind;
		Modifier.ArmorThreshold.Value = 30.f;
		Modifier.AffectedHurtboxes = { TEXT("Torso"), TEXT("Arm_R") };
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_HurtboxModifier>(Modifier));
	}

	const TSharedRef<FGSTN_TimelineEditorModel> Model = MakeShared<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
	Model->SetTickRate(62.5f);
	Model->SetSelection({ FGSTN_Selection::Key(0, 1), FGSTN_Selection::Key(0, 4) });
	Model->SetPlayhead(12);
	const TSharedRef<SGSTN_TimelinePanel> Panel = SNew(SGSTN_TimelinePanel).Model(Model);
	TestFalse(TEXT("a snapshot was written"), GSTNVisualSnapshot::Render(*this, OnEditorPanel(Panel), FIntPoint(1000, 300), TEXT("timeline-hitbox-rows"), [&](int32)
	{
		const double LaneWidth = Panel->GetLaneWidth();
		if (LaneWidth > 0.0)
		{
			FGSTN_TimelineLayout::FrameAll(Model->ViewTransform, LaneWidth, Model->GetPlayableFrames());
			Model->NotifyViewChanged();
		}
	}).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VisualHitDataRowTest, "GSTN.Visual.HitData.Row", GSTN_VISUAL_TEST_FLAGS)
bool FGSTN_VisualHitDataRowTest::RunTest(const FString& Parameters)
{
	if (!GSTNVisualSnapshot::CanRender(*this))
	{
		return true;
	}

	// The hit data row in a real details view, as the timeline editor's Details tab and a hitbox show it
	TStrongObjectPtr<UGSTN_TestHitHolder> Holder(NewObject<UGSTN_TestHitHolder>(GetTransientPackage()));
	Holder->Hit.Damage = 6.f;
	Holder->Hit.BaseKnockback = 400.f;
	Holder->Hit.KnockbackGrowth = 10.f;
	Holder->Hit.AngleDeg = 35.f;
	Holder->Hit.HitType = FGameplayTag::RequestGameplayTag(TEXT("Combat.HitType.Normal"), false);

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.bAllowSearch = false;
	const TSharedRef<IDetailsView> View = PropertyEditor.CreateDetailView(Args);
	View->SetObject(Holder.Get());
	TestFalse(TEXT("a snapshot was written"), GSTNVisualSnapshot::Render(*this, OnEditorPanel(View), FIntPoint(620, 760), TEXT("hit-data-row")).IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
