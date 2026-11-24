// Automation tests of the skin proportion check (owner limits 2026-11-23): reference pose composition, measure, ratios and the limits.
// Pure: bone positions are plain data, so no mesh is loaded.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Kits/GSTN_SkinProportions.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * A stick body with the measured bones, scaled per part: legs 90 cm (thigh at 95, foot at 5), arms 55 cm along Y from a shoulder
	 * 20 cm off the middle (shoulders 40 cm), neck at 150 and the top of the head at 175
	 */
	struct FSPBody
	{
		float Legs = 1.f;
		float Arms = 1.f;
		float Shoulders = 1.f;
		float Head = 1.f;

		TMap<FName, FVector> Positions() const
		{
			const FGSTN_SkinMeasureBones Bones;
			const float HipZ = 5.f + 90.f * Legs;
			const float NeckZ = HipZ + 55.f;
			const float ShoulderY = 20.f * Shoulders;
			TMap<FName, FVector> Out;
			Out.Add(Bones.Foot, FVector(0, 10, 5));
			Out.Add(Bones.Thigh, FVector(0, 10, HipZ));
			Out.Add(Bones.UpperArmLeft, FVector(0, ShoulderY, NeckZ - 10.f));
			Out.Add(Bones.UpperArmRight, FVector(0, -ShoulderY, NeckZ - 10.f));
			Out.Add(Bones.Hand, FVector(0, ShoulderY + 55.f * Arms, NeckZ - 10.f));
			Out.Add(Bones.Neck, FVector(0, 0, NeckZ));
			return Out;
		}

		float TopZ() const { return 5.f + 90.f * Legs + 55.f + 25.f * Head; }

		FGSTN_SkinBodyLengths Measure() const
		{
			return FGSTN_SkinProportionUtils::Measure(Positions(), 0.f, TopZ(), FGSTN_SkinMeasureBones());
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNSkinComposeTest, "GSTNCombat.Core.SkinProportions.Compose", GSTN_CORE_TEST_FLAGS)
bool FGSTNSkinComposeTest::RunTest(const FString& Parameters)
{
	// root -> pelvis (+90 Z) -> thigh (+10 Y, turned 90 degrees on Z) -> calf (+40 X local, which the turn sends to +Y)
	const TArray<FName> Names = { TEXT("root"), TEXT("pelvis"), TEXT("thigh_l"), TEXT("calf_l") };
	const TArray<FTransform> Local = {
		FTransform::Identity,
		FTransform(FVector(0, 0, 90)),
		FTransform(FRotator(0, 90, 0), FVector(0, 10, 0)),
		FTransform(FVector(40, 0, 0)) };
	const TArray<int32> Parents = { -1, 0, 1, 2 };
	const TMap<FName, FVector> Positions = FGSTN_SkinProportionUtils::ComposeComponentSpace(Names, Local, Parents);
	TestTrue(TEXT("Thigh at the pelvis plus its offset"), Positions.FindChecked(TEXT("thigh_l")).Equals(FVector(0, 10, 90), 0.01));
	TestTrue(TEXT("Calf offset turned by the thigh's rotation"), Positions.FindChecked(TEXT("calf_l")).Equals(FVector(0, 50, 90), 0.01));

	// A parent index that is not before the child is treated as a root, never read out of order
	const TMap<FName, FVector> Broken = FGSTN_SkinProportionUtils::ComposeComponentSpace({ TEXT("a"), TEXT("b") }, { FTransform(FVector(1, 0, 0)), FTransform(FVector(2, 0, 0)) }, { 1, -1 });
	TestTrue(TEXT("Forward parent ignored"), Broken.FindChecked(TEXT("a")).Equals(FVector(1, 0, 0), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNSkinMeasureTest, "GSTNCombat.Core.SkinProportions.Measure", GSTN_CORE_TEST_FLAGS)
bool FGSTNSkinMeasureTest::RunTest(const FString& Parameters)
{
	const FGSTN_SkinBodyLengths Base = FSPBody().Measure();
	TestEqual(TEXT("Height from the bounds"), Base.Height, 175.f, 0.01f);
	TestEqual(TEXT("Legs hip to ankle"), Base.Legs, 90.f, 0.01f);
	TestEqual(TEXT("Arms shoulder to wrist"), Base.Arms, 55.f, 0.01f);
	TestEqual(TEXT("Shoulders between the upper arms"), Base.Shoulders, 40.f, 0.01f);
	TestEqual(TEXT("Head neck to top"), Base.Head, 25.f, 0.01f);

	TMap<FName, FVector> NoHand = FSPBody().Positions();
	NoHand.Remove(FGSTN_SkinMeasureBones().Hand);
	const FGSTN_SkinBodyLengths Missing = FGSTN_SkinProportionUtils::Measure(NoHand, 0.f, 175.f, FGSTN_SkinMeasureBones());
	TestEqual(TEXT("A missing bone leaves its length unmeasured"), Missing.Arms, 0.f);
	TestEqual(TEXT("The rest is still measured"), Missing.Legs, 90.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNSkinLimitsTest, "GSTNCombat.Core.SkinProportions.Limits", GSTN_CORE_TEST_FLAGS)
bool FGSTNSkinLimitsTest::RunTest(const FString& Parameters)
{
	const FGSTN_SkinProportionLimits Limits;
	const FGSTN_SkinBodyLengths Base = FSPBody().Measure();
	auto Errors = [&](const FSPBody& Body, TArray<FString>* OutUnmeasured = nullptr)
	{
		TArray<FString> Out;
		TArray<FString> Unmeasured;
		FGSTN_SkinProportionUtils::Check(FGSTN_SkinProportionUtils::Compare(Base, Body.Measure()), Limits, Out, Unmeasured);
		if (OutUnmeasured)
		{
			*OutUnmeasured = Unmeasured;
		}
		return Out;
	};

	TestEqual(TEXT("The base fits itself"), Errors(FSPBody()).Num(), 0);

	// The two owner test bodies: big (Ganondorf-like silhouette) and the kid (Link-like), inside the limits
	FSPBody Big;
	Big.Legs = 1.04f;
	Big.Arms = 1.05f;
	Big.Shoulders = 1.20f;
	Big.Head = 1.0f;
	TestEqual(TEXT("Big body at the shoulder and limb limits fits"), Errors(Big).Num(), 0);
	FSPBody Kid;
	Kid.Legs = 0.96f;
	Kid.Arms = 0.96f;
	Kid.Head = 1.25f;
	TestEqual(TEXT("Kid with legs -4% and head +25% fits"), Errors(Kid).Num(), 0);

	FSPBody LongLegs;
	LongLegs.Legs = 1.06f;
	const TArray<FString> LegErrors = Errors(LongLegs);
	TestEqual(TEXT("Legs +6% fail on legs only"), LegErrors.Num(), 1);
	TestTrue(TEXT("The line names legs"), LegErrors.Num() == 1 && LegErrors[0].StartsWith(TEXT("legs")));

	FSPBody Broad;
	Broad.Shoulders = 1.25f;
	TestEqual(TEXT("Shoulders +25% fail"), Errors(Broad).Num(), 1);

	FSPBody HugeHead;
	HugeHead.Head = 1.4f;
	// The head adds 10 cm (6% of the height): fails on the head, not yet on the height
	TestEqual(TEXT("Head +40% fails on the head"), Errors(HugeHead).Num(), 1);

	FGSTN_SkinBodyLengths NoArms = Base;
	NoArms.Arms = 0.f;
	TArray<FString> Out;
	TArray<FString> Unmeasured;
	FGSTN_SkinProportionUtils::Check(FGSTN_SkinProportionUtils::Compare(Base, NoArms), Limits, Out, Unmeasured);
	TestEqual(TEXT("An unmeasured length is no error"), Out.Num(), 0);
	TestTrue(TEXT("but is reported"), Unmeasured.Contains(TEXT("arms")));
	return true;
}

#endif
