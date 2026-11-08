// P18: hurtbox openings (combat-m4-hits 9c B) and the per-hurtbox hardness override (combat-parts.md §3.4). No world.

#include "Data/GSTN_PartHitData.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "GSTN_PartsTestUtils.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HitRunner.h"
#include "Hits/GSTN_HitShapeUtils.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Misc/AutomationTest.h"
#include "Pipeline/GSTN_HitStep_PartResolve.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Pipeline/GSTN_PartsSnapshotExt.h"
#include "Rules/GSTN_CombatRules.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPartsOpeningsTest
{
	// --- Openings: a heavy-armour leg (box, priority 0) around a hollow cavity (sphere, priority 5) whose mouth opens on +X ---

	constexpr int32 ShellBox = 0;
	constexpr int32 CavityBox = 1;

	FGSTN_HitShape MakeSphere(double Radius, const FVector& At)
	{
		FGSTN_HitShape Shape;
		Shape.Type = EGSTN_ShapeType::Sphere;
		Shape.Extent = FVector(Radius, Radius, Radius);
		Shape.Local = FTransform(At);
		return Shape;
	}

	/** The leg: shell 40 x 40 x 100 cm, cavity of radius 10 at its centre, a mouth of radius 4 from the cavity to the shell's +X face, 5 cm pass radius */
	TArray<FGSTN_Hurtbox> MakeLeg(bool bWithOpening)
	{
		TArray<FGSTN_Hurtbox> Leg;
		FGSTN_Hurtbox& Shell = Leg.AddDefaulted_GetRef();
		Shell.Name = "LegShell";
		Shell.Shape.Type = EGSTN_ShapeType::Box;
		Shell.Shape.Extent = FVector(20.0, 20.0, 50.0);
		Shell.Priority = 0;

		FGSTN_Hurtbox& Cavity = Leg.AddDefaulted_GetRef();
		Cavity.Name = "LegCavity";
		Cavity.Shape = MakeSphere(10.0, FVector::ZeroVector);
		Cavity.Priority = 5;
		Cavity.Opening.bEnabled = bWithOpening;
		Cavity.Opening.Mouth = MakeSphere(4.0, FVector(12.0, 0.0, 0.0));
		Cavity.Opening.MaxPassRadiusCm = 5.f;
		return Leg;
	}

	/** The attacker tick's step 4 over one hitbox at rest (no sweep), with the victim at the origin: overlap, the opening rule, the pick */
	int32 PickOnLeg(TConstArrayView<FGSTN_Hurtbox> Leg, const FGSTN_HitShape& Hitbox, const FTransform& Carrier = FTransform::Identity)
	{
		TArray<int32, TInlineAllocator<4>> Overlapped;
		for (int32 Index = 0; Index < Leg.Num(); ++Index)
		{
			if (UGSTN_HitShapeUtils::Overlap(Hitbox, Carrier, Leg[Index].Shape, FTransform::Identity)
				&& (!Leg[Index].Opening.bEnabled || UGSTN_HitShapeUtils::PassesOpening(Leg[Index].Opening, Hitbox, Carrier, Carrier, false, FTransform::Identity)))
			{
				Overlapped.Add(Index);
			}
		}
		return UGSTN_HitShapeUtils::PickHurtboxIndex(Overlapped, Leg);
	}

	// --- Hardness override: one abdomen part (hardness 1, light underneath) whose top hurtbox is medium (3) ---

	constexpr int32 TopBox = 0;
	constexpr int32 BottomBox = 1;

	struct FAbdomenFixture
	{
		TStrongObjectPtr<UGSTN_PartSetDefinition> Set;
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Hurtboxes;
		TStrongObjectPtr<UGSTN_CombatRules> Rules;
		FGSTN_PartsSnapshotExt Ext;

		FAbdomenFixture()
		{
			Set.Reset(NewObject<UGSTN_PartSetDefinition>(GetTransientPackage()));
			Set->Parts.Add(GSTNPartsTest::MakePart("Abdomen", { "AbdomenTop", "AbdomenBottom" }, 1, 30.f));
			Set->Parts[0].HurtboxHardnessOverride.Add(FName("AbdomenTop"), 3);
			Set->RebakeWeightCurve();

			Hurtboxes.Reset(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
			for (const TCHAR* Name : { TEXT("AbdomenTop"), TEXT("AbdomenBottom") })
			{
				Hurtboxes->Hurtboxes.AddDefaulted_GetRef().Name = FName(Name);
			}

			Rules.Reset(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
			Ext.PartSet = Set.Get();
		}

		FGSTN_StepResult RunStep(int32 HurtboxIndex, float Damage, int32 Penetration, EGSTN_StepResult* OutVerdict = nullptr) const
		{
			FGSTN_HitResolutionContext Context;
			Context.Incoming.Hit.Damage = Damage;
			FGSTN_PartHitData PartHit;
			PartHit.Penetration = static_cast<uint8>(Penetration);
			Context.Incoming.Hit.ExtensionData.Add(FInstancedStruct::Make(PartHit));
			Context.Victim.Weight = 100.f;
			Context.Victim.SnapshotExtension = &Ext;
			Context.HurtboxSet = Hurtboxes.Get();
			Context.HurtboxIndex = static_cast<uint8>(HurtboxIndex);

			FGSTN_HitContextView View;
			FGSTN_HitPipelineRunner::MakeView(Rules.Get(), Context, View);
			FGSTN_StepResult Result;
			const EGSTN_StepResult Verdict = GetDefault<UGSTN_HitStep_PartResolve>()->Resolve(View, Result);
			if (OutVerdict)
			{
				*OutVerdict = Verdict;
			}
			return Result;
		}
	};

	const FGSTN_PartHitOutcome* FindOpeningOutcome(const FGSTN_StepResult& Result)
	{
		for (const FInstancedStruct& Entry : Result.OutcomeExtensions)
		{
			if (const FGSTN_PartHitOutcome* Found = Entry.GetPtr<FGSTN_PartHitOutcome>())
			{
				return Found;
			}
		}
		return nullptr;
	}
}

using namespace GSTNPartsOpeningsTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsOpeningsMouthTest, "GSTNParts.Core.Openings.CavityReachedOnlyThroughMouth", GSTNPartsTest::Flags)
bool FGSTN_PartsOpeningsMouthTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_Hurtbox> Leg = MakeLeg(true);
	// A 2 cm stab inside the cavity: through the mouth on +X it lands on the cavity, from the -X side (no hole there) on the shell
	TestEqual(TEXT("a thin hitbox through the mouth reaches the cavity"), PickOnLeg(Leg, MakeSphere(2.0, FVector(9.0, 0.0, 0.0))), CavityBox);
	TestEqual(TEXT("the same hitbox on the closed side falls to the armour"), PickOnLeg(Leg, MakeSphere(2.0, FVector(-9.0, 0.0, 0.0))), ShellBox);

	// The helper alone: the mouth decides, not the cavity overlap the caller already did
	const FGSTN_HurtboxOpening& Opening = Leg[CavityBox].Opening;
	TestTrue(TEXT("overlapping the mouth passes"), UGSTN_HitShapeUtils::PassesOpening(Opening, MakeSphere(2.0, FVector(9.0, 0.0, 0.0)), FTransform::Identity, FTransform::Identity, false, FTransform::Identity));
	TestFalse(TEXT("missing the mouth does not"), UGSTN_HitShapeUtils::PassesOpening(Opening, MakeSphere(2.0, FVector(-9.0, 0.0, 0.0)), FTransform::Identity, FTransform::Identity, false, FTransform::Identity));

	// The victim's transform moves the mouth with the leg (root-relative, like every hurtbox shape)
	const FTransform VictimWorld(FRotator(0.0, 90.0, 0.0), FVector(100.0, 0.0, 0.0));
	const FGSTN_HitShape Stab = MakeSphere(2.0, VictimWorld.TransformPosition(FVector(9.0, 0.0, 0.0)));
	TestTrue(TEXT("the mouth follows the victim's root"), UGSTN_HitShapeUtils::PassesOpening(Opening, Stab, FTransform::Identity, FTransform::Identity, false, VictimWorld));

	// Swept: a stab that crosses the mouth between frames counts like the swept hurtbox test
	const FTransform Before(FVector(30.0, 0.0, 0.0));
	const FTransform Now(FVector(9.0, 0.0, 0.0));
	TestTrue(TEXT("a swept hitbox that crossed the mouth passes"), UGSTN_HitShapeUtils::PassesOpening(Opening, MakeSphere(2.0, FVector::ZeroVector), Before, Now, true, FTransform::Identity));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsOpeningsBulkyTest, "GSTNParts.Core.Openings.BulkyHitboxFallsToArmour", GSTNPartsTest::Flags)
bool FGSTN_PartsOpeningsBulkyTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_Hurtbox> Leg = MakeLeg(true);
	// A 15 cm hammer head overlaps the shell, the cavity and the mouth, but does not fit a 5 cm hole: the armour takes it
	const FGSTN_HitShape Hammer = MakeSphere(15.0, FVector(24.0, 0.0, 0.0));
	TestTrue(TEXT("the hammer does overlap the cavity (the case the rule exists for)"), UGSTN_HitShapeUtils::Overlap(Hammer, FTransform::Identity, Leg[CavityBox].Shape, FTransform::Identity));
	TestEqual(TEXT("a bulky hitbox falls to the armour"), PickOnLeg(Leg, Hammer), ShellBox);

	// A box hitbox passes by its smallest half-size: a 30 cm wide but 6 cm thick blade still does not fit 5 cm
	FGSTN_HitShape Blade;
	Blade.Type = EGSTN_ShapeType::Box;
	Blade.Extent = FVector(6.0, 30.0, 30.0);
	Blade.Local = FTransform(FVector(9.0, 0.0, 0.0));
	TestEqual(TEXT("a box whose thinnest side is too thick falls to the armour"), PickOnLeg(Leg, Blade), ShellBox);

	// The carrier's scale grows the hitbox: a stab that fits at scale 1 does not at scale 3
	TestEqual(TEXT("a scaled-up stab no longer fits"), PickOnLeg(Leg, MakeSphere(2.0, FVector(3.0, 0.0, 0.0)), FTransform(FQuat::Identity, FVector::ZeroVector, FVector(3.0))), ShellBox);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsOpeningsProjectileTest, "GSTNParts.Core.Openings.ProjectileRadiusFits", GSTNPartsTest::Flags)
bool FGSTN_PartsOpeningsProjectileTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_Hurtbox> Leg = MakeLeg(true);
	// A 1 cm bullet, a long thin capsule (passes by its radius, not its length), the pass radius exactly (inclusive), just above it
	TestEqual(TEXT("a bullet fits"), PickOnLeg(Leg, MakeSphere(1.0, FVector(9.0, 0.0, 0.0))), CavityBox);

	FGSTN_HitShape Bolt;
	Bolt.Type = EGSTN_ShapeType::Capsule;
	Bolt.Extent = FVector(1.0, 1.0, 20.0);
	Bolt.Local = FTransform(FRotator(90.0, 0.0, 0.0), FVector(9.0, 0.0, 0.0));
	TestEqual(TEXT("a long thin bolt fits by its radius"), PickOnLeg(Leg, Bolt), CavityBox);

	TestEqual(TEXT("a radius equal to the pass radius fits"), PickOnLeg(Leg, MakeSphere(5.0, FVector(9.0, 0.0, 0.0))), CavityBox);
	TestEqual(TEXT("a radius just above does not"), PickOnLeg(Leg, MakeSphere(5.5, FVector(9.0, 0.0, 0.0))), ShellBox);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsOpeningsNoOpeningTest, "GSTNParts.Core.Openings.NoOpeningKeepsPriorityRule", GSTNPartsTest::Flags)
bool FGSTN_PartsOpeningsNoOpeningTest::RunTest(const FString& Parameters)
{
	// The same leg with the opening off is today's rule: the highest Priority among the overlapped, whatever the size or side
	const TArray<FGSTN_Hurtbox> Leg = MakeLeg(false);
	TestEqual(TEXT("the hammer picks the cavity by priority"), PickOnLeg(Leg, MakeSphere(15.0, FVector(24.0, 0.0, 0.0))), CavityBox);
	TestEqual(TEXT("the closed side picks the cavity by priority"), PickOnLeg(Leg, MakeSphere(2.0, FVector(-9.0, 0.0, 0.0))), CavityBox);
	TestEqual(TEXT("only the shell overlapped"), PickOnLeg(Leg, MakeSphere(2.0, FVector(18.0, 0.0, 40.0))), ShellBox);

	// A disabled opening passes whatever its data says, and the default hurtbox has none
	FGSTN_HurtboxOpening Closed;
	Closed.MaxPassRadiusCm = 0.f;
	Closed.Mouth = MakeSphere(1.0, FVector(1000.0, 0.0, 0.0));
	TestTrue(TEXT("a disabled opening never refuses"), UGSTN_HitShapeUtils::PassesOpening(Closed, MakeSphere(50.0, FVector::ZeroVector), FTransform::Identity, FTransform::Identity, false, FTransform::Identity));
	TestFalse(TEXT("a default hurtbox has no opening"), FGSTN_Hurtbox().Opening.bEnabled);
	return true;
}

// P18b: the minion attacker's overlap loop (GSTN_MinionCombatSubsystem.cpp, the step that builds Overlapped against a fighter's hurtboxes) is
// inline in a world-bound subsystem, so this runs its exact body (swept when the move sweeps, then the opening rule) on a leg, headless
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsOpeningsMinionAttackerTest, "GSTNParts.Core.Openings.MinionAttackerRespectsOpening", GSTNPartsTest::Flags)
bool FGSTN_PartsOpeningsMinionAttackerTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPartsOpeningsTest;
	auto MinionPick = [](TConstArrayView<FGSTN_Hurtbox> Leg, const FGSTN_HitShape& HitShape, const FTransform& PreviousCarrier, const FTransform& Carrier, bool bSweep)
	{
		TArray<int32, TInlineAllocator<4>> Overlapped;
		for (int32 HurtIndex = 0; HurtIndex < Leg.Num(); ++HurtIndex)
		{
			const bool bTouches = bSweep
				? UGSTN_HitShapeUtils::SweptOverlap(HitShape, PreviousCarrier, Carrier, Leg[HurtIndex].Shape, FTransform::Identity)
				: UGSTN_HitShapeUtils::Overlap(HitShape, Carrier, Leg[HurtIndex].Shape, FTransform::Identity);
			if (bTouches && (!Leg[HurtIndex].Opening.bEnabled || UGSTN_HitShapeUtils::PassesOpening(Leg[HurtIndex].Opening, HitShape, PreviousCarrier, Carrier, bSweep, FTransform::Identity)))
			{
				Overlapped.Add(HurtIndex);
			}
		}
		return UGSTN_HitShapeUtils::PickHurtboxIndex(Overlapped, Leg);
	};

	const TArray<FGSTN_Hurtbox> Leg = MakeLeg(true);
	const TArray<FGSTN_Hurtbox> Plain = MakeLeg(false);
	const FGSTN_HitShape Stab = MakeSphere(2.0, FVector::ZeroVector);
	const FTransform Outside(FVector(40.0, 0.0, 0.0));
	const FTransform Mouth(FVector(10.0, 0.0, 0.0));
	const FTransform ClosedSide(FVector(-9.0, 0.0, 0.0));

	TestEqual(TEXT("a stab at the mouth reaches the cavity"), MinionPick(Leg, Stab, Mouth, Mouth, false), CavityBox);
	TestEqual(TEXT("a swept stab that enters through the mouth reaches the cavity"), MinionPick(Leg, Stab, Outside, Mouth, true), CavityBox);
	TestEqual(TEXT("a stab on the closed side falls to the armour"), MinionPick(Leg, Stab, ClosedSide, ClosedSide, false), ShellBox);
	TestEqual(TEXT("a swept stab on the closed side falls to the armour"), MinionPick(Leg, Stab, FTransform(FVector(-40.0, 0.0, 0.0)), ClosedSide, true), ShellBox);
	TestEqual(TEXT("a bulky swing at the mouth falls to the armour"), MinionPick(Leg, MakeSphere(15.0, FVector::ZeroVector), Mouth, Mouth, false), ShellBox);
	TestEqual(TEXT("without an opening the closed side picks the cavity by priority"), MinionPick(Plain, Stab, ClosedSide, ClosedSide, false), CavityBox);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsOpeningsOverrideTest,"GSTNParts.Core.Openings.OverrideWinsOverPartHardness", GSTNPartsTest::Flags)
bool FGSTN_PartsOpeningsOverrideTest::RunTest(const FString& Parameters)
{
	FAbdomenFixture Fixture;
	const FGSTN_PartDefinition& Abdomen = Fixture.Set->Parts[0];
	TestEqual(TEXT("the overridden hurtbox's hardness"), Abdomen.GetHardnessFor("AbdomenTop"), 3);
	TestEqual(TEXT("a hurtbox without an override takes the part's"), Abdomen.GetHardnessFor("AbdomenBottom"), 1);
	TestEqual(TEXT("no override at all is the part's"), FGSTN_PartDefinition().GetHardnessFor("AbdomenTop"), 0);

	// Penetration 2: through the light bottom (2 - 1 = +1) it is a full hit; on the medium top (2 - 3 = -1) it bounces, though the part's own hardness would let it in
	const FGSTN_StepResult Bottom = Fixture.RunStep(BottomBox, 10.f, 2);
	const FGSTN_PartHitOutcome* BottomOutcome = FindOpeningOutcome(Bottom);
	TestTrue(TEXT("the bottom takes the full hit"), BottomOutcome && !BottomOutcome->HasFlag(GSTNPartOutcomeFlags::Ricochet) && BottomOutcome->PartIndex == 0);
	TestEqual(TEXT("full damage to the main pool"), Bottom.DamageAdd, 10.f);

	const FGSTN_StepResult Top = Fixture.RunStep(TopBox, 10.f, 2);
	const FGSTN_PartHitOutcome* TopOutcome = FindOpeningOutcome(Top);
	TestTrue(TEXT("the top ricochets"), TopOutcome && TopOutcome->HasFlag(GSTNPartOutcomeFlags::Ricochet) && TopOutcome->PartIndex == 0);
	TestEqual(TEXT("and deals nothing"), Top.DamageAdd, 0.f);

	// The math alone: the override replaces the part's hardness in the band lookup, negative is the part's
	FGSTN_PartsState State;
	FGSTN_PartHitInput Input = GSTNPartsTest::Hit(10.f, 0, 2);
	Input.HardnessOverride = 3;
	TestTrue(TEXT("ApplyHit uses the override"), UGSTN_PartMath::ApplyHit(Fixture.Set->GetParts(), UGSTN_PenetrationTable::MakeDefaultBands(), State, Input).bRicochet);
	Input.HardnessOverride = -1;
	TestFalse(TEXT("and the part's hardness without one"), UGSTN_PartMath::ApplyHit(Fixture.Set->GetParts(), UGSTN_PenetrationTable::MakeDefaultBands(), State, Input).bRicochet);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsOpeningsEitherSideTest, "GSTNParts.Core.Openings.EitherSideBreaksTheWholePart", GSTNPartsTest::Flags)
bool FGSTN_PartsOpeningsEitherSideTest::RunTest(const FString& Parameters)
{
	// Penetration 4 gets through both sides. One part, one pool, one cap (30): 20 on top plus 20 underneath break it
	{
		FAbdomenFixture Fixture;
		const FGSTN_PartHitOutcome* First = FindOpeningOutcome(Fixture.RunStep(TopBox, 20.f, 4));
		TestTrue(TEXT("the first hit damages the part without breaking it"), First && First->PartIndex == 0 && First->ResultBrokenMask == 0 && First->NewPartDamage > 0);
		if (First)
		{
			Fixture.Ext.PartDamage[0] = First->ResultPartDamage[0];
			Fixture.Ext.BrokenMask = First->ResultBrokenMask;
		}
		const FGSTN_PartHitOutcome* Second = FindOpeningOutcome(Fixture.RunStep(BottomBox, 20.f, 4));
		TestTrue(TEXT("the other side adds to the same pool and breaks the whole part"), Second && Second->PartIndex == 0 && (Second->NewlyBrokenMask & 1u) != 0);
	}
	// Either side alone breaks it with one big hit
	for (const int32 Side : { TopBox, BottomBox })
	{
		FAbdomenFixture Fixture;
		const FGSTN_PartHitOutcome* Outcome = FindOpeningOutcome(Fixture.RunStep(Side, 40.f, 4));
		TestTrue(FString::Printf(TEXT("a hit through hurtbox %d breaks the part"), Side), Outcome && Outcome->PartIndex == 0 && (Outcome->ResultBrokenMask & 1u) != 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsOpeningsRicochetTest, "GSTNParts.Core.Openings.RicochetPredictionUsesOverride", GSTNPartsTest::Flags)
bool FGSTN_PartsOpeningsRicochetTest::RunTest(const FString& Parameters)
{
	// The attacker predicts a ricochet by running this same step on its copy of the victim's static data (the attacker outcome seam reads
	// the Ricochet flag). With the override it must stop exactly where the authority stops, and only from the data: same inputs, same answer
	FAbdomenFixture Attacker;
	FAbdomenFixture Authority;
	EGSTN_StepResult AttackerVerdict = EGSTN_StepResult::Continue;
	EGSTN_StepResult AuthorityVerdict = EGSTN_StepResult::Continue;
	const FGSTN_StepResult Predicted = Attacker.RunStep(TopBox, 10.f, 2, &AttackerVerdict);
	const FGSTN_StepResult Confirmed = Authority.RunStep(TopBox, 10.f, 2, &AuthorityVerdict);

	const FGSTN_PartHitOutcome* PredictedOutcome = FindOpeningOutcome(Predicted);
	const FGSTN_PartHitOutcome* ConfirmedOutcome = FindOpeningOutcome(Confirmed);
	TestTrue(TEXT("the prediction is a ricochet"), PredictedOutcome && PredictedOutcome->HasFlag(GSTNPartOutcomeFlags::Ricochet));
	TestTrue(TEXT("the step stops (nothing lands)"), AttackerVerdict == EGSTN_StepResult::Stop);
	TestTrue(TEXT("the authority agrees"), ConfirmedOutcome && PredictedOutcome && ConfirmedOutcome->Flags == PredictedOutcome->Flags && AuthorityVerdict == AttackerVerdict);

	// The broken mask or the damage do not change the band: a damaged part still ricochets on the top
	Attacker.Ext.PartDamage[0] = 200;
	const FGSTN_PartHitOutcome* Damaged = FindOpeningOutcome(Attacker.RunStep(TopBox, 10.f, 2));
	TestTrue(TEXT("damage taken does not move the override's band"), Damaged && Damaged->HasFlag(GSTNPartOutcomeFlags::Ricochet));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
