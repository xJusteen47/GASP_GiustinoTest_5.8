// Automation tests of the wounds, level 1 (combat-parts.md 3.4): the wounded mask, the wounded hardness, the wound hit, the validation. The
// lag history half (RewoundHitReadsWoundStateOfItsFrame) lives in GSTNPartsNP, where the history is.

#include "Data/GSTN_PartHitData.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Data/GSTN_PartsValidation.h"
#include "GSTN_PartsTestUtils.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HitRunner.h"
#include "Hits/GSTN_HitSteps.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hits/GSTN_KnockbackModels.h"
#include "Misc/AutomationTest.h"
#include "Pipeline/GSTN_HitStep_PartResolve.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Pipeline/GSTN_PartsSnapshotExt.h"
#include "Rules/GSTN_CombatRules.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 Torso = 0;
	constexpr int32 Arm = 1;
	constexpr int32 Hand = 2;
	constexpr int32 Head = 3;

	/** The enemy of the part tests (torso 0, arm 4, hand 2, head 1) with the arm and the head able to be wounded: the arm falls to 1, the head to 0 */
	TArray<FGSTN_PartDefinition> MakeWoundableParts()
	{
		TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
		Parts[Arm].WoundedHardness = 1;
		Parts[Head].WoundedHardness = 0;
		return Parts;
	}

	FGSTN_PartHitInput WoundHit(float Damage, int32 PartIndex, int32 Penetration)
	{
		FGSTN_PartHitInput Input = GSTNPartsTest::Hit(Damage, PartIndex, Penetration);
		Input.bWound = true;
		return Input;
	}

	/** The step alone over a victim that carries a parts extension, with a hurtbox set whose boxes are the parts' in order and a body box */
	struct FWoundStepFixture
	{
		TStrongObjectPtr<UGSTN_PartSetDefinition> Set;
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Hurtboxes;
		TStrongObjectPtr<UGSTN_CombatRules> Rules;
		TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model;
		FGSTN_PartsSnapshotExt Ext;

		FWoundStepFixture()
		{
			Set.Reset(NewObject<UGSTN_PartSetDefinition>(GetTransientPackage()));
			Set->Parts = MakeWoundableParts();
			Set->RebakeWeightCurve();

			Hurtboxes.Reset(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
			for (const TCHAR* Name : { TEXT("TorsoBox"), TEXT("ArmBox"), TEXT("HandBox"), TEXT("HeadBox"), TEXT("BodyBox") })
			{
				FGSTN_Hurtbox& Box = Hurtboxes->Hurtboxes.AddDefaulted_GetRef();
				Box.Name = FName(Name);
			}

			Model.Reset(NewObject<UGSTN_KnockbackModel_WeightDriven>(GetTransientPackage()));
			Rules.Reset(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
			Rules->KnockbackModel = Model.Get();
			Rules->HitPipeline.Add(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_PartResolve::StaticClass()));
			Rules->ResolveHitPipeline();

			Ext.PartSet = Set.Get();
		}

		FGSTN_HitResolutionContext MakeContext(int32 HurtboxIndex, int32 Penetration, bool bWound) const
		{
			FGSTN_HitResolutionContext Context;
			Context.Incoming.Hit.Damage = 10.f;
			FGSTN_PartHitData PartHit;
			PartHit.Penetration = static_cast<uint8>(Penetration);
			Context.Incoming.Hit.ExtensionData.Add(FInstancedStruct::Make(PartHit));
			if (bWound)
			{
				Context.Incoming.Hit.ExtensionData.Add(FInstancedStruct::Make(FGSTN_PartBreakEffect_Wound()));
			}
			Context.Victim.Weight = 100.f;
			Context.Victim.SnapshotExtension = &Ext;
			Context.HurtboxSet = Hurtboxes.Get();
			Context.HurtboxIndex = static_cast<uint8>(HurtboxIndex);
			return Context;
		}

		/** The outcome entry of the step on a context, null when the step wrote none */
		bool RunStep(const FGSTN_HitResolutionContext& Context, FGSTN_PartHitOutcome& OutOutcome) const
		{
			FGSTN_HitContextView View;
			FGSTN_HitPipelineRunner::MakeView(Rules.Get(), Context, View);
			FGSTN_StepResult Result;
			GetDefault<UGSTN_HitStep_PartResolve>()->Resolve(View, Result);
			for (const FInstancedStruct& Entry : Result.OutcomeExtensions)
			{
				if (const FGSTN_PartHitOutcome* Found = Entry.GetPtr<FGSTN_PartHitOutcome>())
				{
					OutOutcome = *Found;
					return true;
				}
			}
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsWoundSetsChosenPartTest, "GSTNParts.Core.Wounds.WoundSetsChosenPart", GSTNPartsTest::Flags)
bool FGSTN_PartsWoundSetsChosenPartTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PartDefinition> Parts = MakeWoundableParts();
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();

	// A wound hit that lands on the arm (penetration 5 over hardness 4) marks the arm and nothing else
	FGSTN_PartsState State;
	UGSTN_PartMath::ApplyHit(Parts, Bands, State, WoundHit(5.f, Arm, 5));
	TestEqual(TEXT("only the arm's bit"), static_cast<int32>(State.WoundedMask), 1 << Arm);
	TestTrue(TEXT("IsWounded agrees"), State.IsWounded(Arm) && !State.IsWounded(Head) && !State.IsWounded(INDEX_NONE) && !State.IsWounded(16));
	TestEqual(TEXT("and the break state is untouched by the mark"), static_cast<int32>(State.BrokenMask), 0);

	// A second wound elsewhere adds its bit and keeps the first
	UGSTN_PartMath::ApplyHit(Parts, Bands, State, WoundHit(5.f, Head, 5));
	TestEqual(TEXT("the head joins the arm"), static_cast<int32>(State.WoundedMask), (1 << Arm) | (1 << Head));

	// A part with no wounded hardness is not marked, a hit without the wound does not mark, the body has no bit
	FGSTN_PartsState Other;
	UGSTN_PartMath::ApplyHit(Parts, Bands, Other, WoundHit(5.f, Torso, 5));
	TestEqual(TEXT("a part that cannot be wounded stays clean"), static_cast<int32>(Other.WoundedMask), 0);
	UGSTN_PartMath::ApplyHit(Parts, Bands, Other, GSTNPartsTest::Hit(5.f, Arm, 5));
	TestEqual(TEXT("a hit that carries no wound marks nothing"), static_cast<int32>(Other.WoundedMask), 0);
	UGSTN_PartMath::ApplyHit(Parts, Bands, Other, WoundHit(5.f, INDEX_NONE, 5));
	TestEqual(TEXT("a body hit marks nothing"), static_cast<int32>(Other.WoundedMask), 0);

	// A ricochet marks nothing: the hit did not land
	const FGSTN_PartHitResult Bounce = UGSTN_PartMath::ApplyHit(Parts, Bands, Other, WoundHit(5.f, Arm, 0));
	TestTrue(TEXT("penetration 0 on the arm ricochets"), Bounce.bRicochet);
	TestEqual(TEXT("and leaves no wound"), static_cast<int32>(Other.WoundedMask), 0);

	// The wound is for the hits after it: the wounding hit itself met the full hardness
	FGSTN_PartsState First;
	const FGSTN_PartHitResult Landed = UGSTN_PartMath::ApplyHit(Parts, Bands, First, WoundHit(5.f, Arm, 4));
	TestFalse(TEXT("penetration 4 over hardness 4 is the equal band, not a ricochet"), Landed.bRicochet);
	TestEqual(TEXT("the wounding hit read the unwounded hardness (equal band 1)"), Landed.BandIndex, 1);

	// The same through the step: the mark is the chosen hurtbox's part, written into the outcome entry
	FWoundStepFixture Fixture;
	FGSTN_PartHitOutcome Outcome;
	TestTrue(TEXT("the step writes an entry"), Fixture.RunStep(Fixture.MakeContext(1, 5, true), Outcome));
	TestEqual(TEXT("ArmBox (hurtbox 1) wounds the arm only"), static_cast<int32>(Outcome.ResultWoundedMask), 1 << Arm);
	TestEqual(TEXT("and the state the outcome hands the applier carries it"), static_cast<int32>(Outcome.GetResultState().WoundedMask), 1 << Arm);
	TestTrue(TEXT("HeadBox (hurtbox 3) wounds the head"), Fixture.RunStep(Fixture.MakeContext(3, 5, true), Outcome) && Outcome.ResultWoundedMask == (1 << Head));
	TestTrue(TEXT("BodyBox (no part) wounds nothing"), Fixture.RunStep(Fixture.MakeContext(4, 5, true), Outcome) && Outcome.ResultWoundedMask == 0);
	TestTrue(TEXT("a hit without the wound struct wounds nothing"), Fixture.RunStep(Fixture.MakeContext(1, 5, false), Outcome) && Outcome.ResultWoundedMask == 0);

	// Two wound hits of one drain apply in order, each from the state the earlier left (the step is given the snapshot's mask each time)
	Fixture.Ext.WoundedMask = 1 << Arm;
	TestTrue(TEXT("a victim already wounded on the arm keeps it when the head is wounded"),
		Fixture.RunStep(Fixture.MakeContext(3, 5, true), Outcome) && Outcome.ResultWoundedMask == ((1 << Arm) | (1 << Head)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsWoundedHardnessTest, "GSTNParts.Core.Wounds.WoundedHardnessUsedWhileBitOn", GSTNPartsTest::Flags)
bool FGSTN_PartsWoundedHardnessTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PartDefinition> Parts = MakeWoundableParts();
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();

	// Penetration 2 against the arm: hardness 4 ricochets (delta -2), wounded hardness 1 gets through (delta +1, the top band)
	FGSTN_PartsState Whole;
	const FGSTN_PartHitResult Bounce = UGSTN_PartMath::ApplyHit(Parts, Bands, Whole, GSTNPartsTest::Hit(10.f, Arm, 2));
	TestTrue(TEXT("bit off: the arm's own hardness, ricochet"), Bounce.bRicochet);

	FGSTN_PartsState Wounded;
	Wounded.WoundedMask = 1 << Arm;
	const FGSTN_PartHitResult Through = UGSTN_PartMath::ApplyHit(Parts, Bands, Wounded, GSTNPartsTest::Hit(10.f, Arm, 2));
	TestFalse(TEXT("bit on: the wounded hardness, no ricochet"), Through.bRicochet);
	TestEqual(TEXT("the top band"), Through.BandIndex, 0);
	TestTrue(TEXT("and the arm took damage"), Through.bPartDamaged && Wounded.PartDamage[Arm] > 0);

	// The wound replaces the hurtbox override too: the part is as hard as its wound says whichever hurtbox the hit came through
	FGSTN_PartsState WithOverride;
	WithOverride.WoundedMask = 1 << Arm;
	FGSTN_PartHitInput Overridden = GSTNPartsTest::Hit(10.f, Arm, 2);
	Overridden.HardnessOverride = 9;
	TestFalse(TEXT("an override of 9 does not hold a wounded part"), UGSTN_PartMath::ApplyHit(Parts, Bands, WithOverride, Overridden).bRicochet);
	FGSTN_PartsState NoWound;
	TestTrue(TEXT("while it does hold an unwounded one"), UGSTN_PartMath::ApplyHit(Parts, Bands, NoWound, Overridden).bRicochet);

	// Another part's bit does not harden or soften this one, and a bit on a part without a wounded hardness changes nothing
	FGSTN_PartsState HeadWounded;
	HeadWounded.WoundedMask = 1 << Head;
	TestTrue(TEXT("the head's wound leaves the arm's hardness alone"), UGSTN_PartMath::ApplyHit(Parts, Bands, HeadWounded, GSTNPartsTest::Hit(10.f, Arm, 2)).bRicochet);
	FGSTN_PartsState TorsoBit;
	TorsoBit.WoundedMask = 1 << Torso;
	FGSTN_PartsState TorsoClean;
	const FGSTN_PartHitResult WithBit = UGSTN_PartMath::ApplyHit(Parts, Bands, TorsoBit, GSTNPartsTest::Hit(10.f, Torso, 0));
	const FGSTN_PartHitResult WithoutBit = UGSTN_PartMath::ApplyHit(Parts, Bands, TorsoClean, GSTNPartsTest::Hit(10.f, Torso, 0));
	TestTrue(TEXT("a stray bit on a part with no wound changes no result"), WithBit.BandIndex == WithoutBit.BandIndex && WithBit.MainDelta == WithoutBit.MainDelta && TorsoBit.PartDamage[Torso] == TorsoClean.PartDamage[Torso]);

	// A wounded hardness that is not lower is allowed (a wound may harden: a scab); the rule is "the wound's value while the bit is on"
	TArray<FGSTN_PartDefinition> Harder = MakeWoundableParts();
	Harder[Arm].WoundedHardness = 9;
	FGSTN_PartsState HarderWounded;
	HarderWounded.WoundedMask = 1 << Arm;
	TestTrue(TEXT("a wound can also harden"), UGSTN_PartMath::ApplyHit(Harder, Bands, HarderWounded, GSTNPartsTest::Hit(10.f, Arm, 5)).bRicochet);

	// A wounded part with equal state compares equal, and the mask is part of the comparison
	FGSTN_PartsState A;
	FGSTN_PartsState B;
	TestTrue(TEXT("equal states"), A == B);
	B.WoundedMask = 1;
	TestTrue(TEXT("the wounded mask is compared"), A != B);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsWoundRicochetPredictionTest, "GSTNParts.Core.Wounds.RicochetPredictionSeesWoundedMask", GSTNPartsTest::Flags)
bool FGSTN_PartsWoundRicochetPredictionTest::RunTest(const FString& Parameters)
{
	// The attacker predicts a ricochet by running this same step over the snapshot of the victim, whose extension carries the wounded mask
	// (the victim's fragment as the attacker has it). So the prediction follows the mask and nothing else
	FWoundStepFixture Fixture;
	FGSTN_PartHitOutcome Outcome;

	TestTrue(TEXT("a wound-free snapshot: penetration 2 on the arm (hardness 4) is a ricochet"),
		Fixture.RunStep(Fixture.MakeContext(1, 2, false), Outcome) && Outcome.HasFlag(GSTNPartOutcomeFlags::Ricochet));

	Fixture.Ext.WoundedMask = 1 << Arm;
	TestTrue(TEXT("the snapshot says the arm is wounded: the same hit no longer ricochets"),
		Fixture.RunStep(Fixture.MakeContext(1, 2, false), Outcome) && !Outcome.HasFlag(GSTNPartOutcomeFlags::Ricochet));
	TestTrue(TEXT("and lands on the arm"), Outcome.PartIndex == Arm && Outcome.NewPartDamage > 0);

	// The step reads it from the extension's state: SetState and ToState carry the mask through the POD the snapshot copies
	FGSTN_PartsState State;
	State.WoundedMask = 1 << Head;
	FGSTN_PartsSnapshotExt Copy;
	Copy.SetState(State);
	TestEqual(TEXT("the snapshot extension holds the mask"), static_cast<int32>(Copy.WoundedMask), 1 << Head);
	TestTrue(TEXT("and gives it back"), Copy.ToState() == State);

	// A different hit of the same state predicts the same on both sides: the step is a function of (hit, snapshot) only
	FGSTN_PartHitOutcome Again;
	TestTrue(TEXT("running it twice gives the same entry"),
		Fixture.RunStep(Fixture.MakeContext(1, 2, false), Outcome) && Fixture.RunStep(Fixture.MakeContext(1, 2, false), Again)
		&& Outcome.Flags == Again.Flags && Outcome.NewPartDamage == Again.NewPartDamage && Outcome.ResultWoundedMask == Again.ResultWoundedMask);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsWoundValidationTest, "GSTNParts.Core.Wounds.Validation", GSTNPartsTest::Flags)
bool FGSTN_PartsWoundValidationTest::RunTest(const FString& Parameters)
{
	auto ValidateAsFighter = [](TConstArrayView<FGSTN_PartDefinition> Parts)
	{
		FGSTN_PartsValidationReport Report;
		FGSTN_PartsValidation::ValidateParts(Parts, FGSTN_GrabGate(), FGSTN_PartReaction(), nullptr, GSTN_PartsLimits::MaxParts, Report);
		return Report;
	};

	TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	TestEqual(TEXT("the default (-1, cannot be wounded) is valid"), ValidateAsFighter(Parts).NumErrors(), 0);

	for (const int32 Good : { 0, 7, GSTN_PartsLimits::MaxHardness })
	{
		Parts[Arm].WoundedHardness = Good;
		TestEqual(*FString::Printf(TEXT("wounded hardness %d is valid"), Good), ValidateAsFighter(Parts).NumErrors(), 0);
	}
	for (const int32 Bad : { -2, GSTN_PartsLimits::MaxHardness + 1, 99 })
	{
		Parts[Arm].WoundedHardness = Bad;
		TestTrue(*FString::Printf(TEXT("wounded hardness %d is an error"), Bad), ValidateAsFighter(Parts).Contains(true, TEXT("wounded hardness")));
	}

	// Minions: the row has no wounded mask, so a set for minions cannot declare a wound
	TArray<FGSTN_PartDefinition> MinionParts = GSTNPartsTest::MakeEnemyParts();
	FGSTN_PartsValidationReport Fine;
	FGSTN_PartsValidation::ValidateForMinions(MinionParts, Fine);
	TestEqual(TEXT("a minion set without wounds is valid"), Fine.NumErrors(), 0);
	MinionParts[Head].WoundedHardness = 0;
	FGSTN_PartsValidationReport Rejected;
	FGSTN_PartsValidation::ValidateForMinions(MinionParts, Rejected);
	TestTrue(TEXT("a wounded hardness on a minion part is an error"), Rejected.Contains(true, TEXT("cannot be wounded")));
	TestEqual(TEXT("and the only one"), Rejected.NumErrors(), 1);
	TestEqual(TEXT("a fighter's validation accepts the same set"), ValidateAsFighter(MinionParts).NumErrors(), 0);

	// A state that carries a wound does not fit a minion row (the step of the bridge never sets one: it never reads the wound struct)
	FGSTN_PartsState Wounded;
	Wounded.WoundedMask = 1;
	uint8 Mask = 0;
	uint8 Damage[GSTN_PartsLimits::MaxMinionParts] = {};
	TestFalse(TEXT("ToMinionStore refuses a wound"), FGSTN_PartsSnapshotExt::ToMinionStore(Wounded, Mask, Damage));
	TestTrue(TEXT("and takes a state without one"), FGSTN_PartsSnapshotExt::ToMinionStore(FGSTN_PartsState(), Mask, Damage));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
