#include "Framework/Application/SlateApplication.h"
#include "Hits/GSTN_HitData.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Tests/GSTN_EditorTestTypes.h"
#include "Tests/GSTN_VisualSnapshot.h"
#include "Hits/GSTN_KnockbackModels.h"
#include "Hits/GSTN_HitMathUtils.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_CombatRules.h"
#include "Timeline/Details/GSTN_HitDataCustomization.h"
#include "Timeline/Details/GSTN_HitDataMath.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

// The hit data row (plan m5 E12): the angle dial's geometry and the knockback preview, which are plain functions, and a guard that the row lays out
// every field of the struct. The widgets are checked by hand (Slate, mouse): see the README.

#define GSTN_HITDATA_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNHitDataTest
{
	/** Rules with a model of our choice and fixed numbers, so a designer's tuning of the project does not change a test */
	TStrongObjectPtr<UGSTN_CombatRules> MakeRules(UClass* ModelClass)
	{
		TStrongObjectPtr<UGSTN_CombatRules> Rules(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
		Rules->KnockbackModel = ModelClass ? NewObject<UGSTN_KnockbackModel>(Rules.Get(), ModelClass) : nullptr;
		Rules->StaggerThreshold = 0.f;
		Rules->Sakurai.GroundedLowAngleDeg = 0.f;
		Rules->Sakurai.HighAngleDeg = 44.f;
		Rules->Sakurai.KnockbackThreshold = 800.f;
		if (Rules->KnockbackModel)
		{
			Rules->KnockbackModel->Tuning = FGSTN_ReactionTuning();
		}
		return Rules;
	}

	FGSTN_HitData MakeHit()
	{
		FGSTN_HitData Hit;
		Hit.Damage = 6.f;
		Hit.BaseKnockback = 400.f;
		Hit.KnockbackGrowth = 10.f;
		Hit.FixedKnockback = -1.f;
		Hit.AngleDeg = 45.f;
		return Hit;
	}
}

using namespace GSTNHitDataTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitDataDialTest, "GSTNCombat.Editor.HitData.AngleDialMapsToAngleDeg", GSTN_HITDATA_TEST_FLAGS)
bool FGSTN_HitDataDialTest::RunTest(const FString& Parameters)
{
	const FVector2D Pivot(14.0, 50.0);

	// Screen y grows down: a point right of the pivot and above it is a positive elevation
	TestEqual(TEXT("straight ahead is level"), GSTNHitDataDial::AngleFromPoint(Pivot, Pivot + FVector2D(60.0, 0.0), 0.f), 0.f);
	TestEqual(TEXT("up and ahead by the same amount is 45"), GSTNHitDataDial::AngleFromPoint(Pivot, Pivot + FVector2D(40.0, -40.0), 0.f), 45.f);
	TestEqual(TEXT("down and ahead is -45"), GSTNHitDataDial::AngleFromPoint(Pivot, Pivot + FVector2D(40.0, 40.0), 0.f), -45.f);
	TestTrue(TEXT("a shallow rise is a small angle"), FMath::IsNearlyEqual(GSTNHitDataDial::AngleFromPoint(Pivot, Pivot + FVector2D(100.0, -10.0), 0.f), 5.710593f, 1e-3f));

	// Over the pivot or behind it the needle is straight up or down, whichever side the pointer is on
	TestEqual(TEXT("straight above"), GSTNHitDataDial::AngleFromPoint(Pivot, Pivot + FVector2D(0.0, -30.0), 0.f), 90.f);
	TestEqual(TEXT("behind and above"), GSTNHitDataDial::AngleFromPoint(Pivot, Pivot + FVector2D(-30.0, -5.0), 0.f), 90.f);
	TestEqual(TEXT("behind and below"), GSTNHitDataDial::AngleFromPoint(Pivot, Pivot + FVector2D(-30.0, 5.0), 0.f), -90.f);
	TestEqual(TEXT("behind and level is up (a tie goes up)"), GSTNHitDataDial::AngleFromPoint(Pivot, Pivot + FVector2D(-30.0, 0.0), 0.f), 90.f);

	// Snapping: whole degrees, 5 with Shift, 15 with Ctrl
	const FVector2D Odd = Pivot + FVector2D(100.0, -43.0); // 23.27 degrees
	TestEqual(TEXT("whole degrees"), GSTNHitDataDial::AngleFromPoint(Pivot, Odd, GSTNHitDataDial::GetSnapDegrees(false, false)), 23.f);
	TestEqual(TEXT("5 degrees with Shift"), GSTNHitDataDial::AngleFromPoint(Pivot, Odd, GSTNHitDataDial::GetSnapDegrees(true, false)), 25.f);
	TestEqual(TEXT("15 degrees with Ctrl"), GSTNHitDataDial::AngleFromPoint(Pivot, Odd, GSTNHitDataDial::GetSnapDegrees(false, true)), 30.f);
	TestEqual(TEXT("Ctrl wins over Shift"), GSTNHitDataDial::GetSnapDegrees(true, true), 15.f);

	// Whatever the snap, the result is inside the range of AngleDeg
	for (int32 X = -100; X <= 100; X += 20)
	{
		for (int32 Y = -100; Y <= 100; Y += 20)
		{
			const float Angle = GSTNHitDataDial::AngleFromPoint(Pivot, Pivot + FVector2D(X, Y), 15.f);
			TestTrue(TEXT("inside [-90, 90]"), Angle >= -90.f && Angle <= 90.f);
		}
	}

	// The needle's tip and the click that placed it agree, for every whole degree
	bool bRoundTrips = true;
	for (int32 Degrees = -90; Degrees <= 90; ++Degrees)
	{
		const FVector2D Tip = GSTNHitDataDial::PointOnArc(Pivot, 70.0, static_cast<float>(Degrees));
		// +-90 sits exactly above or below the pivot, where the maths and the pixel rounding of the double meet: allow a tenth of a degree
		bRoundTrips &= FMath::IsNearlyEqual(GSTNHitDataDial::AngleFromPoint(Pivot, Tip, 0.f), static_cast<float>(Degrees), 0.1f);
	}
	TestTrue(TEXT("PointOnArc and AngleFromPoint are inverses"), bRoundTrips);

	// The components the caption quotes: unit length, forward then up
	const FVector2D Level = GSTNHitDataDial::PlaneComponents(0.f);
	TestTrue(TEXT("level: all forward"), Level.Equals(FVector2D(1.0, 0.0), 1e-9));
	const FVector2D Up = GSTNHitDataDial::PlaneComponents(90.f);
	TestTrue(TEXT("straight up: no forward"), FMath::IsNearlyZero(Up.X, 1e-9) && FMath::IsNearlyEqual(Up.Y, 1.0, 1e-9));
	const FVector2D Diagonal = GSTNHitDataDial::PlaneComponents(45.f);
	TestTrue(TEXT("45: equal parts"), FMath::IsNearlyEqual(Diagonal.X, Diagonal.Y, 1e-9) && FMath::IsNearlyEqual(Diagonal.Size(), 1.0, 1e-9));
	TestTrue(TEXT("below the plane the up part is negative"), GSTNHitDataDial::PlaneComponents(-30.f).Y < 0.0);
	TestTrue(TEXT("an angle outside the range is clamped, not wrapped"), GSTNHitDataDial::PlaneComponents(200.f).Equals(GSTNHitDataDial::PlaneComponents(90.f), 1e-9));

	// The sentences
	// The numbers follow the editor's language (a comma in Italian), so the caption is read with the decimal point normalised
	const FString Caption = GSTNHitDataText::DescribeAngle(45.f).ToString().Replace(TEXT("0,71"), TEXT("0.71"));
	TestEqual(TEXT("the caption"), Caption, FString(TEXT("45 deg, above the plane: forward 0.71, up 0.71")));
	TestTrue(TEXT("level"), GSTNHitDataText::DescribeAngle(0.f).ToString().Contains(TEXT("level with the plane")));
	TestTrue(TEXT("below"), GSTNHitDataText::DescribeAngle(-20.f).ToString().Contains(TEXT("below the plane")));
	for (const EGSTN_YawSource Source : { EGSTN_YawSource::AttackerFacing, EGSTN_YawSource::HitboxToVictim, EGSTN_YawSource::HitboxVelocity })
	{
		TestFalse(TEXT("every yaw source has a sentence"), GSTNHitDataText::DescribeYawSource(Source).IsEmpty());
	}
	TestFalse(TEXT("different sentences"), GSTNHitDataText::DescribeYawSource(EGSTN_YawSource::AttackerFacing).EqualTo(GSTNHitDataText::DescribeYawSource(EGSTN_YawSource::HitboxVelocity)));
	for (const EGSTN_SpecialAngle Special : { EGSTN_SpecialAngle::None, EGSTN_SpecialAngle::Sakurai3D, EGSTN_SpecialAngle::AutoLink })
	{
		TestFalse(TEXT("every special angle has a sentence"), GSTNHitDataText::DescribeSpecialAngle(Special).IsEmpty());
	}

	FGSTN_HitData Hit = MakeHit();
	TestEqual(TEXT("the summary of a hit with a bracket"), GSTNHitDataText::Summarize(Hit).ToString(), FString(TEXT("Damage 6, knockback 400 + 10/dmg, 45 deg")));
	Hit.FixedKnockback = 500.f;
	TestEqual(TEXT("and of a fixed one"), GSTNHitDataText::Summarize(Hit).ToString(), FString(TEXT("Damage 6, knockback 500 (fixed), 45 deg")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitDataPreviewTest, "GSTNCombat.Editor.HitData.KnockbackPreviewMatchesModel", GSTN_HITDATA_TEST_FLAGS)
bool FGSTN_HitDataPreviewTest::RunTest(const FString& Parameters)
{
	// A hand case on the standard model: K = (base + growth x damage) x WeightReference / weight = (400 + 10 x 6) x 100 / 50 = 920
	const TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules(UGSTN_KnockbackModel_WeightDriven::StaticClass());
	const FGSTN_HitData Hit = MakeHit();
	FGSTN_KnockbackPreviewVictim Victim;
	Victim.Weight = 50.f;
	const FGSTN_KnockbackPreview Preview = GSTNHitDataPreview::Compute(Hit, Rules.Get(), Victim);
	TestTrue(TEXT("the rules have a model"), Preview.bHasModel);
	TestTrue(TEXT("the hand case: 920"), FMath::IsNearlyEqual(Preview.Result.Knockback, 920.0, 1e-6));

	// The same input handed to the model directly gives the same result, field for field: the preview is the model's answer, not a copy of it
	FGSTN_KnockbackInput Input;
	Input.Hit = Hit;
	Input.Up = FVector::UpVector;
	Input.Weight = Victim.Weight;
	Input.DamagePercent = Victim.DamagePercent;
	Input.HealthFraction = 1.f;
	Input.LaunchDirection = UGSTN_HitMathUtils::LaunchDirection(Hit.YawSource, Hit.AngleDeg, Hit.SpecialAngle, Input.Up, FVector::ForwardVector, FVector::ForwardVector, FVector::ZeroVector, true, 0.f, Rules->Sakurai);
	const FGSTN_KnockbackResult Direct = Rules->KnockbackModel->Compute(Input);
	TestEqual(TEXT("knockback"), Preview.Result.Knockback, Direct.Knockback);
	TestEqual(TEXT("tier"), static_cast<int32>(Preview.Result.Reaction), static_cast<int32>(Direct.Reaction));
	TestEqual(TEXT("staggered"), Preview.Result.bStaggered, Direct.bStaggered);
	TestEqual(TEXT("tumble"), Preview.Result.bTumble, Direct.bTumble);
	TestEqual(TEXT("hitstun"), Preview.Result.HitstunFrames, Direct.HitstunFrames);
	TestEqual(TEXT("hitstop"), Preview.Result.HitstopFrames, Direct.HitstopFrames);
	TestEqual(TEXT("attacker hitstop"), Preview.Result.AttackerHitstopFrames, Direct.AttackerHitstopFrames);
	TestTrue(TEXT("launch velocity"), Preview.Result.LaunchVelocity.Equals(Direct.LaunchVelocity, 1e-9));

	// The direction is the elevation above the plane, forward along +X
	TestTrue(TEXT("45 degrees: equal forward and up"), FMath::IsNearlyEqual(Preview.LaunchDirection.X, Preview.LaunchDirection.Z, 1e-9) && FMath::IsNearlyZero(Preview.LaunchDirection.Y, 1e-9));
	TestTrue(TEXT("the effective angle is the authored one"), FMath::IsNearlyEqual(Preview.EffectiveAngleDeg, 45.f, 1e-3f));
	TestTrue(TEXT("the line says what the model said"), Preview.Line.ToString().Contains(TEXT("920")));

	// The victim's weight moves it the way the model says: twice the weight, half the knockback
	Victim.Weight = 100.f;
	TestTrue(TEXT("twice the weight"), FMath::IsNearlyEqual(GSTNHitDataPreview::Compute(Hit, Rules.Get(), Victim).Result.Knockback, 460.0, 1e-6));

	// A fixed knockback replaces the bracket (the weight still applies on this model)
	FGSTN_HitData Fixed = Hit;
	Fixed.FixedKnockback = 300.f;
	TestTrue(TEXT("fixed"), FMath::IsNearlyEqual(GSTNHitDataPreview::Compute(Fixed, Rules.Get(), Victim).Result.Knockback, 300.0, 1e-6));

	// Another model: the percent one reads the percent, the weight driven one does not
	const TStrongObjectPtr<UGSTN_CombatRules> PercentRules = MakeRules(UGSTN_KnockbackModel_PercentScaling::StaticClass());
	FGSTN_KnockbackPreviewVictim Low;
	Low.DamagePercent = 0.f;
	FGSTN_KnockbackPreviewVictim High;
	High.DamagePercent = 150.f;
	TestTrue(TEXT("the percent model sends a damaged victim further"), GSTNHitDataPreview::Compute(Hit, PercentRules.Get(), High).Result.Knockback > GSTNHitDataPreview::Compute(Hit, PercentRules.Get(), Low).Result.Knockback);
	TestEqual(TEXT("the weight driven model ignores the percent"), GSTNHitDataPreview::Compute(Hit, Rules.Get(), High).Result.Knockback, GSTNHitDataPreview::Compute(Hit, Rules.Get(), Low).Result.Knockback);

	// The Fixed model, and the tier: with the default tuning every staggered hit launches (both thresholds 0)
	const TStrongObjectPtr<UGSTN_CombatRules> FixedRules = MakeRules(UGSTN_KnockbackModel_Fixed::StaticClass());
	const FGSTN_KnockbackPreview OnFixed = GSTNHitDataPreview::Compute(Hit, FixedRules.Get(), Victim);
	TestTrue(TEXT("the fixed model gives the base"), FMath::IsNearlyEqual(OnFixed.Result.Knockback, 400.0, 1e-6));
	TestTrue(TEXT("a staggered hit with the default tiers launches"), OnFixed.Result.bStaggered && OnFixed.Result.Reaction == EGSTN_HitReaction::Launch);
	TestTrue(TEXT("and the line names the tier"), OnFixed.Line.ToString().Contains(TEXT("Launch")));

	// The tuning of the model decides the tier: a high launch threshold makes the same hit a push back
	FixedRules->KnockbackModel->Tuning.PushBackThreshold = 0.f;
	FixedRules->KnockbackModel->Tuning.LaunchThreshold = 1000.f;
	TestTrue(TEXT("below the launch threshold: push back"), GSTNHitDataPreview::Compute(Hit, FixedRules.Get(), Victim).Result.Reaction == EGSTN_HitReaction::PushBack);
	TestTrue(TEXT("and the line says so"), GSTNHitDataPreview::Compute(Hit, FixedRules.Get(), Victim).Line.ToString().Contains(TEXT("Push back")));

	// A weak hit under the rules' stagger threshold does not stagger
	FixedRules->StaggerThreshold = 50.f;
	FGSTN_HitData Weak = Hit;
	Weak.Damage = 1.f;
	const FGSTN_KnockbackPreview Resisted = GSTNHitDataPreview::Compute(Weak, FixedRules.Get(), Victim);
	TestFalse(TEXT("under the threshold: no stagger"), Resisted.Result.bStaggered);
	TestTrue(TEXT("the line says it does not stagger"), Resisted.Line.ToString().Contains(TEXT("does not stagger")));

	// Sakurai3D replaces the angle by the rules' low angle on a grounded victim below the knockback threshold, and the high one otherwise
	FGSTN_HitData Sakurai = Hit;
	Sakurai.SpecialAngle = EGSTN_SpecialAngle::Sakurai3D;
	Rules->Sakurai.KnockbackThreshold = 800.f;
	Victim.Weight = 50.f; // K = 920: above the threshold, the high angle
	TestTrue(TEXT("Sakurai above the threshold: the high angle"), FMath::IsNearlyEqual(GSTNHitDataPreview::Compute(Sakurai, Rules.Get(), Victim).EffectiveAngleDeg, 44.f, 1e-2f));
	Victim.Weight = 200.f; // K = 230: below it and grounded, the low angle
	TestTrue(TEXT("Sakurai below the threshold, grounded: the low angle"), FMath::IsNearlyEqual(GSTNHitDataPreview::Compute(Sakurai, Rules.Get(), Victim).EffectiveAngleDeg, 0.f, 1e-2f));
	Victim.bGrounded = false;
	TestTrue(TEXT("in the air: the high angle again"), FMath::IsNearlyEqual(GSTNHitDataPreview::Compute(Sakurai, Rules.Get(), Victim).EffectiveAngleDeg, 44.f, 1e-2f));

	// No model, no rules: a sentence, never a crash
	const TStrongObjectPtr<UGSTN_CombatRules> NoModel = MakeRules(nullptr);
	const FGSTN_KnockbackPreview None = GSTNHitDataPreview::Compute(Hit, NoModel.Get(), Victim);
	TestFalse(TEXT("no model"), None.bHasModel);
	TestFalse(TEXT("but a line"), None.Line.IsEmpty());
	TestFalse(TEXT("no rules"), GSTNHitDataPreview::Compute(Hit, nullptr, Victim).bHasModel);
	TestFalse(TEXT("with its own line"), GSTNHitDataPreview::Compute(Hit, nullptr, Victim).Line.EqualTo(None.Line));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitDataAllFieldsTest, "GSTNCombat.Editor.HitData.ExtensionDataNotHiddenByCustomization", GSTN_HITDATA_TEST_FLAGS)
bool FGSTN_HitDataAllFieldsTest::RunTest(const FString& Parameters)
{
	// The row lays out its fields by name; a field added to FGSTN_HitData and forgotten here would vanish from every hit in the editor
	const TArray<FName> LaidOut = FGSTN_HitDataCustomization::GetLaidOutProperties();
	TSet<FName> Seen;
	for (const FName Name : LaidOut)
	{
		TestFalse(*FString::Printf(TEXT("%s is laid out once"), *Name.ToString()), Seen.Contains(Name));
		Seen.Add(Name);
	}

	TArray<FName> Missing;
	for (TFieldIterator<FProperty> It(FGSTN_HitData::StaticStruct()); It; ++It)
	{
		if (!Seen.Contains(It->GetFName()))
		{
			Missing.Add(It->GetFName());
		}
		else
		{
			Seen.Remove(It->GetFName());
		}
	}
	TestEqual(*FString::Printf(TEXT("every property of FGSTN_HitData is in the row (missing: %s)"), *FString::JoinBy(Missing, TEXT(", "), [](FName Name) { return Name.ToString(); })), Missing.Num(), 0);
	TestEqual(TEXT("and the row names nothing the struct does not have"), Seen.Num(), 0);
	TestTrue(TEXT("the extension data in particular"), LaidOut.Contains(GET_MEMBER_NAME_CHECKED(FGSTN_HitData, ExtensionData)));

	// The extension array keeps its meta (a base struct filter): the customization does not replace the property, it hands it to the standard editor
	const FArrayProperty* Extension = CastField<FArrayProperty>(FGSTN_HitData::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FGSTN_HitData, ExtensionData)));
	TestNotNull(TEXT("the extension data is an array of instanced structs"), Extension);
	TestEqual(TEXT("registered for the hit data struct"), FGSTN_HitDataCustomization::GetStructName(), FName(TEXT("GSTN_HitData")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitDataDetailsViewTest, "GSTNCombat.Editor.HitData.DetailsViewBuildsTheRow", GSTN_HITDATA_TEST_FLAGS)
bool FGSTN_HitDataDetailsViewTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("No Slate application in this run: the details view was not built."));
		return true;
	}

	// A real details view over an object that holds a hit: the module registered the customization, so this builds its header, groups, dial and preview
	TStrongObjectPtr<UGSTN_TestHitHolder> Holder(NewObject<UGSTN_TestHitHolder>(GetTransientPackage()));
	Holder->Hit = MakeHit();
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.bAllowSearch = false;
	const TSharedRef<IDetailsView> View = PropertyEditor.CreateDetailView(Args);
	View->SetObject(Holder.Get());
	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		GSTNVisualSnapshot::TickTree(View.Get(), FPlatformTime::Seconds(), 1.f / 60.f);
	}
	TestTrue(TEXT("the view holds the object"), View->GetSelectedObjects().Num() == 1);
	TestTrue(TEXT("and has widgets"), View->GetChildren() != nullptr && View->GetChildren()->Num() > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
