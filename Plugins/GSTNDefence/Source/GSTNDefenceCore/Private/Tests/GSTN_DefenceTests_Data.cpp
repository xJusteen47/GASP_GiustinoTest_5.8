// Automation tests of the defence plugin: rules, curve, kit extension and tracks (plan F3).

#include "Data/GSTN_Curve1D.h"
#include "Data/GSTN_DefenceKitExtension.h"
#include "Data/GSTN_DefenceRules.h"
#include "Data/GSTN_DefenceRulesExtension.h"
#include "Data/GSTN_DefenceTracks.h"
#include "GSTN_DefenceTestUtils.h"
#include "Misc/AutomationTest.h"
#include "StructUtils/InstancedStruct.h"
#include <limits>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceRulesValidTest, "GSTNDefence.Core.Data.RulesValid", GSTNDefenceTest::Flags)
bool FGSTN_DefenceRulesValidTest::RunTest(const FString& Parameters)
{
	UGSTN_DefenceRules* Rules = GSTNDefenceTest::MakeRules();
	TArray<FString> Errors;
	Rules->Validate(Errors);
	TestEqual(TEXT("A fresh rules asset has no error"), Errors.Num(), 0);
	TestEqual(TEXT("A fresh asset holds the shield's profile 0"), Rules->ParryProfiles.Num(), 1);
	TestTrue(TEXT("The shield is enabled by default"), Rules->bShieldEnabled);

	// The fixed bubble: one radius, no value dependence among the fields
	TestTrue(TEXT("The bubble has a positive radius"), Rules->Shield.RadiusCm > 0.f);

#if WITH_EDITOR
	FDataValidationContext Context;
	TestEqual(TEXT("The asset validates"), static_cast<int32>(Rules->IsDataValid(Context)), static_cast<int32>(EDataValidationResult::Valid));
#endif

	// Each broken field is reported on its own
	{
		UGSTN_DefenceRules* Bad = GSTNDefenceTest::MakeRules();
		Bad->Shield.RadiusCm = 0.f;
		TArray<FString> BadErrors;
		Bad->Validate(BadErrors);
		TestTrue(TEXT("A zero radius with the shield enabled is reported"), GSTNDefenceTest::AnyContains(BadErrors, TEXT("radius")));

		Bad->bShieldEnabled = false;
		BadErrors.Reset();
		Bad->Validate(BadErrors);
		TestFalse(TEXT("A zero radius is fine where the mode has no shield"), GSTNDefenceTest::AnyContains(BadErrors, TEXT("is enabled but")));
	}
	{
		UGSTN_DefenceRules* Bad = GSTNDefenceTest::MakeRules();
		Bad->Shield.RecoverFractionAfterBreak = 1.5f;
		Bad->Shield.MaxShieldStun = 1;
		Bad->Shield.ShieldStunBase = 3;
		Bad->GroundBounce.GroundBounceSteerDeg = 120.f;
		Bad->SDI.MinZone = 2.f;
		TArray<FString> BadErrors;
		Bad->Validate(BadErrors);
		TestEqual(TEXT("Four independent problems give four messages"), BadErrors.Num(), 4);
	}
	{
		UGSTN_DefenceRules* Bad = GSTNDefenceTest::MakeRules();
		Bad->DI.StrengthCurve.Points = { GSTNDefenceTest::MakePoint(0, 0), GSTNDefenceTest::MakePoint(0, 100) };
		TArray<FString> BadErrors;
		Bad->Validate(BadErrors);
		TestTrue(TEXT("A curve whose X does not increase is reported"), GSTNDefenceTest::AnyContains(BadErrors, TEXT("strictly increase")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceEmptyProfilesTest, "GSTNDefence.Core.Data.EmptyParryProfilesRejected", GSTNDefenceTest::Flags)
bool FGSTN_DefenceEmptyProfilesTest::RunTest(const FString& Parameters)
{
	UGSTN_DefenceRules* Rules = GSTNDefenceTest::MakeRules();
	Rules->ParryProfiles.Empty();
	TArray<FString> Errors;
	Rules->Validate(Errors);
	TestTrue(TEXT("No profile is reported"), GSTNDefenceTest::AnyContains(Errors, TEXT("no parry profile")));
	TestTrue(TEXT("The shield's parry then names a profile that does not exist"), GSTNDefenceTest::AnyContains(Errors, TEXT("does not exist")));

#if WITH_EDITOR
	FDataValidationContext Context;
	TestEqual(TEXT("The asset is invalid"), static_cast<int32>(Rules->IsDataValid(Context)), static_cast<int32>(EDataValidationResult::Invalid));
#endif

	// A shield parry index past the end is rejected too
	Rules->ParryProfiles.Add(FGSTN_ParryProfile());
	Rules->Parry.ProfileIndex = 1;
	Errors.Reset();
	Rules->Validate(Errors);
	TestTrue(TEXT("A profile index past the end is reported"), GSTNDefenceTest::AnyContains(Errors, TEXT("does not exist")));

	// Too many profiles for the wire index
	Rules->Parry.ProfileIndex = 0;
	Rules->ParryProfiles.Reset();
	for (int32 Index = 0; Index <= GSTNDefenceLimits::MaxParryProfiles; ++Index)
	{
		FGSTN_ParryProfile Profile;
		Profile.Name = Index == 0 ? NAME_None : FName(*FString::Printf(TEXT("P%d"), Index));
		Rules->ParryProfiles.Add(Profile);
	}
	Errors.Reset();
	Rules->Validate(Errors);
	TestTrue(TEXT("More profiles than the limit is reported"), GSTNDefenceTest::AnyContains(Errors, TEXT("the limit is")));

	// Names: only profile 0 may be unnamed, names are unique
	Rules->ParryProfiles.Reset();
	Rules->ParryProfiles.Add(FGSTN_ParryProfile());
	FGSTN_ParryProfile Unnamed;
	Rules->ParryProfiles.Add(Unnamed);
	FGSTN_ParryProfile Counter;
	Counter.Name = TEXT("Counter");
	Rules->ParryProfiles.Add(Counter);
	Rules->ParryProfiles.Add(Counter);
	Errors.Reset();
	Rules->Validate(Errors);
	TestTrue(TEXT("An unnamed profile other than 0 is reported"), GSTNDefenceTest::AnyContains(Errors, TEXT("has no name")));
	TestTrue(TEXT("A duplicate name is reported"), GSTNDefenceTest::AnyContains(Errors, TEXT("must be unique")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceProfileNameBakesTest, "GSTNDefence.Core.Data.ProfileNameBakes", GSTNDefenceTest::Flags)
bool FGSTN_DefenceProfileNameBakesTest::RunTest(const FString& Parameters)
{
	UGSTN_DefenceRules* Rules = GSTNDefenceTest::MakeRules();
	FGSTN_ParryProfile Counter;
	Counter.Name = TEXT("Counter");
	FGSTN_ParryProfile Riposte;
	Riposte.Name = TEXT("Riposte");
	Rules->ParryProfiles.Add(Counter);
	Rules->ParryProfiles.Add(Riposte);

	TestEqual(TEXT("An empty name is profile 0"), Rules->FindProfileIndex(NAME_None), 0);
	TestEqual(TEXT("Counter is profile 1"), Rules->FindProfileIndex(TEXT("Counter")), 1);
	TestEqual(TEXT("Riposte is profile 2"), Rules->FindProfileIndex(TEXT("Riposte")), 2);
	TestEqual(TEXT("An unknown name is not silently profile 0"), Rules->FindProfileIndex(TEXT("Nope")), static_cast<int32>(INDEX_NONE));

	FGSTN_ParryWindowTrack Track;
	TestEqual(TEXT("A track is not baked at construction"), Track.ProfileIndex, static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("An empty name bakes"), Track.Bake(*Rules));
	TestEqual(TEXT("An empty name bakes to 0"), Track.ProfileIndex, 0);

	Track.ProfileName = TEXT("Riposte");
	TestTrue(TEXT("A known name bakes"), Track.Bake(*Rules));
	TestEqual(TEXT("Riposte bakes to 2"), Track.ProfileIndex, 2);

	Track.ProfileName = TEXT("Nope");
	TestFalse(TEXT("An unknown name does not bake"), Track.Bake(*Rules));
	TestEqual(TEXT("An unknown name leaves no index"), Track.ProfileIndex, static_cast<int32>(INDEX_NONE));

	// Baking is the same on every call: it reads only the rules
	Track.ProfileName = TEXT("Counter");
	Track.Bake(*Rules);
	const int32 First = Track.ProfileIndex;
	Track.Bake(*Rules);
	TestEqual(TEXT("Baking twice gives the same index"), Track.ProfileIndex, First);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceCurve1DTest, "GSTNDefence.Core.Data.Curve1DMonotonicAndQuantized", GSTNDefenceTest::Flags)
bool FGSTN_DefenceCurve1DTest::RunTest(const FString& Parameters)
{
	// Hand values on a line from 0 to 1: the halves of the fixed point are exact
	const FGSTN_Curve1D Line = FGSTN_Curve1D::MakeLinear(0.0, 1.0);
	TestEqual(TEXT("At 0"), Line.EvaluateFixed(0.0), 0);
	TestEqual(TEXT("At 0.5"), Line.EvaluateFixed(0.5), 8192);
	TestEqual(TEXT("At 0.25"), Line.EvaluateFixed(0.25), 4096);
	TestEqual(TEXT("At 1"), Line.EvaluateFixed(1.0), 16384);
	TestEqual(TEXT("Below 0 holds the first value"), Line.EvaluateFixed(-3.0), 0);
	TestEqual(TEXT("Above 1 holds the last value"), Line.EvaluateFixed(7.0), 16384);
	TestEqual(TEXT("A NaN input holds the first value"), Line.EvaluateFixed(std::numeric_limits<double>::quiet_NaN()), 0);

	// The input is quantised to 1/4096: 0.49 of a step rounds down, 0.51 rounds up, and one step is 4 units of Y
	TestEqual(TEXT("0.49 of a step reads as 0"), Line.EvaluateFixed(0.49 / 4096.0), 0);
	TestEqual(TEXT("0.51 of a step reads as one step"), Line.EvaluateFixed(0.51 / 4096.0), 4);
	TestEqual(TEXT("QuantiseX rounds half up"), FGSTN_Curve1D::QuantiseX(0.5 / 4096.0), 1);

	// A three-point curve: 0 -> 1 -> 0.5, a falling segment rounds like a rising one
	FGSTN_Curve1D Hill;
	Hill.AddPoint(0.0, 0.0);
	Hill.AddPoint(0.5, 1.0);
	Hill.AddPoint(1.0, 0.5);
	TestEqual(TEXT("The peak"), Hill.EvaluateFixed(0.5), 16384);
	TestEqual(TEXT("Halfway down the falling segment"), Hill.EvaluateFixed(0.75), 12288);
	TestEqual(TEXT("At the end"), Hill.EvaluateFixed(1.0), 8192);
	TestFalse(TEXT("A curve that falls is not monotonic"), Hill.IsMonotonicNonDecreasing());

	// Monotonic curve: the output never decreases over a fine sweep, and every output is a multiple of 1/16384
	FGSTN_Curve1D Rising;
	Rising.AddPoint(0.0, 0.0);
	Rising.AddPoint(0.3, 0.2);
	Rising.AddPoint(0.8, 0.9);
	Rising.AddPoint(1.0, 1.0);
	TestTrue(TEXT("The authored curve is monotonic"), Rising.IsMonotonicNonDecreasing());
	int32 Previous = -1;
	bool bMonotonic = true;
	bool bQuantised = true;
	for (int32 Step = 0; Step <= 2000; ++Step)
	{
		const double X = static_cast<double>(Step) / 2000.0;
		const int32 Fixed = Rising.EvaluateFixed(X);
		bMonotonic &= Fixed >= Previous;
		Previous = Fixed;
		const double AsDouble = Rising.Evaluate(X);
		bQuantised &= FMath::IsNearlyEqual(AsDouble * FGSTN_Curve1D::YOne, static_cast<double>(Fixed), 1e-9);
	}
	TestTrue(TEXT("The output never decreases"), bMonotonic);
	TestTrue(TEXT("Every output is a multiple of 1/16384"), bQuantised);

	// Determinism: the same input gives the same bits in two runs
	TestEqual(TEXT("Two evaluations agree"), Rising.EvaluateFixed(0.4375), Rising.EvaluateFixed(0.4375));

	// Not authored = identity
	const FGSTN_Curve1D Empty;
	TestFalse(TEXT("An empty curve is not authored"), Empty.IsAuthored());
	TestEqual(TEXT("An empty curve is the identity"), Empty.EvaluateFixed(0.3), FGSTN_Curve1D::YOne);
	TestFalse(TEXT("An empty curve is not monotonic data"), Empty.IsMonotonicNonDecreasing());

	// Validation
	TArray<FString> Errors;
	Rising.Validate(TEXT("curve"), Errors);
	TestEqual(TEXT("A good curve has no error"), Errors.Num(), 0);
	FGSTN_Curve1D Bad;
	Bad.Points = { GSTNDefenceTest::MakePoint(100, 10), GSTNDefenceTest::MakePoint(100, 20), GSTNDefenceTest::MakePoint(5000, 70000) };
	Bad.Validate(TEXT("curve"), Errors);
	TestEqual(TEXT("A repeated X, an X past 4096 and a Y past 65535 give three messages"), Errors.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceKitNoShieldTest, "GSTNDefence.Core.Data.KitExtensionNoShieldMeansNone", GSTNDefenceTest::Flags)
bool FGSTN_DefenceKitNoShieldTest::RunTest(const FString& Parameters)
{
	// An entry with nothing set: no shield, no tech, no rules
	const FGSTN_DefenceKitExtension Empty;
	TestFalse(TEXT("An empty entry has no shield"), Empty.HasShield());
	TestTrue(TEXT("An empty entry has no tech"), Empty.Tech.IsEmpty());
	TestTrue(TEXT("An empty entry has no rules of its own"), Empty.Rules.IsNull());
	TestTrue(TEXT("An empty entry has no get-up attacks"), Empty.GetupSet.Attacks.IsEmpty());

	// A kit with no defence entry at all
	FGSTN_KitExtensionList NoEntries;
	TestNull(TEXT("A kit without the entry finds none"), NoEntries.Find<FGSTN_DefenceKitExtension>());

	// A kit with the entry but no shield ability is still a kit without a shield; a ShieldAbility makes it one
	FGSTN_KitExtensionList List;
	List.ExtensionData.Add(FInstancedStruct::Make(FGSTN_DefenceKitExtension()));
	const FGSTN_DefenceKitExtension* Found = List.Find<FGSTN_DefenceKitExtension>();
	TestNotNull(TEXT("The entry is found"), Found);
	if (Found)
	{
		TestFalse(TEXT("The found entry has no shield"), Found->HasShield());
	}

	FGSTN_DefenceKitExtension WithShield;
	WithShield.ShieldAbility = TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/Script/Engine.Actor")));
	TestTrue(TEXT("A shield ability makes the kit a shield kit"), WithShield.HasShield());

	FGSTN_DefenceKitExtension WithTech;
	WithTech.Tech.Abilities.Add(EGSTN_TechType::InPlace, TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/Script/Engine.Actor"))));
	TestFalse(TEXT("A tech set does not give a shield"), WithTech.HasShield());
	TestFalse(TEXT("A tech set with an ability is not empty"), WithTech.Tech.IsEmpty());

	// The rules extension is data only and empty by default
	const FGSTN_DefenceRulesExtension RulesExtension;
	TestTrue(TEXT("The rules extension names no asset by default"), RulesExtension.Rules.IsNull());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceTracksValidatedTest, "GSTNDefence.Core.Data.TrackWindowsValidated", GSTNDefenceTest::Flags)
bool FGSTN_DefenceTracksValidatedTest::RunTest(const FString& Parameters)
{
	UGSTN_DefenceRules* Rules = GSTNDefenceTest::MakeRules();

	// Hold window
	FGSTN_HoldWindowTrack Hold;
	Hold.StartFrame = 1;
	Hold.EndFrame = 200;
	Hold.HoldButton = FGameplayTag::RequestGameplayTag(TEXT("Input.Button.Shield"), false);
	TArray<FString> Errors;
	Hold.Validate(TEXT("hold"), Errors);
	TestEqual(TEXT("A hold window on the shield button is valid"), Errors.Num(), Hold.HoldButton.IsValid() ? 0 : 1);
	TestTrue(TEXT("The window contains its first frame"), Hold.Contains(1));
	TestFalse(TEXT("The window excludes its end frame"), Hold.Contains(200));

	FGSTN_HoldWindowTrack BadHold;
	BadHold.StartFrame = 5;
	BadHold.EndFrame = 5;
	Errors.Reset();
	BadHold.Validate(TEXT("hold"), Errors);
	TestTrue(TEXT("An empty window is reported"), GSTNDefenceTest::AnyContains(Errors, TEXT("empty or inverted")));
	TestTrue(TEXT("A missing button is reported"), GSTNDefenceTest::AnyContains(Errors, TEXT("no hold button")));

	// Parry window
	FGSTN_ParryWindowTrack Parry;
	Parry.StartFrame = 10;
	Parry.EndFrame = 20;
	Errors.Reset();
	Parry.Validate(TEXT("parry"), Errors);
	TestTrue(TEXT("A parry window that is not baked is reported"), GSTNDefenceTest::AnyContains(Errors, TEXT("not baked")));

	TestTrue(TEXT("The window bakes against the rules"), Parry.Bake(*Rules));
	Errors.Reset();
	Parry.Validate(TEXT("parry"), Errors);
	TestEqual(TEXT("A baked window is valid"), Errors.Num(), 0);

	FGSTN_ParryWindowTrack Inverted = Parry;
	Inverted.StartFrame = 30;
	Inverted.EndFrame = 20;
	Errors.Reset();
	Inverted.Validate(TEXT("parry"), Errors);
	TestTrue(TEXT("An inverted window is reported"), GSTNDefenceTest::AnyContains(Errors, TEXT("empty or inverted")));

	FGSTN_ParryWindowTrack UnknownProfile = Parry;
	UnknownProfile.ProfileName = TEXT("Nope");
	UnknownProfile.Bake(*Rules);
	Errors.Reset();
	UnknownProfile.Validate(TEXT("parry"), Errors);
	TestTrue(TEXT("An unknown profile name is reported"), GSTNDefenceTest::AnyContains(Errors, TEXT("'Nope'")));

	FGSTN_ParryWindowTrack EmptyName = Parry;
	EmptyName.AffectedHurtboxes = { NAME_None };
	Errors.Reset();
	EmptyName.Validate(TEXT("parry"), Errors);
	TestTrue(TEXT("An empty hurtbox name is reported"), GSTNDefenceTest::AnyContains(Errors, TEXT("empty hurtbox name")));

	// Hurtbox selection: empty = any, otherwise a name or a tag
	const FGameplayTag BladeTag = FGameplayTag::RequestGameplayTag(TEXT("Combat.Hurtbox.Head"), false);
	FGameplayTagContainer Tags;
	if (BladeTag.IsValid())
	{
		Tags.AddTag(BladeTag);
	}
	TestTrue(TEXT("An empty selector covers any hurtbox"), Parry.Affects(TEXT("Anything"), Tags));
	Parry.AffectedHurtboxes = { TEXT("Blade") };
	TestTrue(TEXT("A named hurtbox is covered"), Parry.Affects(TEXT("Blade"), FGameplayTagContainer()));
	TestFalse(TEXT("Another hurtbox is not"), Parry.Affects(TEXT("Torso"), FGameplayTagContainer()));
	if (BladeTag.IsValid())
	{
		Parry.AffectedTags.AddTag(BladeTag);
		TestTrue(TEXT("A tag match is covered, in union with the names"), Parry.Affects(TEXT("Torso"), Tags));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
