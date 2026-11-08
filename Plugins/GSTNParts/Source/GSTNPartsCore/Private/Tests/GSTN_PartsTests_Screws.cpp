// P19: screws on mechanical enemies (combat-parts.md 3.4, 8.1 "Screws (P19)"). No world: the math directly, and the step with a test hitbox
// that authors TurnSense (the spin and drill hitboxes will author it later).

#include "Data/GSTN_PartHitData.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Data/GSTN_PartsValidation.h"
#include "GSTN_PartsTestUtils.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HitRunner.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Misc/AutomationTest.h"
#include "Pipeline/GSTN_HitStep_PartResolve.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Pipeline/GSTN_PartsSnapshotExt.h"
#include "Rules/GSTN_CombatRules.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPartsScrewsTest
{
	constexpr int32 ScrewA = 1;
	constexpr int32 ScrewB = 2;
	constexpr int32 Plate = 3;
	constexpr int32 Cover = 4;

	/** A screw: hardness 5, a 4-turn release (64 bytes a step), loosened counter-clockwise (-1) */
	FGSTN_PartDefinition MakeScrew(FName Name, FName Hurtbox, int32 TurnsToRelease = 4, int32 LoosenSense = -1, bool bOppositeTightens = true)
	{
		FGSTN_PartDefinition Screw = GSTNPartsTest::MakePart(Name, { Hurtbox }, 5, 100.f, 0.1f);
		Screw.Screw.bEnabled = true;
		Screw.Screw.LoosenSense = LoosenSense;
		Screw.Screw.TurnsToRelease = TurnsToRelease;
		Screw.Screw.bOppositeTightens = bOppositeTightens;
		return Screw;
	}

	/** Body, two screws, an unbreakable hardness 15 plate held by both, a cover held by the plate. The plate and the cover carry a bonus each */
	TArray<FGSTN_PartDefinition> MakeParts(EGSTN_HeldByRule PlateRule = EGSTN_HeldByRule::AllBroken, float PlateCap = 0.f)
	{
		TArray<FGSTN_PartDefinition> Parts;
		Parts.Add(GSTNPartsTest::MakePart("Body", { "BodyBox" }, 0, 100.f));
		Parts.Add(MakeScrew("ScrewA", "ScrewABox"));
		Parts.Add(MakeScrew("ScrewB", "ScrewBBox"));
		FGSTN_PartDefinition PlatePart = GSTNPartsTest::MakePart("Plate", { "PlateBox" }, 15, PlateCap, 0.2f);
		PlatePart.HeldBy = { "ScrewA", "ScrewB" };
		PlatePart.HeldByRule = PlateRule;
		GSTNPartsTest::AddMainDamage(PlatePart, 7.f);
		Parts.Add(PlatePart);
		FGSTN_PartDefinition CoverPart = GSTNPartsTest::MakePart("Cover", { "CoverBox" }, 15, 0.f, 0.2f);
		CoverPart.HeldBy = { "Plate" };
		GSTNPartsTest::AddMainDamage(CoverPart, 3.f);
		Parts.Add(CoverPart);
		return Parts;
	}

	FGSTN_PartHitInput TurnHit(int32 PartIndex, int32 Sense, int32 Steps, int32 Penetration = 5)
	{
		FGSTN_PartHitInput Input = GSTNPartsTest::Hit(10.f, PartIndex, Penetration);
		Input.TurnSense = Sense;
		Input.TurnSteps = Steps;
		return Input;
	}

	FGSTN_PartHitResult Apply(TConstArrayView<FGSTN_PartDefinition> Parts, FGSTN_PartsState& State, const FGSTN_PartHitInput& Input)
	{
		return UGSTN_PartMath::ApplyHit(Parts, UGSTN_PenetrationTable::MakeDefaultBands(), State, Input);
	}

	const FGSTN_PartHitOutcome* FindOutcome(const FGSTN_StepResult& Result)
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

	/** The part resolve step over a victim whose snapshot holds the given state, with a hitbox that authors the turn: what the spin and drill hitboxes will do */
	struct FScrewFixture
	{
		TStrongObjectPtr<UGSTN_PartSetDefinition> Set;
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Hurtboxes;
		TStrongObjectPtr<UGSTN_CombatRules> Rules;

		FScrewFixture()
		{
			Set.Reset(NewObject<UGSTN_PartSetDefinition>(GetTransientPackage()));
			Set->Parts = MakeParts();
			Set->RebakeWeightCurve();

			Hurtboxes.Reset(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
			for (const TCHAR* Name : { TEXT("BodyBox"), TEXT("ScrewABox"), TEXT("ScrewBBox"), TEXT("PlateBox"), TEXT("CoverBox") })
			{
				Hurtboxes->Hurtboxes.AddDefaulted_GetRef().Name = FName(Name);
			}
			Rules.Reset(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
		}

		FGSTN_StepResult RunStep(const FGSTN_PartsSnapshotExt& Ext, int32 HurtboxIndex, int32 TurnSense, int32 TurnSteps, int32 Penetration = 5) const
		{
			FGSTN_HitResolutionContext Context;
			Context.Incoming.Hit.Damage = 10.f;
			Context.Incoming.Hit.TurnSense = TurnSense;
			Context.Incoming.Hit.TurnSteps = TurnSteps;
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
			GetDefault<UGSTN_HitStep_PartResolve>()->Resolve(View, Result);
			return Result;
		}
	};
}

using namespace GSTNPartsScrewsTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsScrewSenseTest, "GSTNParts.Core.Screws.ScrewTurnsOnlyWithLoosenSense", GSTNPartsTest::Flags)
bool FGSTN_PartsScrewSenseTest::RunTest(const FString& Parameters)
{
	// Opposite sense does not tighten here (and a fully-in screw has nowhere to go anyway), so only the loosening sense moves it
	TArray<FGSTN_PartDefinition> Parts = MakeParts();
	Parts[ScrewA].Screw.bOppositeTightens = false;
	TestEqual(TEXT("one step of four is 64 bytes"), Parts[ScrewA].Screw.StepBytes(1), 64);
	TestEqual(TEXT("steps are ceil(255 x steps / turns)"), MakeScrew("S", "H", 8).Screw.StepBytes(3), 96);

	FGSTN_PartsState State;
	Apply(Parts, State, TurnHit(ScrewA, 0, 1));
	TestEqual(TEXT("a hit with no sense turns nothing"), static_cast<int32>(State.PartDamage[ScrewA]), 0);
	Apply(Parts, State, TurnHit(ScrewA, +1, 1));
	TestEqual(TEXT("the wrong sense turns nothing when it does not tighten"), static_cast<int32>(State.PartDamage[ScrewA]), 0);
	Apply(Parts, State, TurnHit(ScrewA, -1, 0));
	TestEqual(TEXT("zero steps turn nothing"), static_cast<int32>(State.PartDamage[ScrewA]), 0);

	const FGSTN_PartHitResult First = Apply(Parts, State, TurnHit(ScrewA, -1, 1));
	TestEqual(TEXT("the loosening sense moves one step out"), static_cast<int32>(State.PartDamage[ScrewA]), 64);
	TestTrue(TEXT("and reports the byte moved"), First.bPartDamaged && First.NewPartDamage == 64 && First.NewlyBrokenMask == 0);
	Apply(Parts, State, TurnHit(ScrewA, -1, 2));
	TestEqual(TEXT("two steps in one hit are 128 more"), static_cast<int32>(State.PartDamage[ScrewA]), 192);
	TestEqual(TEXT("the other screw did not move"), static_cast<int32>(State.PartDamage[ScrewB]), 0);

	const FGSTN_PartHitResult Last = Apply(Parts, State, TurnHit(ScrewA, -1, 1));
	TestTrue(TEXT("the byte reaching 255 breaks the screw, once"), State.IsBroken(ScrewA) && (Last.NewlyBrokenMask & (1u << ScrewA)) != 0 && State.PartDamage[ScrewA] == 255);
	TestFalse(TEXT("the plate still waits for the other screw"), State.IsBroken(Plate));

	// A clockwise screw: +1 loosens, -1 does not
	Parts[ScrewB].Screw.LoosenSense = 1;
	Parts[ScrewB].Screw.bOppositeTightens = false;
	Apply(Parts, State, TurnHit(ScrewB, -1, 4));
	TestEqual(TEXT("-1 does not loosen a clockwise screw"), static_cast<int32>(State.PartDamage[ScrewB]), 0);
	Apply(Parts, State, TurnHit(ScrewB, +1, 4));
	TestTrue(TEXT("+1 does, and four steps release it"), State.IsBroken(ScrewB));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsScrewOppositeTest, "GSTNParts.Core.Screws.OppositeSenseTightens", GSTNPartsTest::Flags)
bool FGSTN_PartsScrewOppositeTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts = MakeParts();
	FGSTN_PartsState State;
	State.PartDamage[ScrewA] = 128;

	Apply(Parts, State, TurnHit(ScrewA, +1, 1));
	TestEqual(TEXT("the opposite sense moves one step back in"), static_cast<int32>(State.PartDamage[ScrewA]), 64);
	Apply(Parts, State, TurnHit(ScrewA, +1, 3));
	TestEqual(TEXT("never below fully in"), static_cast<int32>(State.PartDamage[ScrewA]), 0);
	TestFalse(TEXT("tightening never breaks anything"), State.IsBroken(ScrewA) || State.BreakSerial != 0);

	Parts[ScrewA].Screw.bOppositeTightens = false;
	State.PartDamage[ScrewA] = 128;
	Apply(Parts, State, TurnHit(ScrewA, +1, 1));
	TestEqual(TEXT("with bOppositeTightens off the byte stays"), static_cast<int32>(State.PartDamage[ScrewA]), 128);

	// A broken screw is a stump: turning no longer changes it
	Parts[ScrewA].Screw.bOppositeTightens = true;
	State.PartDamage[ScrewA] = 255;
	State.BrokenMask = static_cast<uint16>(1u << ScrewA);
	Apply(Parts, State, TurnHit(ScrewA, +1, 2));
	TestEqual(TEXT("a screw that is out stays out"), static_cast<int32>(State.PartDamage[ScrewA]), 255);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsScrewHardnessTest, "GSTNParts.Core.Screws.ScrewIgnoresHardnessAndOrdinaryDamage", GSTNPartsTest::Flags)
bool FGSTN_PartsScrewHardnessTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PartDefinition> Parts = MakeParts();
	// Penetration 5 against hardness 5 is the 65 % band: an ordinary hit would scale the part damage, a turn does not
	{
		FGSTN_PartsState State;
		const FGSTN_PartHitResult Turn = Apply(Parts, State, TurnHit(ScrewA, -1, 1, 5));
		TestEqual(TEXT("the equal band does not scale the step"), static_cast<int32>(State.PartDamage[ScrewA]), 64);
		TestFalse(TEXT("no ricochet at equal hardness"), Turn.bRicochet);
	}
	{
		FGSTN_PartsState State;
		FGSTN_PartHitInput Strong = TurnHit(ScrewA, -1, 1, 15);
		Strong.PartDamageScale = 10.f;
		Apply(Parts, State, Strong);
		TestEqual(TEXT("the hit's and the part's damage scales do not apply either"), static_cast<int32>(State.PartDamage[ScrewA]), 64);
	}
	{
		// Ordinary damage, however large, never moves the screw; the main pool still takes the band's share through MainTransfer
		FGSTN_PartsState State;
		const FGSTN_PartHitInput Heavy = GSTNPartsTest::Hit(10000.f, ScrewA, 15);
		const FGSTN_PartHitResult Result = Apply(Parts, State, Heavy);
		TestEqual(TEXT("ordinary damage leaves the screw in"), static_cast<int32>(State.PartDamage[ScrewA]), 0);
		TestTrue(TEXT("and breaks nothing"), State.BrokenMask == 0 && !Result.bPartDamaged);
		TestEqual(TEXT("the main pool still takes its share"), Result.MainDelta, 10000.f * 0.1f);
	}
	{
		// The screw's hardness still decides ricochets: a hit below it bounces and turns nothing
		FGSTN_PartsState State;
		const FGSTN_PartHitResult Bounce = Apply(Parts, State, TurnHit(ScrewA, -1, 1, 2));
		TestTrue(TEXT("a hit below the screw's hardness ricochets"), Bounce.bRicochet);
		TestEqual(TEXT("and turns nothing"), static_cast<int32>(State.PartDamage[ScrewA]), 0);

		// MainOnly leaves the part alone, a turn included
		FGSTN_PartHitInput MainOnly = TurnHit(ScrewA, -1, 1, 15);
		MainOnly.bMainOnly = true;
		Apply(Parts, State, MainOnly);
		TestEqual(TEXT("a main-only hit does not turn"), static_cast<int32>(State.PartDamage[ScrewA]), 0);

		// A hit that ignores hardness turns whatever the penetration
		FGSTN_PartHitInput Ignore = TurnHit(ScrewA, -1, 1, 0);
		Ignore.bIgnoreHardness = true;
		Apply(Parts, State, Ignore);
		TestEqual(TEXT("an ignore-hardness hit turns it"), static_cast<int32>(State.PartDamage[ScrewA]), 64);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsScrewHeldAllTest, "GSTNParts.Core.Screws.HeldByAllBrokenBreaksPlateSameFrame", GSTNPartsTest::Flags)
bool FGSTN_PartsScrewHeldAllTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts = MakeParts();
	FGSTN_PartsState State;
	State.PartDamage[ScrewA] = 192;
	State.PartDamage[ScrewB] = 192;

	const FGSTN_PartHitResult First = Apply(Parts, State, TurnHit(ScrewA, -1, 1));
	TestTrue(TEXT("one of two screws out: the plate is still on"), State.IsBroken(ScrewA) && !State.IsBroken(Plate) && !State.IsBroken(Cover));
	TestEqual(TEXT("one break, one serial step"), static_cast<int32>(State.BreakSerial), 1);
	TestEqual(TEXT("no bonus yet"), First.BonusMainDamage, 0.f);

	const FGSTN_PartHitResult Second = Apply(Parts, State, TurnHit(ScrewB, -1, 1));
	const uint16 Expected = static_cast<uint16>((1u << ScrewB) | (1u << Plate) | (1u << Cover));
	TestEqual(TEXT("the last screw and, in the same hit, the plate and the cover it held come off"), static_cast<int32>(Second.NewlyBrokenMask), static_cast<int32>(Expected));
	TestTrue(TEXT("the mask holds all of them"), State.IsBroken(Plate) && State.IsBroken(Cover) && State.PartDamage[Plate] == 255 && State.PartDamage[Cover] == 255);
	TestEqual(TEXT("the break effects of the plate and the cover ran once each"), Second.BonusMainDamage, 10.f);
	TestEqual(TEXT("one serial step for the whole hit"), static_cast<int32>(State.BreakSerial), 2);

	// AnyBroken: one holder is enough
	Parts = MakeParts(EGSTN_HeldByRule::AnyBroken);
	FGSTN_PartsState AnyState;
	AnyState.PartDamage[ScrewA] = 192;
	Apply(Parts, AnyState, TurnHit(ScrewA, -1, 1));
	TestTrue(TEXT("with AnyBroken the first screw releases the plate"), AnyState.IsBroken(Plate) && !AnyState.IsBroken(ScrewB));

	// Same data, same hits, same bytes: the pass is deterministic
	FGSTN_PartsState One;
	FGSTN_PartsState Two;
	One.PartDamage[ScrewA] = Two.PartDamage[ScrewA] = 192;
	One.PartDamage[ScrewB] = Two.PartDamage[ScrewB] = 192;
	for (FGSTN_PartsState* Run : { &One, &Two })
	{
		Apply(Parts, *Run, TurnHit(ScrewB, -1, 1));
		Apply(Parts, *Run, TurnHit(ScrewA, -1, 1));
	}
	TestTrue(TEXT("two runs end in the same state"), One == Two);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsScrewHeldIgnoresTest, "GSTNParts.Core.Screws.HeldByIgnoresHardnessAndUnbreakableCap", GSTNPartsTest::Flags)
bool FGSTN_PartsScrewHeldIgnoresTest::RunTest(const FString& Parameters)
{
	// The plate is hardness 15 with a cap of 0: no weapon breaks it, the screws do
	TArray<FGSTN_PartDefinition> Parts = MakeParts();
	TestFalse(TEXT("the plate is unbreakable by damage"), Parts[Plate].IsBreakable());
	FGSTN_PartsState State;
	Apply(Parts, State, GSTNPartsTest::Hit(100000.f, Plate, 15));
	TestTrue(TEXT("a huge hit at top penetration does not move the plate"), State.BrokenMask == 0 && State.PartDamage[Plate] == 0);

	State.PartDamage[ScrewA] = 192;
	State.PartDamage[ScrewB] = 192;
	Apply(Parts, State, TurnHit(ScrewA, -1, 1));
	Apply(Parts, State, TurnHit(ScrewB, -1, 1));
	TestTrue(TEXT("the screws take the plate off whatever its hardness and cap"), State.IsBroken(Plate));

	// Owner decision (2026-11-08): a held plate with a cap still breaks from damage, screws in or not (whichever comes first)
	Parts = MakeParts(EGSTN_HeldByRule::AllBroken, 10.f);
	FGSTN_PartsState Capped;
	const FGSTN_PartHitResult Hit = Apply(Parts, Capped, GSTNPartsTest::Hit(100000.f, Plate, 15));
	TestTrue(TEXT("damage breaks a held plate with a cap"), Capped.IsBroken(Plate) && (Hit.NewlyBrokenMask & (1u << Plate)) != 0);
	TestFalse(TEXT("the screws stay in"), Capped.IsBroken(ScrewA) || Capped.IsBroken(ScrewB));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsScrewRewoundTest, "GSTNParts.Core.Screws.RewoundTurnReadsProgressOfItsFrame", GSTNPartsTest::Flags)
bool FGSTN_PartsScrewRewoundTest::RunTest(const FString& Parameters)
{
	// The step reads the victim's part state from the snapshot of the frame it resolves against: a rewound turn starts from that frame's byte, not
	// from a later one, and writes the absolute result the applier stores
	FScrewFixture Fixture;
	const int32 ScrewABox = 1;

	FGSTN_PartsSnapshotExt Early;
	Early.PartSet = Fixture.Set.Get();
	Early.PartDamage[ScrewA] = 64;
	FGSTN_PartsSnapshotExt Late = Early;
	Late.PartDamage[ScrewA] = 192;

	const FGSTN_StepResult EarlyResult = Fixture.RunStep(Early, ScrewABox, -1, 1);
	const FGSTN_PartHitOutcome* FromEarly = FindOutcome(EarlyResult);
	TestTrue(TEXT("the early frame's turn lands on the screw"), FromEarly && FromEarly->PartIndex == ScrewA);
	TestEqual(TEXT("it adds one step to the byte of its frame"), FromEarly ? static_cast<int32>(FromEarly->ResultPartDamage[ScrewA]) : -1, 128);

	const FGSTN_StepResult LateResult = Fixture.RunStep(Late, ScrewABox, -1, 1);
	const FGSTN_PartHitOutcome* FromLate = FindOutcome(LateResult);
	TestTrue(TEXT("the late frame's turn breaks the screw"), FromLate && (FromLate->NewlyBrokenMask & (1u << ScrewA)) != 0 && FromLate->ResultPartDamage[ScrewA] == 255);

	// Resolving the early frame again after the late one gives the early answer: the step keeps nothing between hits
	const FGSTN_StepResult AgainResult = Fixture.RunStep(Early, ScrewABox, -1, 1);
	const FGSTN_PartHitOutcome* Again = FindOutcome(AgainResult);
	TestEqual(TEXT("same frame state, same result"), Again ? static_cast<int32>(Again->ResultPartDamage[ScrewA]) : -1, 128);

	// The hitbox's sense decides: no sense, no turn through the step either
	const FGSTN_StepResult NoSense = Fixture.RunStep(Early, ScrewABox, 0, 1);
	const FGSTN_PartHitOutcome* NoSenseOutcome = FindOutcome(NoSense);
	TestEqual(TEXT("a hitbox without TurnSense leaves the byte of its frame"), NoSenseOutcome ? static_cast<int32>(NoSenseOutcome->ResultPartDamage[ScrewA]) : -1, 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsScrewValidationTest, "GSTNParts.Core.Screws.ValidationRejectsBadScrewAndHeldBy", GSTNPartsTest::Flags)
bool FGSTN_PartsScrewValidationTest::RunTest(const FString& Parameters)
{
	auto Check = [](TConstArrayView<FGSTN_PartDefinition> Parts)
	{
		FGSTN_PartsValidationReport Report;
		FGSTN_PartsValidation::ValidateParts(Parts, FGSTN_GrabGate(), FGSTN_PartReaction(), nullptr, GSTN_PartsLimits::MaxParts, Report);
		return Report;
	};

	TestFalse(TEXT("the sample set is clean"), Check(MakeParts()).HasErrors());

	TArray<FGSTN_PartDefinition> Parts = MakeParts();
	Parts[ScrewA].Screw.LoosenSense = 0;
	TestTrue(TEXT("loosen sense 0 is rejected"), Check(Parts).Contains(true, TEXT("loosen sense")));
	Parts = MakeParts();
	Parts[ScrewA].Screw.TurnsToRelease = 0;
	TestTrue(TEXT("turns to release 0 is rejected"), Check(Parts).Contains(true, TEXT("turns to release")));
	Parts[ScrewA].Screw.TurnsToRelease = 256;
	TestTrue(TEXT("turns to release 256 is rejected"), Check(Parts).Contains(true, TEXT("turns to release")));

	Parts = MakeParts();
	Parts[Plate].HeldBy.Add("Nope");
	TestTrue(TEXT("an unknown holder is rejected"), Check(Parts).Contains(true, TEXT("not a part of the set")));
	Parts = MakeParts();
	Parts[Plate].HeldBy.Add("Plate");
	TestTrue(TEXT("a part held by itself is rejected"), Check(Parts).Contains(true, TEXT("held by itself")));

	Parts = MakeParts();
	Parts[ScrewA].HeldBy = { "Cover" }; // Cover is held by Plate, Plate by ScrewA: a loop
	TestTrue(TEXT("a HeldBy loop is rejected"), Check(Parts).Contains(true, TEXT("cycle through HeldBy")));
	Parts = MakeParts();
	Parts[ScrewA].ParentIndex = Plate; // the plate waits for its screw, and the screw (its child) breaks only after the plate
	TestTrue(TEXT("a loop through a parent is rejected"), Check(Parts).Contains(true, TEXT("cycle through HeldBy")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
