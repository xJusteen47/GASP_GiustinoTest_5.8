// Automation tests of the defence plugin: DI by the rules asset, SDI drift and the landing bounce (plan F4).

#include "Data/GSTN_DefenceRules.h"
#include "GSTN_DefenceTags.h"
#include "GSTN_DefenceTestUtils.h"
#include "Hits/GSTN_HitMathUtils.h"
#include "Math/GSTN_DIRulesMath.h"
#include "Math/GSTN_DefenceMath.h"
#include "Math/GSTN_SDIMath.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FVector DITestUp(0.0, 0.0, 1.0);
	const FVector DITestYaw(1.0, 0.0, 0.0);

	/** A launch at 45 degrees along +X, 1000 cm/s */
	FVector DITestLaunch()
	{
		return FVector(1.0, 0.0, 1.0).GetSafeNormal() * 1000.0;
	}

	bool DITestNear(const FVector& A, const FVector& B)
	{
		return A.Equals(B, 1e-9);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceDICurveTest, "GSTNDefence.Core.DI.CurveAndVictimScale", GSTNDefenceTest::Flags)
bool FGSTN_DefenceDICurveTest::RunTest(const FString& Parameters)
{
	FGSTN_DIRules Rules;
	const FVector Launch = DITestLaunch();
	const FVector Stick(0.0, 0.8, 0.0);
	const FGameplayTagContainer NoTags;

	// The helpers: an empty curve is the identity, a line returns its input
	TestEqual(TEXT("An empty strength curve is 1"), UGSTN_DIRulesMath::StrengthCurve(Rules, 0.3), 1.0);
	TestEqual(TEXT("An empty elevation curve is 1"), UGSTN_DIRulesMath::ElevationDeltaByElevation(Rules, 0.7), 1.0);
	Rules.StrengthCurve = FGSTN_Curve1D::MakeLinear(0.0, 1.0);
	TestEqual(TEXT("A line from 0 to 1 returns the magnitude"), UGSTN_DIRulesMath::StrengthCurve(Rules, 0.5), 0.5);

	// The stick is rescaled to |stick| x curve(|stick|) along its own direction. The curve quantises its input to 1/4096, so the
	// factor at 0.8 is read from the curve (0.800048828125) rather than assumed to be 0.8
	const double Factor = UGSTN_DIRulesMath::StrengthCurve(Rules, 0.8);
	TestTrue(TEXT("The curve reads 0.8 as 3277/4096"), FMath::IsNearlyEqual(Factor, 0.800048828125, 1e-12));
	const FVector WithCurve = UGSTN_DIRulesMath::ApplyDI(Launch, Stick, DITestUp, DITestYaw, 1000.0, NoTags, Rules, false);
	TestTrue(TEXT("The curve scales the stick like the M4 function with the rescaled stick"),
		DITestNear(WithCurve, UGSTN_HitMathUtils::ApplyDI(Launch, FVector(0.0, 0.8 * Factor, 0.0), DITestUp, DITestYaw, Rules.Params)));
	TestFalse(TEXT("and the launch did turn"), DITestNear(WithCurve, Launch));
	TestTrue(TEXT("with the speed unchanged"), FMath::IsNearlyEqual(WithCurve.Size(), 1000.0, 1e-6));

	// The victim scale multiplies in: a half-scale tag
	const FGameplayTag HalfTag = GSTNDefenceTags::Immunity_Shield;
	Rules.ScaleByVictimTag.Add(HalfTag, 0.5f);
	FGameplayTagContainer Half;
	Half.AddTag(HalfTag);
	const FVector Scaled = UGSTN_DIRulesMath::ApplyDI(Launch, Stick, DITestUp, DITestYaw, 1000.0, Half, Rules, false);
	TestTrue(TEXT("A victim tag of 0.5 halves the strength"),
		DITestNear(Scaled, UGSTN_HitMathUtils::ApplyDI(Launch, FVector(0.0, 0.8 * Factor * 0.5, 0.0), DITestUp, DITestYaw, Rules.Params)));
	TestTrue(TEXT("A victim without the tag is unscaled"), DITestNear(UGSTN_DIRulesMath::ApplyDI(Launch, Stick, DITestUp, DITestYaw, 1000.0, NoTags, Rules, false), WithCurve));

	// Bosses: a scale of 0 returns the launch exactly
	Rules.ScaleByVictimTag.Add(HalfTag, 0.f);
	TestTrue(TEXT("A victim scale of 0 leaves the launch alone"), DITestNear(UGSTN_DIRulesMath::ApplyDI(Launch, Stick, DITestUp, DITestYaw, 1000.0, Half, Rules, false), Launch));

	// The product of every matching tag
	TMap<FGameplayTag, float> Scales;
	Scales.Add(GSTNDefenceTags::Immunity_Shield, 0.5f);
	Scales.Add(GSTNDefenceTags::Immunity_Parry, 0.25f);
	FGameplayTagContainer Both;
	Both.AddTag(GSTNDefenceTags::Immunity_Shield);
	Both.AddTag(GSTNDefenceTags::Immunity_Parry);
	TestEqual(TEXT("Two matching tags multiply"), UGSTN_DIRulesMath::ScaleByVictimTag(Scales, Both), 0.125);
	TestEqual(TEXT("No matching tag is 1"), UGSTN_DIRulesMath::ScaleByVictimTag(Scales, NoTags), 1.0);

	// The elevation curve scales MaxDIElevationDot: a flat 0 curve takes the elevation part away, leaving the M4 result with that parameter at 0
	FGSTN_DIRules Flat;
	Flat.ElevationDeltaByElevation.Points = { GSTNDefenceTest::MakePoint(0, 0), GSTNDefenceTest::MakePoint(4096, 0) };
	const FVector AlongStick(0.6, 0.6, 0.0);
	FGSTN_DIParams NoElevation = Flat.Params;
	NoElevation.MaxDIElevationDot = 0.f;
	TestTrue(TEXT("A zero elevation curve equals M4 with MaxDIElevationDot 0"),
		DITestNear(UGSTN_DIRulesMath::ApplyDI(Launch, AlongStick, DITestUp, DITestYaw, 1000.0, NoTags, Flat, false), UGSTN_HitMathUtils::ApplyDI(Launch, AlongStick, DITestUp, DITestYaw, NoElevation)));
	FGSTN_DIRules Plain;
	TestTrue(TEXT("and an empty curve equals M4 with the rules' parameters"),
		DITestNear(UGSTN_DIRulesMath::ApplyDI(Launch, AlongStick, DITestUp, DITestYaw, 1000.0, NoTags, Plain, false), UGSTN_HitMathUtils::ApplyDI(Launch, AlongStick, DITestUp, DITestYaw, Plain.Params)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceDIMinKnockbackTest, "GSTNDefence.Core.DI.MinKnockback", GSTNDefenceTest::Flags)
bool FGSTN_DefenceDIMinKnockbackTest::RunTest(const FString& Parameters)
{
	FGSTN_DIRules Rules;
	Rules.MinKnockbackForDI = 500.f;
	const FVector Launch = DITestLaunch();
	const FVector Stick(0.0, 1.0, 0.0);
	const FGameplayTagContainer NoTags;

	TestTrue(TEXT("Knockback 499: no DI"), DITestNear(UGSTN_DIRulesMath::ApplyDI(Launch, Stick, DITestUp, DITestYaw, 499.0, NoTags, Rules, false), Launch));
	TestFalse(TEXT("Knockback 500: DI applies"), DITestNear(UGSTN_DIRulesMath::ApplyDI(Launch, Stick, DITestUp, DITestYaw, 500.0, NoTags, Rules, false), Launch));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceDIDisableTest, "GSTNDefence.Core.DI.DisableFlag", GSTNDefenceTest::Flags)
bool FGSTN_DefenceDIDisableTest::RunTest(const FString& Parameters)
{
	FGSTN_DIRules Rules;
	const FVector Launch = DITestLaunch();
	const FGameplayTagContainer NoTags;

	TestTrue(TEXT("A hit that disables DI keeps the launch"), DITestNear(UGSTN_DIRulesMath::ApplyDI(Launch, FVector(0.0, 1.0, 0.0), DITestUp, DITestYaw, 1000.0, NoTags, Rules, true), Launch));
	TestFalse(TEXT("The same hit without the flag turns"), DITestNear(UGSTN_DIRulesMath::ApplyDI(Launch, FVector(0.0, 1.0, 0.0), DITestUp, DITestYaw, 1000.0, NoTags, Rules, false), Launch));
	TestTrue(TEXT("A zero stick keeps the launch"), DITestNear(UGSTN_DIRulesMath::ApplyDI(Launch, FVector::ZeroVector, DITestUp, DITestYaw, 1000.0, NoTags, Rules, false), Launch));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceSDIDriftTest, "GSTNDefence.Core.SDI.DriftUntilMax", GSTNDefenceTest::Flags)
bool FGSTN_DefenceSDIDriftTest::RunTest(const FString& Parameters)
{
	// 120 cm/s at the full stick, 0.016 s per frame = 1.92 cm per frame; the maximum is 12 cm
	FGSTN_SDIRules Rules;
	Rules.DriftSpeedCmPerSec = 120.f;
	Rules.MaxDistanceCmPerHit = 12.f;
	Rules.MinZone = 0.3f;
	const FGameplayTagContainer NoTags;
	const FVector Stick(1.0, 0.0, 0.0);

	double Accumulated = 0.0;
	for (int32 Frame = 1; Frame <= 6; ++Frame)
	{
		const FGSTN_SDIStep Step = UGSTN_SDIMath::NextDisplacement(Rules, Stick, Accumulated, false, false, false, NoTags, 20);
		TestFalse(TEXT("Not blocked"), Step.bBlocked);
		TestTrue(FString::Printf(TEXT("Frame %d adds 1.92 cm"), Frame), FMath::IsNearlyEqual(Step.DistanceAdded, 1.92, 1e-9));
		TestTrue(TEXT("at 120 cm/s along the stick"), Step.DriftVelocity.Equals(FVector(120.0, 0.0, 0.0), 1e-9));
		Accumulated += Step.DistanceAdded;
	}
	TestTrue(TEXT("Six frames: 11.52 cm"), FMath::IsNearlyEqual(Accumulated, 11.52, 1e-9));

	// The seventh frame drifts only the 0.48 cm that is left, at 30 cm/s
	const FGSTN_SDIStep Last = UGSTN_SDIMath::NextDisplacement(Rules, Stick, Accumulated, false, false, false, NoTags, 20);
	TestTrue(TEXT("The last frame adds the remainder 0.48"), FMath::IsNearlyEqual(Last.DistanceAdded, 0.48, 1e-9));
	TestTrue(TEXT("at 30 cm/s"), Last.DriftVelocity.Equals(FVector(30.0, 0.0, 0.0), 1e-9));
	Accumulated += Last.DistanceAdded;
	TestTrue(TEXT("The total is exactly the maximum"), FMath::IsNearlyEqual(Accumulated, 12.0, 1e-9));

	const FGSTN_SDIStep Done = UGSTN_SDIMath::NextDisplacement(Rules, Stick, Accumulated, false, false, false, NoTags, 20);
	TestTrue(TEXT("Nothing drifts past the maximum"), Done.DriftVelocity.IsNearlyZero() && Done.DistanceAdded == 0.0);
	TestFalse(TEXT("and it is not a block"), Done.bBlocked);

	// Half the stick drifts at half the speed (the stick's own magnitude scales it)
	const FGSTN_SDIStep Half = UGSTN_SDIMath::NextDisplacement(Rules, FVector(0.5, 0.0, 0.0), 0.0, false, false, false, NoTags, 20);
	TestTrue(TEXT("Half the stick: 60 cm/s"), Half.DriftVelocity.Equals(FVector(60.0, 0.0, 0.0), 1e-9));
	TestTrue(TEXT("0.96 cm per frame"), FMath::IsNearlyEqual(Half.DistanceAdded, 0.96, 1e-9));

	// Spread over the hitstop: 12 frames cap the speed at 12 / (12 x 0.016) = 62.5 cm/s = exactly 1 cm per frame, 12 cm in total
	Rules.bScaleByHitstopFrames = true;
	double Spread = 0.0;
	for (int32 Frame = 1; Frame <= 12; ++Frame)
	{
		const FGSTN_SDIStep Step = UGSTN_SDIMath::NextDisplacement(Rules, Stick, Spread, false, false, false, NoTags, 12);
		TestTrue(TEXT("One centimetre per frame"), FMath::IsNearlyEqual(Step.DistanceAdded, 1.0, 1e-9));
		Spread += Step.DistanceAdded;
	}
	TestTrue(TEXT("12 frames reach the maximum"), FMath::IsNearlyEqual(Spread, 12.0, 1e-9));

	// A victim scale of 0 (a boss) drifts nothing and is not a block
	Rules.bScaleByHitstopFrames = false;
	Rules.ScaleByVictimTag.Add(GSTNDefenceTags::Immunity_Shield, 0.f);
	FGameplayTagContainer Boss;
	Boss.AddTag(GSTNDefenceTags::Immunity_Shield);
	const FGSTN_SDIStep BossStep = UGSTN_SDIMath::NextDisplacement(Rules, Stick, 0.0, false, false, false, Boss, 20);
	TestTrue(TEXT("A boss drifts nothing"), BossStep.DriftVelocity.IsNearlyZero());
	TestFalse(TEXT("without being blocked"), BossStep.bBlocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceSDIMinZoneTest, "GSTNDefence.Core.SDI.MinZone", GSTNDefenceTest::Flags)
bool FGSTN_DefenceSDIMinZoneTest::RunTest(const FString& Parameters)
{
	FGSTN_SDIRules Rules;
	Rules.MinZone = 0.3f;
	const FGameplayTagContainer NoTags;

	const FGSTN_SDIStep Below = UGSTN_SDIMath::NextDisplacement(Rules, FVector(0.29, 0.0, 0.0), 0.0, false, false, false, NoTags, 20);
	TestTrue(TEXT("A tilt below the zone drifts nothing"), Below.DriftVelocity.IsNearlyZero() && Below.DistanceAdded == 0.0);
	TestFalse(TEXT("and is not a block"), Below.bBlocked);

	const FGSTN_SDIStep Above = UGSTN_SDIMath::NextDisplacement(Rules, FVector(0.31, 0.0, 0.0), 0.0, false, false, false, NoTags, 20);
	TestFalse(TEXT("A tilt above the zone drifts"), Above.DriftVelocity.IsNearlyZero());

	const FGSTN_SDIStep Neutral = UGSTN_SDIMath::NextDisplacement(Rules, FVector::ZeroVector, 0.0, false, false, false, NoTags, 20);
	TestTrue(TEXT("A neutral stick drifts nothing"), Neutral.DriftVelocity.IsNearlyZero());

	// The curve shapes the speed: a line from 0 to 1 squares the stick magnitude
	Rules.StrengthCurve = FGSTN_Curve1D::MakeLinear(0.0, 1.0);
	const FGSTN_SDIStep Curved = UGSTN_SDIMath::NextDisplacement(Rules, FVector(0.5, 0.0, 0.0), 0.0, false, false, false, NoTags, 20);
	// 0.5 x curve(0.5) = 0.25 of the full speed of 120 cm/s
	TestTrue(TEXT("A linear curve squares the half stick: 0.25 x 120"), Curved.DriftVelocity.Equals(FVector(30.0, 0.0, 0.0), 1e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceSDIArmoredTest, "GSTNDefence.Core.SDI.ArmoredBlocked", GSTNDefenceTest::Flags)
bool FGSTN_DefenceSDIArmoredTest::RunTest(const FString& Parameters)
{
	FGSTN_SDIRules Rules;
	const FGameplayTagContainer NoTags;
	const FVector Stick(1.0, 0.0, 0.0);

	// Armored: blocked by default, allowed by the rules
	FGSTN_SDIStep Step = UGSTN_SDIMath::NextDisplacement(Rules, Stick, 0.0, true, false, false, NoTags, 20);
	TestTrue(TEXT("An armored victim is blocked by default"), Step.bBlocked);
	TestTrue(TEXT("with no drift"), Step.DriftVelocity.IsNearlyZero());
	Rules.bSDIAllowedWhenArmored = true;
	Step = UGSTN_SDIMath::NextDisplacement(Rules, Stick, 0.0, true, false, false, NoTags, 20);
	TestFalse(TEXT("The rules may allow it"), Step.bBlocked);
	TestFalse(TEXT("and it drifts"), Step.DriftVelocity.IsNearlyZero());

	// Throws always block
	Step = UGSTN_SDIMath::NextDisplacement(Rules, Stick, 0.0, false, false, true, NoTags, 20);
	TestTrue(TEXT("A throw blocks SDI"), Step.bBlocked);

	// The stagger-in-place tier drifts by default, and the rules may forbid it
	Step = UGSTN_SDIMath::NextDisplacement(Rules, Stick, 0.0, false, true, false, NoTags, 20);
	TestFalse(TEXT("Stagger in place drifts by default"), Step.bBlocked);
	Rules.bSDIInStaggerInPlace = false;
	Step = UGSTN_SDIMath::NextDisplacement(Rules, Stick, 0.0, false, true, false, NoTags, 20);
	TestTrue(TEXT("and is blocked when the rules say so"), Step.bBlocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceBounceSteerTest, "GSTNDefence.Core.Bounce.SteerFromStick", GSTNDefenceTest::Flags)
bool FGSTN_DefenceBounceSteerTest::RunTest(const FString& Parameters)
{
	// Speed 300, steering 30 degrees at the full stick. sin 30 = 0.5, cos 30 = 0.8660254037844386; sin 15 = 0.25881904510252074,
	// cos 15 = 0.9659258262890683 (hand values)
	FGSTN_GroundBounceRules Rules;
	Rules.GroundBounceSpeed = 300.f;
	Rules.GroundBounceSteerDeg = 30.f;
	const FVector Up(0.0, 0.0, 1.0);

	TestTrue(TEXT("A neutral stick bounces straight up"), UGSTN_DefenceMath::GroundBounceVelocity(Rules, Up, FVector::ZeroVector).Equals(FVector(0.0, 0.0, 300.0), 1e-9));
	TestTrue(TEXT("The full stick tilts 30 degrees toward it"), UGSTN_DefenceMath::GroundBounceVelocity(Rules, Up, FVector(1.0, 0.0, 0.0)).Equals(FVector(150.0, 0.0, 259.8076211353316), 1e-6));
	TestTrue(TEXT("Half the stick tilts 15 degrees"), UGSTN_DefenceMath::GroundBounceVelocity(Rules, Up, FVector(0.5, 0.0, 0.0)).Equals(FVector(77.64571353075622, 0.0, 289.7777478867205), 1e-6));
	TestTrue(TEXT("The other way tilts the other way"), UGSTN_DefenceMath::GroundBounceVelocity(Rules, Up, FVector(0.0, -1.0, 0.0)).Equals(FVector(0.0, -150.0, 259.8076211353316), 1e-6));
	TestTrue(TEXT("A stick beyond 1 counts as the full stick"), UGSTN_DefenceMath::GroundBounceVelocity(Rules, Up, FVector(2.0, 0.0, 0.0)).Equals(FVector(150.0, 0.0, 259.8076211353316), 1e-6));
	TestTrue(TEXT("The part of the stick along up does not steer"), UGSTN_DefenceMath::GroundBounceVelocity(Rules, Up, FVector(1.0, 0.0, 5.0)).Equals(FVector(150.0, 0.0, 259.8076211353316), 1e-6));

	// The speed is the same whichever way the stick points
	TestTrue(TEXT("The bounce keeps its speed"), FMath::IsNearlyEqual(UGSTN_DefenceMath::GroundBounceVelocity(Rules, Up, FVector(0.6, 0.4, 0.0)).Size(), 300.0, 1e-9));

	// Any gravity: up along +X, the stick on the YZ plane
	TestTrue(TEXT("Up along +X"), UGSTN_DefenceMath::GroundBounceVelocity(Rules, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0)).Equals(FVector(259.8076211353316, 150.0, 0.0), 1e-6));
	TestTrue(TEXT("A zero up falls back to +Z"), UGSTN_DefenceMath::GroundBounceVelocity(Rules, FVector::ZeroVector, FVector::ZeroVector).Equals(FVector(0.0, 0.0, 300.0), 1e-9));

	// Determinism
	TestTrue(TEXT("Two calls agree bit for bit"), UGSTN_DefenceMath::GroundBounceVelocity(Rules, Up, FVector(0.3, 0.7, 0.0)) == UGSTN_DefenceMath::GroundBounceVelocity(Rules, Up, FVector(0.3, 0.7, 0.0)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
