// Automation tests of the defence plugin: the shield value, the bubble and the reaction to a blocked hit (plan F4).
// Goldens are hand values; each test says how it was derived.

#include "Data/GSTN_DefenceRules.h"
#include "GSTN_DefenceTags.h"
#include "GSTN_DefenceTestUtils.h"
#include "Math/GSTN_DefenceMath.h"
#include "Misc/AutomationTest.h"
#include "Sync/GSTN_SyncFragment.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_ShieldRates ShieldTestRates(double Drain, double Regen, double Max, int32 Delay)
	{
		FGSTN_ShieldRates Rates;
		Rates.Drain = Drain;
		Rates.Regen = Regen;
		Rates.Max = Max;
		Rates.RegenDelayFrames = Delay;
		return Rates;
	}

	FGSTN_ShieldView ShieldTestHeld(double Value, int32 Stamp)
	{
		FGSTN_ShieldView View;
		View.Value = Value;
		View.StampFrame = Stamp;
		View.bHeld = true;
		View.PressFrame = Stamp;
		return View;
	}

	FGSTN_ShieldView ShieldTestReleased(double Value, int32 Stamp, int32 ReleaseFrame)
	{
		FGSTN_ShieldView View;
		View.Value = Value;
		View.StampFrame = Stamp;
		View.bHeld = false;
		View.ReleaseFrame = ReleaseFrame;
		return View;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceShieldAvailableTest, "GSTNDefence.Core.Shield.AvailableFlagTimesKit", GSTNDefenceTest::Flags)
bool FGSTN_DefenceShieldAvailableTest::RunTest(const FString& Parameters)
{
	// The shield exists only where the mode enables it AND the kit has one (m6 spec 1d): the truth table
	UGSTN_DefenceRules* Enabled = GSTNDefenceTest::MakeRules();
	Enabled->bShieldEnabled = true;
	UGSTN_DefenceRules* Disabled = GSTNDefenceTest::MakeRules();
	Disabled->bShieldEnabled = false;

	TestTrue(TEXT("Mode on, kit has a shield"), UGSTN_DefenceMath::IsShieldAvailable(Enabled, true));
	TestFalse(TEXT("Mode on, kit has none"), UGSTN_DefenceMath::IsShieldAvailable(Enabled, false));
	TestFalse(TEXT("Mode off, kit has a shield (PvE)"), UGSTN_DefenceMath::IsShieldAvailable(Disabled, true));
	TestFalse(TEXT("Mode off, kit has none"), UGSTN_DefenceMath::IsShieldAvailable(Disabled, false));
	TestFalse(TEXT("No rules asset, no shield"), UGSTN_DefenceMath::IsShieldAvailable(nullptr, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceShieldValueTest, "GSTNDefence.Core.Shield.ValueDrainRegenDelayClamp", GSTNDefenceTest::Flags)
bool FGSTN_DefenceShieldValueTest::RunTest(const FString& Parameters)
{
	const FGSTN_ShieldRates Rates = ShieldTestRates(0.5, 0.25, 50.0, 30);

	// Held: 50 at frame 0 draining 0.5 per frame
	const FGSTN_ShieldView Held = ShieldTestHeld(50.0, 0);
	TestEqual(TEXT("Held, at the stamp"), UGSTN_DefenceMath::ShieldValueAt(Held, 0, Rates), 50.0);
	TestEqual(TEXT("Held, 40 frames: 50 - 0.5 * 40"), UGSTN_DefenceMath::ShieldValueAt(Held, 40, Rates), 30.0);
	TestEqual(TEXT("Held, 100 frames: exactly empty"), UGSTN_DefenceMath::ShieldValueAt(Held, 100, Rates), 0.0);
	TestEqual(TEXT("Held, 200 frames: clamped at 0"), UGSTN_DefenceMath::ShieldValueAt(Held, 200, Rates), 0.0);
	TestEqual(TEXT("A frame before the stamp reads the stamped value"), UGSTN_DefenceMath::ShieldValueAt(Held, -5, Rates), 50.0);

	// Released at frame 40 holding 30: nothing until 40 + 30 = 70, then +0.25 per frame, capped at the maximum
	const FGSTN_ShieldView Released = ShieldTestReleased(30.0, 40, 40);
	TestEqual(TEXT("Released, at the release"), UGSTN_DefenceMath::ShieldValueAt(Released, 40, Rates), 30.0);
	TestEqual(TEXT("Released, still inside the regeneration delay"), UGSTN_DefenceMath::ShieldValueAt(Released, 69, Rates), 30.0);
	TestEqual(TEXT("Released, on the last delay frame"), UGSTN_DefenceMath::ShieldValueAt(Released, 70, Rates), 30.0);
	TestEqual(TEXT("Released, 4 frames of regeneration: 30 + 0.25 * 4"), UGSTN_DefenceMath::ShieldValueAt(Released, 74, Rates), 31.0);
	TestEqual(TEXT("Released, long after: clamped at the maximum"), UGSTN_DefenceMath::ShieldValueAt(Released, 1000, Rates), 50.0);

	// A stamp above the maximum reads as the maximum
	TestEqual(TEXT("A stamped value above the maximum is clamped"), UGSTN_DefenceMath::ShieldValueAt(ShieldTestHeld(80.0, 0), 0, Rates), 50.0);

	// The damage re-stamp: the bubble keeps its size, the value drops, regeneration waits for the stamp
	const FGSTN_ShieldView Hit = UGSTN_DefenceMath::ApplyShieldDamage(Released, 50, Rates, 10.0);
	TestEqual(TEXT("A blocked hit during the delay lowers the value"), UGSTN_DefenceMath::ShieldValueAt(Hit, 50, Rates), 20.0);
	TestEqual(TEXT("and regeneration still starts at the end of the delay"), UGSTN_DefenceMath::ShieldValueAt(Hit, 74, Rates), 21.0);
	const FGSTN_ShieldView Broken = UGSTN_DefenceMath::ApplyShieldDamage(Released, 50, Rates, 99.0);
	TestEqual(TEXT("Damage past the value floors at 0 (a break)"), UGSTN_DefenceMath::ShieldValueAt(Broken, 50, Rates), 0.0);

	// A late hit after regeneration began delays regeneration to the hit frame
	const FGSTN_ShieldView LateHit = UGSTN_DefenceMath::ApplyShieldDamage(Released, 90, Rates, 5.0);
	TestEqual(TEXT("At the hit frame: 30 + 0.25 * 20 - 5"), UGSTN_DefenceMath::ShieldValueAt(LateHit, 90, Rates), 30.0);
	TestEqual(TEXT("Regeneration restarts from the hit frame"), UGSTN_DefenceMath::ShieldValueAt(LateHit, 94, Rates), 31.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceShieldRestampTest, "GSTNDefence.Core.Shield.RateChangeRestamp", GSTNDefenceTest::Flags)
bool FGSTN_DefenceShieldRestampTest::RunTest(const FString& Parameters)
{
	// Held, drain 1 per frame; at frame 10 the drain doubles. Without a re-stamp the new rate would rewrite history
	const FGSTN_ShieldRates OldDrain = ShieldTestRates(1.0, 0.5, 50.0, 30);
	const FGSTN_ShieldRates NewDrain = ShieldTestRates(2.0, 0.5, 50.0, 30);
	const FGSTN_ShieldView Held = ShieldTestHeld(50.0, 0);
	TestEqual(TEXT("Old rate at frame 10"), UGSTN_DefenceMath::ShieldValueAt(Held, 10, OldDrain), 40.0);
	TestEqual(TEXT("Without a re-stamp the new rate rewrites history: 50 - 2 * 15"), UGSTN_DefenceMath::ShieldValueAt(Held, 15, NewDrain), 20.0);

	const FGSTN_ShieldView Restamped = UGSTN_DefenceMath::RestampShield(Held, 10, OldDrain);
	TestEqual(TEXT("The re-stamp keeps the value of the old rate"), Restamped.Value, 40.0);
	TestEqual(TEXT("and moves the stamp"), Restamped.StampFrame, 10);
	TestTrue(TEXT("and keeps the shield held"), Restamped.bHeld);
	TestEqual(TEXT("With the re-stamp the new rate applies from frame 10: 40 - 2 * 5"), UGSTN_DefenceMath::ShieldValueAt(Restamped, 15, NewDrain), 30.0);

	// Released, regeneration 0.5 after a delay of 30: at frame 50 the rate doubles to 1
	const FGSTN_ShieldRates OldRegen = ShieldTestRates(1.0, 0.5, 50.0, 30);
	const FGSTN_ShieldRates NewRegen = ShieldTestRates(1.0, 1.0, 50.0, 30);
	const FGSTN_ShieldView Released = ShieldTestReleased(20.0, 0, 0);
	TestEqual(TEXT("Old regeneration at frame 50: 20 + 0.5 * 20"), UGSTN_DefenceMath::ShieldValueAt(Released, 50, OldRegen), 30.0);
	const FGSTN_ShieldView RestampedReleased = UGSTN_DefenceMath::RestampShield(Released, 50, OldRegen);
	TestEqual(TEXT("The re-stamp at frame 50 keeps 30"), RestampedReleased.Value, 30.0);
	TestEqual(TEXT("New regeneration from frame 50: 30 + 1 * 10"), UGSTN_DefenceMath::ShieldValueAt(RestampedReleased, 60, NewRegen), 40.0);

	// A re-stamp during the delay does not start regeneration early
	const FGSTN_ShieldView InDelay = UGSTN_DefenceMath::RestampShield(Released, 10, OldRegen);
	TestEqual(TEXT("Re-stamped during the delay, still nothing at frame 30"), UGSTN_DefenceMath::ShieldValueAt(InDelay, 30, NewRegen), 20.0);
	TestEqual(TEXT("and regeneration starts at the end of the delay: 20 + 1 * 5"), UGSTN_DefenceMath::ShieldValueAt(InDelay, 35, NewRegen), 25.0);

	// Press and release events stamp and flip the held flag
	const FGSTN_ShieldView Raised = UGSTN_DefenceMath::RaiseShield(Released, 100, NewRegen);
	TestTrue(TEXT("Raise holds the shield"), Raised.bHeld);
	TestEqual(TEXT("Raise remembers the press frame"), Raised.PressFrame, 100);
	TestEqual(TEXT("Raise forgets the old release"), Raised.ReleaseFrame, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Raise stamps the value regenerated so far: 20 + 1 * (100 - 30), capped at 50"), Raised.Value, 50.0);
	const FGSTN_ShieldView Dropped = UGSTN_DefenceMath::ReleaseShield(Raised, 120, NewRegen);
	TestFalse(TEXT("Release drops the shield"), Dropped.bHeld);
	TestEqual(TEXT("Release remembers the release frame"), Dropped.ReleaseFrame, 120);
	TestEqual(TEXT("Release stamps the drained value: 50 - 1 * 20"), Dropped.Value, 30.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceShieldRadiusFixedTest, "GSTNDefence.Core.Shield.RadiusFixedAtAnyValue", GSTNDefenceTest::Flags)
bool FGSTN_DefenceShieldRadiusFixedTest::RunTest(const FString& Parameters)
{
	// Owner decision 2026-10-26: the bubble has a fixed size. MakeShieldBubble has no value or tilt input at all; the shield value
	// only decides the break, so the same rules give the same radius whatever the entity and whatever its shield holds.
	FGSTN_ShieldRules Rules;
	Rules.RadiusCm = 85.f;
	Rules.CentreHeightCm = 90.f;

	const FGSTN_ShieldRates Rates = ShieldTestRates(0.5, 0.25, 50.0, 30);
	const double Values[] = { 50.0, 25.0, 1.0, 0.0 };
	for (const double Value : Values)
	{
		// The value is read here only to show that it exists and varies; the bubble is built without it
		const double Read = UGSTN_DefenceMath::ShieldValueAt(ShieldTestHeld(Value, 0), 0, Rates);
		TestEqual(TEXT("The value under test"), Read, Value);
		const FGSTN_ShieldBubble Bubble = UGSTN_DefenceMath::MakeShieldBubble(Rules, FTransform(FVector(100.0, 0.0, 0.0)));
		TestEqual(FString::Printf(TEXT("Radius at shield value %g"), Value), Bubble.Radius, 85.0);
	}

	const FTransform Transforms[] = {
		FTransform::Identity,
		FTransform(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(90.0)), FVector(1000.0, -300.0, 20.0)),
		FTransform(FQuat(FVector::XAxisVector, FMath::DegreesToRadians(180.0)), FVector(-5.0, 6.0, 7.0), FVector(2.0)) };
	for (const FTransform& Transform : Transforms)
	{
		const FGSTN_ShieldBubble Bubble = UGSTN_DefenceMath::MakeShieldBubble(Rules, Transform);
		TestEqual(TEXT("Radius at any entity transform"), Bubble.Radius, 85.0);
		TestTrue(TEXT("The centre is CentreHeightCm from the root"), FMath::IsNearlyEqual((Bubble.Centre - Transform.GetLocation()).Size(), 90.0, 1e-9));
	}

	// The extra hurtbox: root-relative sphere, highest priority, defence source, ignoring what the caller names
	const FGameplayTag Unblockable = FGameplayTag::RequestGameplayTag(TEXT("Combat.HitType.Unblockable"), false);
	FGameplayTagContainer Ignore;
	if (Unblockable.IsValid())
	{
		Ignore.AddTag(Unblockable);
	}
	const FGSTN_ExtraHurtbox Extra = UGSTN_DefenceMath::MakeShieldBubbleHurtbox(Rules, Ignore);
	TestEqual(TEXT("A sphere"), static_cast<int32>(Extra.Shape.Type), static_cast<int32>(EGSTN_ShapeType::Sphere));
	TestEqual(TEXT("of radius RadiusCm (X)"), Extra.Shape.Extent.X, 85.0);
	TestEqual(TEXT("(Y)"), Extra.Shape.Extent.Y, 85.0);
	TestEqual(TEXT("(Z)"), Extra.Shape.Extent.Z, 85.0);
	TestEqual(TEXT("centred CentreHeightCm up in root space"), Extra.Shape.Local.GetLocation().Z, 90.0);
	TestTrue(TEXT("with no sideways offset"), FMath::IsNearlyZero(Extra.Shape.Local.GetLocation().X) && FMath::IsNearlyZero(Extra.Shape.Local.GetLocation().Y));
	TestEqual(TEXT("above every body hurtbox"), static_cast<int32>(Extra.Priority), 255);
	TestEqual(TEXT("sourced by the defence feature"), static_cast<int32>(Extra.Source), static_cast<int32>(GSTNFeatureIds::Defence));
	TestEqual(TEXT("ignoring the hit types the caller names"), Extra.IgnoreHitTypes.Num(), Ignore.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceShieldCentreUpTest, "GSTNDefence.Core.Shield.CentreArbitraryUp", GSTNDefenceTest::Flags)
bool FGSTN_DefenceShieldCentreUpTest::RunTest(const FString& Parameters)
{
	FGSTN_ShieldRules Rules;
	Rules.CentreHeightCm = 90.f;

	TestTrue(TEXT("Up is +Z"), UGSTN_DefenceMath::ShieldCentreAt(Rules, FVector(0, 0, 1)).Equals(FVector(0, 0, 90), 1e-9));
	TestTrue(TEXT("Up is +X (a wall)"), UGSTN_DefenceMath::ShieldCentreAt(Rules, FVector(1, 0, 0)).Equals(FVector(90, 0, 0), 1e-9));
	TestTrue(TEXT("Up is -Z, not unit length (a ceiling)"), UGSTN_DefenceMath::ShieldCentreAt(Rules, FVector(0, 0, -2)).Equals(FVector(0, 0, -90), 1e-9));
	TestTrue(TEXT("Up is a diagonal"), UGSTN_DefenceMath::ShieldCentreAt(Rules, FVector(0, 3, 4)).Equals(FVector(0, 54, 72), 1e-9));
	TestTrue(TEXT("A zero up gives the root"), UGSTN_DefenceMath::ShieldCentreAt(Rules, FVector::ZeroVector).IsNearlyZero());

	// The world bubble follows the entity's own up through its rotation (a quarter turn about Y takes +Z to +X)
	const FTransform Wall(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(90.0)), FVector(10.0, 20.0, 30.0));
	const FGSTN_ShieldBubble Bubble = UGSTN_DefenceMath::MakeShieldBubble(Rules, Wall);
	TestTrue(TEXT("The centre is 90 cm along the entity's up from the root"), Bubble.Centre.Equals(Wall.GetLocation() + Wall.GetUnitAxis(EAxis::Z) * 90.0, 1e-9));
	TestTrue(TEXT("and not above the root in world Z"), FMath::IsNearlyEqual(Bubble.Centre.Z, 30.0, 1e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceShieldStunPushbackTest, "GSTNDefence.Core.Shield.StunAndPushbackScaleWithDamage", GSTNDefenceTest::Flags)
bool FGSTN_DefenceShieldStunPushbackTest::RunTest(const FString& Parameters)
{
	FGSTN_ShieldRules Rules;
	Rules.ShieldStunBase = 3;
	Rules.ShieldStunPerDamage = 0.5f;
	Rules.MaxShieldStun = 30;
	Rules.PushbackSpeed = 150.f;
	Rules.PushbackSpeedPerDamage = 15.f;
	Rules.ShieldDamagePerDamage = 1.5f;

	// Stun: min(30, 3 + 0.5 * damage), rounded half up
	TestEqual(TEXT("No damage: the base"), UGSTN_DefenceMath::ShieldStunFrames(0.0, Rules), 3);
	TestEqual(TEXT("Damage 10: 3 + 5"), UGSTN_DefenceMath::ShieldStunFrames(10.0, Rules), 8);
	TestEqual(TEXT("Damage 5: 3 + 2.5 rounds half up to 6"), UGSTN_DefenceMath::ShieldStunFrames(5.0, Rules), 6);
	TestEqual(TEXT("Damage 100 is capped at 30"), UGSTN_DefenceMath::ShieldStunFrames(100.0, Rules), 30);
	TestEqual(TEXT("A negative damage counts as 0"), UGSTN_DefenceMath::ShieldStunFrames(-8.0, Rules), 3);

	// Pushback: 150 + 15 * damage
	TestEqual(TEXT("No damage: the base speed"), UGSTN_DefenceMath::PushbackSpeedOf(0.0, Rules), 150.0);
	TestEqual(TEXT("Damage 10: 150 + 150"), UGSTN_DefenceMath::PushbackSpeedOf(10.0, Rules), 300.0);
	TestEqual(TEXT("A negative damage counts as 0"), UGSTN_DefenceMath::PushbackSpeedOf(-3.0, Rules), 150.0);

	// More damage never means less stun or pushback
	int32 PreviousStun = 0;
	double PreviousPush = 0.0;
	bool bMonotonic = true;
	for (int32 Damage = 0; Damage <= 60; ++Damage)
	{
		const int32 Stun = UGSTN_DefenceMath::ShieldStunFrames(Damage, Rules);
		const double Push = UGSTN_DefenceMath::PushbackSpeedOf(Damage, Rules);
		bMonotonic &= Stun >= PreviousStun && Push >= PreviousPush;
		PreviousStun = Stun;
		PreviousPush = Push;
	}
	TestTrue(TEXT("Stun and pushback never decrease with damage"), bMonotonic);

	// Shield damage: the hit's own value wins, else damage times the rules' factor, then the victim and relationship scales
	TestEqual(TEXT("The hit says 12"), UGSTN_DefenceMath::ShieldDamageOf(12.0, 10.0, Rules, 1.0, 1.0), 12.0);
	TestEqual(TEXT("The hit says 0: 10 * 1.5"), UGSTN_DefenceMath::ShieldDamageOf(0.0, 10.0, Rules, 1.0, 1.0), 15.0);
	TestEqual(TEXT("Victim multiplier 0.5"), UGSTN_DefenceMath::ShieldDamageOf(0.0, 10.0, Rules, 0.5, 1.0), 7.5);
	TestEqual(TEXT("Relationship scale 2 on top"), UGSTN_DefenceMath::ShieldDamageOf(12.0, 10.0, Rules, 0.5, 2.0), 12.0);

	// Relationship scales: exact entry, deepest matching ancestor, else 1 (any registered tags serve as the keys)
	const FGameplayTag State = FGameplayTag::RequestGameplayTag(TEXT("Combat.State"), false);
	const FGameplayTag Tech = FGameplayTag::RequestGameplayTag(TEXT("Combat.State.Tech"), false);
	TestTrue(TEXT("The key tags exist"), State.IsValid() && Tech.IsValid());
	if (State.IsValid() && Tech.IsValid())
	{
		Rules.IncomingShieldDamageScaleByRelationship.Add(State, 3.f);
		Rules.IncomingShieldDamageScaleByRelationship.Add(Tech, 5.f);
		TestEqual(TEXT("The exact entry"), UGSTN_DefenceMath::RelationshipScaleOf(Rules, Tech), 5.0);
		TestEqual(TEXT("The deepest ancestor wins over a shallower one"), UGSTN_DefenceMath::RelationshipScaleOf(Rules, GSTNDefenceTags::State_Tech_Lockout), 5.0);
		TestEqual(TEXT("A shallower ancestor"), UGSTN_DefenceMath::RelationshipScaleOf(Rules, GSTNDefenceTags::State_Shielding), 3.0);
		TestEqual(TEXT("No match is 1"), UGSTN_DefenceMath::RelationshipScaleOf(Rules, GSTNDefenceTags::Immunity_Parry), 1.0);
		TestEqual(TEXT("An unset relationship is 1"), UGSTN_DefenceMath::RelationshipScaleOf(Rules, FGameplayTag()), 1.0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
