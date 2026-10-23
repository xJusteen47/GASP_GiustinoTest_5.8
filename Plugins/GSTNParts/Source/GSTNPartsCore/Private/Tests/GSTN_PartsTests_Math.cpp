// Automation tests of the part math: quantisation, hits, cascade, weight, restore (plan P3).

#include "Data/GSTN_PartBreakEffects.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "GSTN_PartsSettings.h"
#include "GSTN_PartsTags.h"
#include "GSTN_PartsTestUtils.h"
#include "Math/GSTN_PartMath.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsApplyGoldenTest, "GSTNParts.Core.Math.ApplyHitGolden", GSTNPartsTest::Flags)
bool FGSTN_PartsApplyGoldenTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeGoldenParts();
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	FGSTN_PartsState State;

	// Each row was worked by hand from the rule of combat-parts.md 4.2 and checked against an independent single precision script
	FGSTN_PartHitResult R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(10.f, 0, 3));
	TestEqual(TEXT("1: torso, above, part"), R.PartIndex, 0);
	TestEqual(TEXT("1: torso byte"), static_cast<int32>(State.PartDamage[0]), 26);
	TestEqual(TEXT("1: main"), R.MainDelta, 10.f, 1e-4f);
	TestFalse(TEXT("1: nothing broke"), R.NewlyBrokenMask != 0);

	R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(10.f, 0, 3));
	TestEqual(TEXT("2: torso starts from the quantised value (26 + 25.5 -> 52)"), static_cast<int32>(State.PartDamage[0]), 52);

	R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(20.f, 1, 4)); // equal: 65 %
	TestEqual(TEXT("3: arm equal band, byte"), static_cast<int32>(State.PartDamage[1]), 56);
	TestEqual(TEXT("3: part damage 13"), R.PartDelta, 13.f, 1e-4f);
	TestEqual(TEXT("3: main 20 x 0.65 x 0.3"), R.MainDelta, 3.9f, 1e-4f);

	R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(30.f, 1, 6));
	TestEqual(TEXT("4: arm byte"), static_cast<int32>(State.PartDamage[1]), 184);
	TestEqual(TEXT("4: main"), R.MainDelta, 9.f, 1e-4f);

	R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(40.f, 1, 6)); // breaks the arm, and the hand with it
	TestEqual(TEXT("5: arm byte at the cap"), static_cast<int32>(State.PartDamage[1]), 255);
	TestEqual(TEXT("5: hand cascaded to the cap"), static_cast<int32>(State.PartDamage[2]), 255);
	TestEqual(TEXT("5: newly broken arm and hand"), static_cast<int32>(R.NewlyBrokenMask), 0b0110);
	TestEqual(TEXT("5: broken mask"), static_cast<int32>(State.BrokenMask), 0b0110);
	TestEqual(TEXT("5: hit main 40 x 0.3"), R.HitMainDelta, 12.f, 1e-4f);
	TestEqual(TEXT("5: bonus of the arm (10) and the hand (5), once each"), R.BonusMainDamage, 15.f, 1e-4f);
	TestEqual(TEXT("5: main is the sum"), R.MainDelta, 27.f, 1e-4f);
	TestEqual(TEXT("5: one serial for the whole hit"), static_cast<int32>(State.BreakSerial), 1);
	TestFalse(TEXT("5: nobody asked to kill"), R.bKill);

	R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(10.f, 1, 6)); // the stump (Keep)
	TestTrue(TEXT("6: the stump is hit"), R.bHitBrokenPart && R.PartIndex == 1);
	TestEqual(TEXT("6: broken transfer is half: 10 x 0.15"), R.MainDelta, 1.5f, 1e-4f);
	TestFalse(TEXT("6: a stump takes no part damage"), R.bPartDamaged);
	TestEqual(TEXT("6: no new serial"), static_cast<int32>(State.BreakSerial), 1);

	R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(5.f, 3, 1)); // weak point, equal band
	TestEqual(TEXT("7: head byte"), static_cast<int32>(State.PartDamage[3]), 21);
	TestEqual(TEXT("7: main 5 x 0.65 x 2"), R.MainDelta, 6.5f, 1e-4f);
	TestTrue(TEXT("7: weak point flagged"), R.bWeakPointHit);

	R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(8.f, 0, 0));
	TestEqual(TEXT("8: torso byte (52 + 13.26 -> 66)"), static_cast<int32>(State.PartDamage[0]), 66);
	TestEqual(TEXT("8: main 8 x 0.65"), R.MainDelta, 5.2f, 1e-4f);

	TestEqual(TEXT("Final mask"), static_cast<int32>(State.BrokenMask), 0b0110);
	TestEqual(TEXT("Final torso byte"), static_cast<int32>(State.PartDamage[0]), 66);
	TestEqual(TEXT("Final head byte"), static_cast<int32>(State.PartDamage[3]), 21);
	TestEqual(TEXT("Final arm byte"), static_cast<int32>(State.PartDamage[1]), 255);

	// A hit on the body is the full damage, whatever it carries, and touches no state
	const FGSTN_PartsState Before = State;
	R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(7.f, INDEX_NONE, 0));
	TestEqual(TEXT("Body hit main"), R.MainDelta, 7.f);
	TestEqual(TEXT("Body hit part"), R.PartIndex, static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("Body hit leaves the state alone"), State == Before);
	R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(7.f, 9, 0)); // out of range = body
	TestEqual(TEXT("An index out of range is the body"), R.MainDelta, 7.f);
	TestTrue(TEXT("... and touches no state"), State == Before);

	// No parts at all: the hit is the body's
	FGSTN_PartsState Empty;
	R = UGSTN_PartMath::ApplyHit(TConstArrayView<FGSTN_PartDefinition>(), Bands, Empty, GSTNPartsTest::Hit(12.f, 0, 5));
	TestEqual(TEXT("No part set: identity"), R.MainDelta, 12.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsCascadeTest, "GSTNParts.Core.Math.Cascade", GSTNPartsTest::Flags)
bool FGSTN_PartsCascadeTest::RunTest(const FString& Parameters)
{
	// Parents have higher indices than their children, so a single pass in index order could not settle it
	TArray<FGSTN_PartDefinition> Parts;
	Parts.Add(GSTNPartsTest::MakePart("Finger", { "FingerBox" }, 0, 20.f, 0.1f, 1));  // 0, child of the hand
	Parts.Add(GSTNPartsTest::MakePart("Hand", { "HandBox" }, 0, 20.f, 0.1f, 2));      // 1, child of the arm
	Parts.Add(GSTNPartsTest::MakePart("Arm", { "ArmBox" }, 0, 50.f, 0.3f));            // 2, root
	Parts.Add(GSTNPartsTest::MakePart("Sleeve", { "SleeveBox" }, 0, 0.f, 0.f, 2));     // 3, unbreakable, child of the arm
	Parts.Add(GSTNPartsTest::MakePart("Leg", { "LegBox" }, 0, 50.f, 0.3f));            // 4, unrelated root
	GSTNPartsTest::AddMainDamage(Parts[0], 1.f);
	GSTNPartsTest::AddMainDamage(Parts[1], 2.f);
	GSTNPartsTest::AddMainDamage(Parts[2], 4.f);

	TestEqual(TEXT("The arm takes the whole limb"), static_cast<int32>(UGSTN_PartMath::ComputeCascade(Parts, 0b00100)), 0b01111);
	TestEqual(TEXT("The hand takes its finger only"), static_cast<int32>(UGSTN_PartMath::ComputeCascade(Parts, 0b00010)), 0b00011);
	TestEqual(TEXT("A finger takes nothing"), static_cast<int32>(UGSTN_PartMath::ComputeCascade(Parts, 0b00001)), 0b00001);
	TestEqual(TEXT("The leg is unrelated"), static_cast<int32>(UGSTN_PartMath::ComputeCascade(Parts, 0b10000)), 0b10000);
	TestEqual(TEXT("Nothing broken, nothing cascades"), static_cast<int32>(UGSTN_PartMath::ComputeCascade(Parts, 0)), 0);

	// Through a hit: the arm breaks, the hand, finger and the unbreakable sleeve go with it, each bonus once
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	FGSTN_PartsState State;
	const FGSTN_PartHitResult R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(500.f, 2, 5));
	TestEqual(TEXT("Newly broken"), static_cast<int32>(R.NewlyBrokenMask), 0b01111);
	TestEqual(TEXT("State mask"), static_cast<int32>(State.BrokenMask), 0b01111);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		TestEqual(FString::Printf(TEXT("Part %d at the cap"), Index), static_cast<int32>(State.PartDamage[Index]), 255);
	}
	TestEqual(TEXT("The leg is untouched"), static_cast<int32>(State.PartDamage[4]), 0);
	TestEqual(TEXT("Bonuses 4 + 2 + 1 once each"), R.BonusMainDamage, 7.f, 1e-4f);
	TestEqual(TEXT("One serial for the hit"), static_cast<int32>(State.BreakSerial), 1);

	// A cycle (invalid data) still terminates
	Parts[2].ParentIndex = 0;
	TestEqual(TEXT("A cycle settles"), static_cast<int32>(UGSTN_PartMath::ComputeCascade(Parts, 0b00001)), 0b01111);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsWeakPointTest, "GSTNParts.Core.Math.WeakPointMoreMainThanTorso", GSTNPartsTest::Flags)
bool FGSTN_PartsWeakPointTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	FGSTN_PartsState OnTorso;
	FGSTN_PartsState OnHead;
	FGSTN_PartsState OnArm;
	const FGSTN_PartHitResult Torso = UGSTN_PartMath::ApplyHit(Parts, Bands, OnTorso, GSTNPartsTest::Hit(10.f, 0, 6));
	const FGSTN_PartHitResult Head = UGSTN_PartMath::ApplyHit(Parts, Bands, OnHead, GSTNPartsTest::Hit(10.f, 3, 6));
	const FGSTN_PartHitResult Arm = UGSTN_PartMath::ApplyHit(Parts, Bands, OnArm, GSTNPartsTest::Hit(10.f, 1, 6));
	TestTrue(TEXT("The weak point passes more to the main pool than the torso"), Head.MainDelta > Torso.MainDelta);
	TestTrue(TEXT("... and the armoured limb less"), Arm.MainDelta < Torso.MainDelta);
	TestEqual(TEXT("Torso 10 x 1"), Torso.MainDelta, 10.f);
	TestEqual(TEXT("Head 10 x 2"), Head.MainDelta, 20.f);
	TestEqual(TEXT("Arm 10 x 0.3"), Arm.MainDelta, 3.f, 1e-4f);
	TestTrue(TEXT("Only the weak point is flagged"), Head.bWeakPointHit && !Torso.bWeakPointHit && !Arm.bWeakPointHit);
	// The head's smaller cap breaks sooner too: 10 / 40 per hit against 10 / 100
	TestTrue(TEXT("... and takes more of its cap per hit"), OnHead.PartDamage[3] > OnTorso.PartDamage[0]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsRicochetTest, "GSTNParts.Core.Math.RicochetNoDamage", GSTNPartsTest::Flags)
bool FGSTN_PartsRicochetTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	FGSTN_PartsState State;
	const FGSTN_PartsState Before = State;

	// Arm hardness 4 against penetration 3: below, ricochet
	FGSTN_PartHitResult R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(999.f, 1, 3));
	TestTrue(TEXT("Ricochets"), R.bRicochet);
	TestEqual(TEXT("No main damage"), R.MainDelta, 0.f);
	TestEqual(TEXT("No part damage"), R.PartDelta, 0.f);
	TestEqual(TEXT("No stagger"), R.StaggerScale, 0.f);
	TestEqual(TEXT("No hitstop"), R.HitstopScale, 0.f);
	TestTrue(TEXT("The state is untouched, serial included"), State == Before);
	TestEqual(TEXT("Nothing broke"), static_cast<int32>(R.NewlyBrokenMask), 0);
	TestFalse(TEXT("A ricochet is not a weak point hit"), R.bWeakPointHit);
	TestEqual(TEXT("The band is reported"), R.BandIndex, 2);

	// A ricochet band deals nothing even if the table gives it scales: the flag rules
	Bands[2].PartScale = 1.f;
	Bands[2].MainScale = 1.f;
	R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(999.f, 1, 3));
	TestTrue(TEXT("Still a ricochet"), R.bRicochet && R.MainDelta == 0.f && R.PartDelta == 0.f);
	TestTrue(TEXT("Still untouched"), State == Before);

	// IgnoreHardness gets through the same armour
	FGSTN_PartHitInput Piercing = GSTNPartsTest::Hit(10.f, 1, 0);
	Piercing.bIgnoreHardness = true;
	R = UGSTN_PartMath::ApplyHit(Parts, UGSTN_PenetrationTable::MakeDefaultBands(), State, Piercing);
	TestFalse(TEXT("A piercing hit does not ricochet"), R.bRicochet);
	TestEqual(TEXT("It takes the top band"), R.BandIndex, 0);
	TestTrue(TEXT("It damages the part"), R.bPartDamaged);

	// MainOnly leaves the part alone
	FGSTN_PartsState Fresh;
	FGSTN_PartHitInput MainOnly = GSTNPartsTest::Hit(10.f, 1, 9);
	MainOnly.bMainOnly = true;
	R = UGSTN_PartMath::ApplyHit(Parts, UGSTN_PenetrationTable::MakeDefaultBands(), Fresh, MainOnly);
	TestFalse(TEXT("MainOnly does not damage the part"), R.bPartDamaged);
	TestEqual(TEXT("... its byte stays"), static_cast<int32>(Fresh.PartDamage[1]), 0);
	TestEqual(TEXT("... the main pool still takes its share"), R.MainDelta, 3.f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsOverflowTest, "GSTNParts.Core.Math.OverflowNotCarried", GSTNPartsTest::Flags)
bool FGSTN_PartsOverflowTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();

	// A huge hit on a small part: the part breaks at the cap and the main pool gets D x transfer, not D + the overflow
	FGSTN_PartsState Small;
	const FGSTN_PartHitResult Big = UGSTN_PartMath::ApplyHit(Parts, Bands, Small, GSTNPartsTest::Hit(1000.f, 2, 9));
	TestEqual(TEXT("Hand at the cap"), static_cast<int32>(Small.PartDamage[2]), 255);
	TestEqual(TEXT("Main is 1000 x 0.2 whatever the overflow"), Big.MainDelta, 200.f, 1e-3f);
	TestEqual(TEXT("Exactly one part broke"), static_cast<int32>(Big.NewlyBrokenMask), 0b0100);

	// The same hit that exactly reaches the cap changes nothing else
	FGSTN_PartsState Exact;
	const FGSTN_PartHitResult AtCap = UGSTN_PartMath::ApplyHit(Parts, Bands, Exact, GSTNPartsTest::Hit(30.f, 2, 9));
	TestEqual(TEXT("Exactly the cap breaks the part"), static_cast<int32>(Exact.PartDamage[2]), 255);
	TestEqual(TEXT("Main is the same share"), AtCap.MainDelta, 6.f, 1e-4f);

	// Once broken the byte is stuck at 255 and hits only feed the main pool
	const FGSTN_PartHitResult After = UGSTN_PartMath::ApplyHit(Parts, Bands, Small, GSTNPartsTest::Hit(1000.f, 2, 9));
	TestEqual(TEXT("Stays at the cap"), static_cast<int32>(Small.PartDamage[2]), 255);
	TestEqual(TEXT("Stump transfer 0.1"), After.MainDelta, 100.f, 1e-3f);
	TestEqual(TEXT("No second break"), static_cast<int32>(After.NewlyBrokenMask), 0);
	return true;
}

namespace GSTNPartsTest
{
	/** A single part that breaks with one hit, with the given broken-hurtbox policy */
	TArray<FGSTN_PartDefinition> MakePolicyParts(EGSTN_BrokenHurtboxPolicy Policy)
	{
		TArray<FGSTN_PartDefinition> Parts;
		Parts.Add(MakePart("Plate", { "PlateBox" }, 0, 10.f, 0.8f));
		Parts[0].BrokenPolicy = Policy;
		return Parts;
	}

	void CheckBrokenPolicy(FAutomationTestBase& Test, EGSTN_BrokenHurtboxPolicy Policy, bool bExpectPassToBody)
	{
		const TArray<FGSTN_PartDefinition> Parts = MakePolicyParts(Policy);
		const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
		FGSTN_PartsState State;
		UGSTN_PartMath::ApplyHit(Parts, Bands, State, Hit(100.f, 0, 5)); // breaks it
		Test.TestTrue(TEXT("The plate broke"), State.IsBroken(0));

		const FGSTN_PartHitResult R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, Hit(10.f, 0, 5));
		Test.TestEqual(TEXT("Passed to the body"), R.bPassedToBody, bExpectPassToBody);
		if (bExpectPassToBody)
		{
			Test.TestEqual(TEXT("Disable: the hit is the body's"), R.PartIndex, static_cast<int32>(INDEX_NONE));
			Test.TestEqual(TEXT("Disable: full damage, no stump transfer"), R.MainDelta, 10.f);
			Test.TestFalse(TEXT("Disable: not a hit on a broken part"), R.bHitBrokenPart);
		}
		else
		{
			Test.TestEqual(TEXT("The stump is hit"), R.PartIndex, 0);
			Test.TestEqual(TEXT("Broken transfer is half of 0.8"), R.MainDelta, 4.f, 1e-4f);
			Test.TestTrue(TEXT("It is a hit on a broken part"), R.bHitBrokenPart);
		}
		Test.TestEqual(TEXT("The state stays broken"), static_cast<int32>(State.BrokenMask), 1);
		Test.TestEqual(TEXT("... and at the cap"), static_cast<int32>(State.PartDamage[0]), 255);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsPolicyKeepTest, "GSTNParts.Core.Math.BrokenPolicyKeep", GSTNPartsTest::Flags)
bool FGSTN_PartsPolicyKeepTest::RunTest(const FString& Parameters)
{
	GSTNPartsTest::CheckBrokenPolicy(*this, EGSTN_BrokenHurtboxPolicy::Keep, false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsPolicyDisableTest, "GSTNParts.Core.Math.BrokenPolicyDisable", GSTNPartsTest::Flags)
bool FGSTN_PartsPolicyDisableTest::RunTest(const FString& Parameters)
{
	GSTNPartsTest::CheckBrokenPolicy(*this, EGSTN_BrokenHurtboxPolicy::Disable, true);

	// The snapshot hides the stump's hurtboxes, which is what makes the hit the body's in the first place
	FGSTN_PartsProjection Projection;
	FGSTN_PartsState State;
	State.BrokenMask = 1;
	UGSTN_PartMath::ProjectDerived(GSTNPartsTest::MakePolicyParts(EGSTN_BrokenHurtboxPolicy::Disable), State, FGSTN_GrabGate(), Projection);
	TestTrue(TEXT("The hurtbox is hidden"), Projection.DisabledHurtboxes.Contains(FName("PlateBox")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsPolicyReplaceTest, "GSTNParts.Core.Math.BrokenPolicyReplace", GSTNPartsTest::Flags)
bool FGSTN_PartsPolicyReplaceTest::RunTest(const FString& Parameters)
{
	// The smaller stump shape is geometry; the damage is the same as for Keep
	GSTNPartsTest::CheckBrokenPolicy(*this, EGSTN_BrokenHurtboxPolicy::Replace, false);
	FGSTN_PartsProjection Projection;
	FGSTN_PartsState State;
	State.BrokenMask = 1;
	UGSTN_PartMath::ProjectDerived(GSTNPartsTest::MakePolicyParts(EGSTN_BrokenHurtboxPolicy::Replace), State, FGSTN_GrabGate(), Projection);
	TestTrue(TEXT("The hurtbox is not hidden"), Projection.DisabledHurtboxes.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsNonzeroHitTest, "GSTNParts.Core.Math.NonzeroHitChangesByte", GSTNPartsTest::Flags)
bool FGSTN_PartsNonzeroHitTest::RunTest(const FString& Parameters)
{
	// For any cap and any byte below the cap, any damage above zero, however small, moves the byte up by at least one
	const float Caps[] = { 0.001f, 1.f, 7.f, 100.f, 1000.f, 1e6f };
	const float Fractions[] = { 1e-30f, 1e-12f, 1e-6f, 0.001f, 0.3f };
	for (const float Cap : Caps)
	{
		for (const float Fraction : Fractions)
		{
			for (int32 Byte = 0; Byte < 255; ++Byte)
			{
				const uint8 Next = UGSTN_PartMath::AddDamage(static_cast<uint8>(Byte), Cap * Fraction, Cap);
				if (Next < Byte + 1 || Next > 255)
				{
					AddError(FString::Printf(TEXT("Cap %g, damage %g, byte %d went to %d"), Cap, Cap * Fraction, Byte, static_cast<int32>(Next)));
					return false;
				}
			}
		}
	}

	TestEqual(TEXT("Zero damage changes nothing"), static_cast<int32>(UGSTN_PartMath::AddDamage(40, 0.f, 100.f)), 40);
	TestEqual(TEXT("Negative damage changes nothing"), static_cast<int32>(UGSTN_PartMath::AddDamage(40, -5.f, 100.f)), 40);
	TestEqual(TEXT("NaN damage changes nothing"), static_cast<int32>(UGSTN_PartMath::AddDamage(40, std::numeric_limits<float>::quiet_NaN(), 100.f)), 40);
	TestEqual(TEXT("No cap changes nothing"), static_cast<int32>(UGSTN_PartMath::AddDamage(40, 5.f, 0.f)), 40);
	TestEqual(TEXT("Infinite damage reaches the cap"), static_cast<int32>(UGSTN_PartMath::AddDamage(40, std::numeric_limits<float>::infinity(), 100.f)), 255);
	TestEqual(TEXT("At the cap stays at the cap"), static_cast<int32>(UGSTN_PartMath::AddDamage(255, 5.f, 100.f)), 255);

	// Through a hit on a real part
	const TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	FGSTN_PartsState State;
	const FGSTN_PartHitResult R = UGSTN_PartMath::ApplyHit(Parts, UGSTN_PenetrationTable::MakeDefaultBands(), State, GSTNPartsTest::Hit(1e-9f, 0, 5));
	TestEqual(TEXT("A tiny hit still moves the byte"), static_cast<int32>(State.PartDamage[0]), 1);
	TestTrue(TEXT("... and counts as damage"), R.bPartDamaged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsQuantiseHandTest, "GSTNParts.Core.Math.QuantiseHandValues", GSTNPartsTest::Flags)
bool FGSTN_PartsQuantiseHandTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("10 over 100 is 25.5 up to 26"), static_cast<int32>(UGSTN_PartMath::Quantise(10.f, 100.f)), 26);
	TestEqual(TEXT("50 over 100 is 127.5 up to 128"), static_cast<int32>(UGSTN_PartMath::Quantise(50.f, 100.f)), 128);
	TestEqual(TEXT("100 over 100 is the cap"), static_cast<int32>(UGSTN_PartMath::Quantise(100.f, 100.f)), 255);
	TestEqual(TEXT("150 over 100 clamps"), static_cast<int32>(UGSTN_PartMath::Quantise(150.f, 100.f)), 255);
	TestEqual(TEXT("Nothing is pristine"), static_cast<int32>(UGSTN_PartMath::Quantise(0.f, 100.f)), 0);
	TestEqual(TEXT("A whisper is 1"), static_cast<int32>(UGSTN_PartMath::Quantise(0.0001f, 100.f)), 1);
	TestEqual(TEXT("An unbreakable part stores nothing"), static_cast<int32>(UGSTN_PartMath::Quantise(10.f, 0.f)), 0);
	TestEqual(TEXT("Dequantise 0"), UGSTN_PartMath::Dequantise(0, 100.f), 0.f);
	TestEqual(TEXT("Dequantise 255 is the cap"), UGSTN_PartMath::Dequantise(255, 100.f), 100.f);
	TestEqual(TEXT("Dequantise 51 is a fifth"), UGSTN_PartMath::Dequantise(51, 100.f), 20.f, 1e-4f);

	// The Blueprint wrappers agree
	TestEqual(TEXT("BP quantise"), UGSTN_PartMath::QuantisePartDamage(10.f, 100.f), 26);
	TestEqual(TEXT("BP dequantise"), UGSTN_PartMath::DequantisePartDamage(51, 100.f), 20.f, 1e-4f);
	TestEqual(TEXT("BP dequantise clamps"), UGSTN_PartMath::DequantisePartDamage(9999, 100.f), 100.f);
	TestTrue(TEXT("BP is broken"), UGSTN_PartMath::IsPartBroken(0b0100, 2) && !UGSTN_PartMath::IsPartBroken(0b0100, 1) && !UGSTN_PartMath::IsPartBroken(0b0100, 16) && !UGSTN_PartMath::IsPartBroken(0b0100, -1));
	TestEqual(TEXT("BP count"), UGSTN_PartMath::CountBrokenParts(0b101101), 4);
	TestEqual(TEXT("BP band without a table is the built-in default"), UGSTN_PartMath::GetPenetrationBandIndex(nullptr, 3, 4, false), 2);
	TestEqual(TEXT("BP band, ignoring hardness"), UGSTN_PartMath::GetPenetrationBandIndex(nullptr, 3, 4, true), 0);
	TestEqual(TEXT("BP weight without a set is the base weight"), UGSTN_PartMath::GetEffectiveWeight(nullptr, 80.f, 0.5f, 0), 80.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsSinglePartTest, "GSTNParts.Core.Math.SinglePartPlayerBreakIsAllBroken", GSTNPartsTest::Flags)
bool FGSTN_PartsSinglePartTest::RunTest(const FString& Parameters)
{
	// A player is one part whose cap mirrors the entity cap: its break is the player's death
	TArray<FGSTN_PartDefinition> Parts;
	Parts.Add(GSTNPartsTest::MakePart("Body", { "Head", "Torso", "Arm", "Leg" }, 0, 300.f));
	GSTNPartsTest::AddMainDamage(Parts[0], 0.f, true);
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	FGSTN_PartsState State;
	FGSTN_PartsProjection Projection;

	int32 Hits = 0;
	FGSTN_PartHitResult R;
	while (!State.IsBroken(0) && Hits < 100)
	{
		R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(40.f, 0, 3));
		++Hits;
		UGSTN_PartMath::ProjectDerived(Parts, State, FGSTN_GrabGate(), Projection);
		TestEqual(FString::Printf(TEXT("Hit %d: AllBroken only once broken"), Hits), Projection.bAllBroken, State.IsBroken(0));
	}
	TestEqual(TEXT("Eight hits of 40 break 300 (7.5 hits)"), Hits, 8);
	TestTrue(TEXT("The breaking hit reaches AllBroken"), R.bAllBrokenReached);
	TestTrue(TEXT("The kill flag of the break effect is raised"), R.bKill);
	TestTrue(TEXT("AllBroken is projected"), Projection.bAllBroken && Projection.bAnyBroken);

	FGameplayTagContainer Tags;
	Projection.AppendNativeTags(Tags);
	TestTrue(TEXT("The native tags follow the flags"), Tags.HasTagExact(GSTN_PartsTags::Part_Broken) && Tags.HasTagExact(GSTN_PartsTags::Part_AllBroken) && !Tags.HasTagExact(GSTN_PartsTags::Part_GrabLocked));

	// Reaching AllBroken happens once
	R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(40.f, 0, 3));
	TestFalse(TEXT("A hit on the wreck does not reach it again"), R.bAllBrokenReached);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsInboxOrderTest, "GSTNParts.Core.Math.TwoHitsSameFrameOrderIndependentGivenSortedInbox", GSTNPartsTest::Flags)
bool FGSTN_PartsInboxOrderTest::RunTest(const FString& Parameters)
{
	struct FInboxHit
	{
		int32 AttackerId;
		int32 HitboxOrder;
		FGSTN_PartHitInput Input;
	};

	const TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();

	// A heavy hit that breaks the arm and a light one on the same arm, in one frame: the light one lands on a whole arm if it is
	// resolved first and on a stump (half the transfer) if it is resolved second
	const FInboxHit Heavy{ 2, 0, GSTNPartsTest::Hit(200.f, 1, 6) };
	const FInboxHit Light{ 1, 0, GSTNPartsTest::Hit(10.f, 1, 6) };

	auto Resolve = [&](TArray<FInboxHit> Inbox, bool bSort)
	{
		if (bSort)
		{
			Inbox.StableSort([](const FInboxHit& A, const FInboxHit& B)
			{
				return A.AttackerId != B.AttackerId ? A.AttackerId < B.AttackerId : A.HitboxOrder < B.HitboxOrder;
			});
		}
		FGSTN_PartsState State;
		float Main = 0.f;
		for (const FInboxHit& Entry : Inbox)
		{
			Main += UGSTN_PartMath::ApplyHit(Parts, Bands, State, Entry.Input).MainDelta;
		}
		return TPair<FGSTN_PartsState, float>(State, Main);
	};

	const TPair<FGSTN_PartsState, float> HeavyFirst = Resolve({ Heavy, Light }, false);
	const TPair<FGSTN_PartsState, float> LightFirst = Resolve({ Light, Heavy }, false);
	TestTrue(TEXT("Unsorted, the order decides the outcome (the reason the inbox is sorted)"), HeavyFirst.Value != LightFirst.Value);

	const TPair<FGSTN_PartsState, float> SortedA = Resolve({ Heavy, Light }, true);
	const TPair<FGSTN_PartsState, float> SortedB = Resolve({ Light, Heavy }, true);
	TestTrue(TEXT("Sorted, both arrival orders give the same state"), SortedA.Key == SortedB.Key);
	TestEqual(TEXT("... and the same main damage"), SortedA.Value, SortedB.Value);
	// Attacker 1 (the light hit) resolves first: a whole arm takes 10 x 0.3, then the heavy hit breaks it: 200 x 0.3 + bonus none
	TestEqual(TEXT("... which is the light hit first"), SortedA.Value, 3.f + 60.f, 1e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsRestoreClearsTest, "GSTNParts.Core.Math.RestorePartsClearsBitsAndBytes", GSTNPartsTest::Flags)
bool FGSTN_PartsRestoreClearsTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	FGSTN_PartsState State;
	UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(500.f, 1, 6)); // arm and hand
	UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(10.f, 0, 6));  // torso scratched
	TestEqual(TEXT("Arm and hand broken"), static_cast<int32>(State.BrokenMask), 0b0110);
	const int32 SerialBefore = State.BreakSerial;

	const uint16 Restored = UGSTN_PartMath::RestoreParts(State, 0, 0.f); // everything, fully
	TestEqual(TEXT("Both unbroke"), static_cast<int32>(Restored), 0b0110);
	TestEqual(TEXT("No bit is set"), static_cast<int32>(State.BrokenMask), 0);
	for (int32 Index = 0; Index < GSTN_PartsLimits::MaxParts; ++Index)
	{
		TestEqual(FString::Printf(TEXT("Part %d is pristine"), Index), static_cast<int32>(State.PartDamage[Index]), 0);
	}
	TestEqual(TEXT("The serial moved on"), static_cast<int32>(State.BreakSerial), (SerialBefore + 1) & 15);

	// A restore with nothing broken changes nothing, serial included
	const FGSTN_PartsState Pristine = State;
	TestEqual(TEXT("Nothing to unbreak"), static_cast<int32>(UGSTN_PartMath::RestoreParts(State, 0, 0.f)), 0);
	TestTrue(TEXT("... so nothing moved"), State == Pristine);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsRestoreKeepsOthersTest, "GSTNParts.Core.Math.RestorePartsKeepsOthers", GSTNPartsTest::Flags)
bool FGSTN_PartsRestoreKeepsOthersTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	FGSTN_PartsState State;
	UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(500.f, 1, 6)); // arm and hand broken
	UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(30.f, 0, 6));  // torso: 77
	UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(30.f, 3, 6));  // head: 192
	const uint8 TorsoBefore = State.PartDamage[0];
	TestEqual(TEXT("Torso byte"), static_cast<int32>(TorsoBefore), 77);
	TestEqual(TEXT("Head byte"), static_cast<int32>(State.PartDamage[3]), 192);

	// Restore the arm and the head to half damage: 0.5 x 255 = 127.5 -> 128
	const uint16 Restored = UGSTN_PartMath::RestoreParts(State, 0b1010, 0.5f);
	TestEqual(TEXT("Only the arm was broken"), static_cast<int32>(Restored), 0b0010);
	TestEqual(TEXT("The hand stays broken"), static_cast<int32>(State.BrokenMask), 0b0100);
	TestEqual(TEXT("The arm is at half damage"), static_cast<int32>(State.PartDamage[1]), 128);
	TestEqual(TEXT("The head's heavier damage is brought down to half"), static_cast<int32>(State.PartDamage[3]), 128);
	TestEqual(TEXT("The hand's byte stays at the cap"), static_cast<int32>(State.PartDamage[2]), 255);
	TestEqual(TEXT("The torso is not in the mask: untouched"), static_cast<int32>(State.PartDamage[0]), static_cast<int32>(TorsoBefore));

	// A repair never hurts: the torso is lighter than half, so it stays
	UGSTN_PartMath::RestoreParts(State, 0b0001, 0.9f);
	TestEqual(TEXT("A restore never makes a part worse"), static_cast<int32>(State.PartDamage[0]), static_cast<int32>(TorsoBefore));

	// A restored part is left just under the cap, never broken
	FGSTN_PartsState Wreck;
	UGSTN_PartMath::ApplyHit(Parts, Bands, Wreck, GSTNPartsTest::Hit(500.f, 3, 6));
	UGSTN_PartMath::RestoreParts(Wreck, 0, 5.f);
	TestFalse(TEXT("Restored to 'more than the cap' is still unbroken"), Wreck.IsBroken(3));
	TestEqual(TEXT("... at one below the cap"), static_cast<int32>(Wreck.PartDamage[3]), 254);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsRestoreRebreakTest, "GSTNParts.Core.Math.RestorePartsRestoreThenBreakAgain", GSTNPartsTest::Flags)
bool FGSTN_PartsRestoreRebreakTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	FGSTN_PartsState State;

	UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(500.f, 3, 6));
	const int32 FirstBreak = State.BreakSerial;
	TestTrue(TEXT("Broke"), State.IsBroken(3));

	UGSTN_PartMath::RestoreParts(State, 0b1000, 0.f);
	const int32 Restore = State.BreakSerial;
	TestFalse(TEXT("Restored"), State.IsBroken(3));
	TestEqual(TEXT("Pristine again"), static_cast<int32>(State.PartDamage[3]), 0);

	const FGSTN_PartHitResult Second = UGSTN_PartMath::ApplyHit(Parts, Bands, State, GSTNPartsTest::Hit(500.f, 3, 6));
	TestTrue(TEXT("Breaks again"), State.IsBroken(3) && Second.NewlyBrokenMask == 0b1000);
	const int32 SecondBreak = State.BreakSerial;

	TestTrue(TEXT("Three different serials: the second break is a new event key"), FirstBreak != Restore && Restore != SecondBreak && FirstBreak != SecondBreak);
	TestEqual(TEXT("The serial counts up"), SecondBreak, (FirstBreak + 2) & 15);

	// The serial is four bits and wraps
	FGSTN_PartsState Wrap;
	Wrap.BreakSerial = 15;
	Wrap.AdvanceSerial();
	TestEqual(TEXT("Wraps to 0"), static_cast<int32>(Wrap.BreakSerial), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGrabGateTest, "GSTNParts.Core.GrabGate.Evaluation", GSTNPartsTest::Flags)
bool FGSTN_PartsGrabGateTest::RunTest(const FString& Parameters)
{
	FGSTN_GrabGate Gate;
	TestFalse(TEXT("An empty gate never locks (nothing broken)"), UGSTN_PartMath::IsGrabLocked(Gate, 0));
	TestFalse(TEXT("An empty gate never locks (everything broken)"), UGSTN_PartMath::IsGrabLocked(Gate, 0xFFFF));

	Gate.MinBrokenParts = 2;
	TestTrue(TEXT("Min 2: none broken is locked"), UGSTN_PartMath::IsGrabLocked(Gate, 0));
	TestTrue(TEXT("Min 2: one broken is locked"), UGSTN_PartMath::IsGrabLocked(Gate, 0b0100));
	TestFalse(TEXT("Min 2: two broken opens it"), UGSTN_PartMath::IsGrabLocked(Gate, 0b0101));
	TestFalse(TEXT("Min 2: more opens it"), UGSTN_PartMath::IsGrabLocked(Gate, 0b1111));

	Gate = FGSTN_GrabGate();
	Gate.RequiredParts = { 1, 3 };
	TestTrue(TEXT("Required all: none is locked"), UGSTN_PartMath::IsGrabLocked(Gate, 0));
	TestTrue(TEXT("Required all: one of two is locked"), UGSTN_PartMath::IsGrabLocked(Gate, 0b0010));
	TestFalse(TEXT("Required all: both opens it"), UGSTN_PartMath::IsGrabLocked(Gate, 0b1010));
	TestTrue(TEXT("Required all: other parts do not count"), UGSTN_PartMath::IsGrabLocked(Gate, 0b0101));

	Gate.bAnyOf = true;
	TestTrue(TEXT("Any of: none is locked"), UGSTN_PartMath::IsGrabLocked(Gate, 0));
	TestFalse(TEXT("Any of: one opens it"), UGSTN_PartMath::IsGrabLocked(Gate, 0b0010));
	TestFalse(TEXT("Any of: the other opens it"), UGSTN_PartMath::IsGrabLocked(Gate, 0b1000));
	TestTrue(TEXT("Any of: an unrelated part does not"), UGSTN_PartMath::IsGrabLocked(Gate, 0b0001));

	// Both conditions together
	Gate = FGSTN_GrabGate();
	Gate.MinBrokenParts = 2;
	Gate.RequiredParts = { 0 };
	TestTrue(TEXT("Count met, required part missing: locked"), UGSTN_PartMath::IsGrabLocked(Gate, 0b0110));
	TestTrue(TEXT("Required part broken, count short: locked"), UGSTN_PartMath::IsGrabLocked(Gate, 0b0001));
	TestFalse(TEXT("Both met: open"), UGSTN_PartMath::IsGrabLocked(Gate, 0b0011));

	// A required part that does not exist can never be broken
	Gate = FGSTN_GrabGate();
	Gate.RequiredParts = { 40 };
	TestTrue(TEXT("A missing required part keeps the gate shut"), UGSTN_PartMath::IsGrabLocked(Gate, 0xFFFF));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsProjectTest, "GSTNParts.Core.Math.ProjectDerived", GSTNPartsTest::Flags)
bool FGSTN_PartsProjectTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	const FGameplayTag Broken = GSTN_PartsTags::Part_Broken; // any registered tag stands in for a designer leaf
	const FGameplayTag Effect = GSTN_PartsTags::Part_Effect;
	const FGameplayTag Allbroken = GSTN_PartsTags::Part_AllBroken;

	FGSTN_PartBreakEffect_Tags TagEffect;
	TagEffect.Granted.AddTag(Effect);
	TagEffect.Removed.AddTag(Allbroken);
	Parts[1].OnBreak.Add(FInstancedStruct::Make(TagEffect));
	FGSTN_PartBreakEffect_DisableHitGroups Groups;
	Groups.GroupMask = 0b0011;
	Parts[1].OnBreak.Add(FInstancedStruct::Make(Groups));
	FGSTN_PartBreakEffect_DisableHitGroups MoreGroups;
	MoreGroups.GroupMask = 0b0100;
	Parts[2].OnBreak.Add(FInstancedStruct::Make(MoreGroups));
	FGSTN_PartBreakEffect_DisableHurtbox Hide;
	Hide.Hurtboxes = { "HeadBox" };
	Parts[2].OnBreak.Add(FInstancedStruct::Make(Hide));
	Parts[2].OnBreak.Add(FInstancedStruct()); // an unknown effect is skipped
	GSTNPartsTest::AddMainDamage(Parts[1], 10.f); // a one-shot effect projects nothing
	Parts[3].BrokenPolicy = EGSTN_BrokenHurtboxPolicy::Disable;

	FGSTN_PartsProjection Projection;
	FGSTN_PartsState State;
	UGSTN_PartMath::ProjectDerived(Parts, State, FGSTN_GrabGate(), Projection);
	TestFalse(TEXT("Nothing broken: no flag"), Projection.bAnyBroken || Projection.bAllBroken || Projection.bGrabLocked);
	TestTrue(TEXT("... no tags, no groups, no hidden hurtboxes"), Projection.Granted.IsEmpty() && Projection.Removed.IsEmpty() && Projection.HitGroupMask == 0 && Projection.DisabledHurtboxes.IsEmpty());

	State.BrokenMask = 0b0010; // the arm only
	UGSTN_PartMath::ProjectDerived(Parts, State, FGSTN_GrabGate(), Projection);
	TestTrue(TEXT("Broken is projected"), Projection.bAnyBroken && !Projection.bAllBroken);
	TestTrue(TEXT("The arm's tag is granted"), Projection.Granted.HasTagExact(Effect) && !Projection.Granted.HasTagExact(Broken));
	TestTrue(TEXT("The arm's removed tag is suppressed"), Projection.Removed.HasTagExact(Allbroken));
	TestEqual(TEXT("The arm's hit groups"), static_cast<int32>(Projection.HitGroupMask), 0b0011);
	TestTrue(TEXT("The hand is not broken, so its effects are not projected"), Projection.DisabledHurtboxes.IsEmpty());

	State.BrokenMask = 0b1110; // arm, hand and head
	UGSTN_PartMath::ProjectDerived(Parts, State, FGSTN_GrabGate(), Projection);
	TestEqual(TEXT("Hit groups are the union over the broken parts"), static_cast<int32>(Projection.HitGroupMask), 0b0111);
	TestTrue(TEXT("The hand's effect hides the head's hurtbox"), Projection.DisabledHurtboxes.Contains(FName("HeadBox")));
	TestTrue(TEXT("The head's own Disable policy hides its hurtbox once"), Projection.DisabledHurtboxes.Num() == 1);

	// Reusing the projection leaves nothing of the previous frame behind (the state went back: a rollback)
	State.BrokenMask = 0;
	UGSTN_PartMath::ProjectDerived(Parts, State, FGSTN_GrabGate(), Projection);
	TestTrue(TEXT("Back to nothing"), !Projection.bAnyBroken && Projection.Granted.IsEmpty() && Projection.HitGroupMask == 0 && Projection.DisabledHurtboxes.IsEmpty());

	// Bits above the part count are ignored
	State.BrokenMask = 0xFFF0;
	UGSTN_PartMath::ProjectDerived(Parts, State, FGSTN_GrabGate(), Projection);
	TestFalse(TEXT("A mask beyond the parts projects nothing"), Projection.bAnyBroken);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsAllBrokenTest, "GSTNParts.Core.Math.AllBrokenCountsBreakableOnly", GSTNPartsTest::Flags)
bool FGSTN_PartsAllBrokenTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	Parts[1].PartCap = 0.f; // the arm plate is unbreakable
	TestEqual(TEXT("Breakable parts"), static_cast<int32>(UGSTN_PartMath::GetBreakableMask(Parts)), 0b1101);
	TestFalse(TEXT("None broken"), UGSTN_PartMath::IsAllBroken(Parts, 0));
	TestFalse(TEXT("Some broken"), UGSTN_PartMath::IsAllBroken(Parts, 0b0101));
	TestTrue(TEXT("Every breakable broken, the plate untouched"), UGSTN_PartMath::IsAllBroken(Parts, 0b1101));
	TestTrue(TEXT("... with the plate cascaded too"), UGSTN_PartMath::IsAllBroken(Parts, 0b1111));

	TArray<FGSTN_PartDefinition> Armour;
	Armour.Add(GSTNPartsTest::MakePart("Shell", { "Shell" }, 9, 0.f));
	TestFalse(TEXT("A set with nothing breakable is never all broken"), UGSTN_PartMath::IsAllBroken(Armour, 0xFFFF));
	TestFalse(TEXT("... and no parts at all neither"), UGSTN_PartMath::IsAllBroken(TConstArrayView<FGSTN_PartDefinition>(), 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsWeightHandTest, "GSTNParts.Core.Determinism.WeightCurveTableEqualsHandValues", GSTNPartsTest::Flags)
bool FGSTN_PartsWeightHandTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	Parts[1].LostMassFraction = 0.25f;
	Parts[3].LostMassFraction = 0.1f;
	const FGSTN_PartsBakedCurve Curve = FGSTN_PartsBakedCurve::MakeLinear(1.f, 0.6f);

	// Progress 0: the table's first entry, exactly 1
	TestEqual(TEXT("No damage, no loss"), UGSTN_PartMath::EffectiveWeightScale(Parts, Curve, 0.f, 0, 0.35f), 1.f);
	// Progress 1: 39321 / 65535
	TestEqual(TEXT("At the cap"), UGSTN_PartMath::EffectiveWeightScale(Parts, Curve, 1.f, 0, 0.35f), 39321.f / 65535.f);
	// Progress 0.5: 52428 / 65535 by the hand-worked read
	TestEqual(TEXT("Half way"), UGSTN_PartMath::EffectiveWeightScale(Parts, Curve, 0.5f, 0, 0.35f), 52428.f / 65535.f);
	// Lost mass multiplies in part index order: arm 0.75, head 0.9
	const float Half = 52428.f / 65535.f;
	TestEqual(TEXT("Arm lost"), UGSTN_PartMath::EffectiveWeightScale(Parts, Curve, 0.5f, 0b0010, 0.35f), Half * 0.75f);
	TestEqual(TEXT("Arm and head lost"), UGSTN_PartMath::EffectiveWeightScale(Parts, Curve, 0.5f, 0b1010, 0.35f), (Half * 0.75f) * 0.9f);
	TestEqual(TEXT("The weight is the base times the scale"), UGSTN_PartMath::EffectiveWeight(80.f, Parts, Curve, 0.5f, 0b0010, 0.35f), 80.f * (Half * 0.75f));
	TestEqual(TEXT("Broken parts beyond the part count lose nothing"), UGSTN_PartMath::EffectiveWeightScale(Parts, Curve, 0.f, 0xFFF0, 0.35f), 1.f);

	// The set's default curve is the settings' shape, so the same number comes from the asset
	UGSTN_PartSetDefinition* Set = NewObject<UGSTN_PartSetDefinition>(GetTransientPackage());
	Set->Parts = Parts;
	Set->RebakeWeightCurve();
	const float Scale = UGSTN_PartsSettings::Get().DefaultWeightScaleAtCap;
	TestTrue(TEXT("The asset's curve is the linear default"), Set->GetWeightCurve() == FGSTN_PartsBakedCurve::MakeLinear(1.f, Scale));
	TestEqual(TEXT("The Blueprint helper reads the same"), UGSTN_PartMath::GetEffectiveWeight(Set, 80.f, 0.5f, 0b0010),
		UGSTN_PartMath::EffectiveWeight(80.f, Parts, Set->GetWeightCurve(), 0.5f, 0b0010, UGSTN_PartsSettings::Get().MinWeightScale));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsWeightClampTest, "GSTNParts.Core.Determinism.EffectiveWeightNeverBelowClamp", GSTNPartsTest::Flags)
bool FGSTN_PartsWeightClampTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	for (FGSTN_PartDefinition& Part : Parts)
	{
		Part.LostMassFraction = 0.9f;
	}
	const float Clamp = 0.35f;
	const FGSTN_PartsBakedCurve Falling = FGSTN_PartsBakedCurve::MakeLinear(1.f, 0.6f);
	const FGSTN_PartsBakedCurve Steep = FGSTN_PartsBakedCurve::MakeLinear(1.f, 0.f);

	TestEqual(TEXT("Everything lost sits exactly on the clamp"), UGSTN_PartMath::EffectiveWeightScale(Parts, Falling, 1.f, 0b1111, Clamp), Clamp);
	for (uint32 Mask = 0; Mask < 16; ++Mask)
	{
		for (int32 Step = 0; Step <= 20; ++Step)
		{
			const float Progress = Step / 20.f;
			for (const FGSTN_PartsBakedCurve* Curve : { &Falling, &Steep })
			{
				const float Scale = UGSTN_PartMath::EffectiveWeightScale(Parts, *Curve, Progress, static_cast<uint16>(Mask), Clamp);
				if (Scale < Clamp || Scale > 1.f)
				{
					AddError(FString::Printf(TEXT("Mask %u progress %g gave %g outside [%g, 1]"), Mask, Progress, Scale, Clamp));
					return false;
				}
			}
		}
	}
	TestEqual(TEXT("A clamp of 1 pins the weight"), UGSTN_PartMath::EffectiveWeightScale(Parts, Steep, 1.f, 0b1111, 1.f), 1.f);
	TestEqual(TEXT("A clamp of 0 lets a steep curve reach zero"), UGSTN_PartMath::EffectiveWeightScale(Parts, Steep, 1.f, 0, 0.f), 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
