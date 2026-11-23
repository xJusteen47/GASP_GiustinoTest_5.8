// Automation tests of the minion-ready pure pieces (plan P15): the same step on a store-shaped extension, the 8 part limit, the
// Stats rejection, the snapshot builder and the lethality rule. No GSTNMinion code: the "store" is a byte and an 8 byte array.

#include "Data/GSTN_PartHitData.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Data/GSTN_PartsMinionExtension.h"
#include "Data/GSTN_PartsValidation.h"
#include "GSTN_PartsTestUtils.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HitRunner.h"
#include "Hits/GSTN_HitSteps.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Misc/AutomationTest.h"
#include "Pipeline/GSTN_HitStep_PartResolve.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Pipeline/GSTN_PartsSnapshotExt.h"
#include "Rules/GSTN_CombatRules.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A fake minion store row: what P16 keeps per part-bearing minion, 9 bytes */
	struct FFakeStoreRow
	{
		uint8 BrokenMask = 0;
		uint8 PartDamage[GSTN_PartsLimits::MaxMinionParts] = {};
	};

	TStrongObjectPtr<UGSTN_PartSetDefinition> MakeMinionSet()
	{
		TStrongObjectPtr<UGSTN_PartSetDefinition> Set(NewObject<UGSTN_PartSetDefinition>(GetTransientPackage()));
		Set->Parts = GSTNPartsTest::MakeEnemyParts();
		Set->bForMinions = true;
		Set->RebakeWeightCurve();
		return Set;
	}

	/** The fighter limit pass, as a fighter's set gets it */
	FGSTN_PartsValidationReport ValidateAsFighter(const TArray<FGSTN_PartDefinition>& Parts)
	{
		FGSTN_PartsValidationReport Report;
		FGSTN_PartsValidation::ValidateParts(Parts, FGSTN_GrabGate(), FGSTN_PartReaction(), nullptr, GSTN_PartsLimits::MaxParts, Report);
		return Report;
	}

	const FGSTN_PartHitOutcome* FindMinionOutcome(const FGSTN_StepResult& Result)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionStepTest, "GSTNParts.Core.Minion.StepSameOutputAsFighter", GSTNPartsTest::Flags)
bool FGSTN_PartsMinionStepTest::RunTest(const FString& Parameters)
{
	// The same step on a fighter's extension (state copy) and on one built from store arrays gives the same outcome, for every part
	TStrongObjectPtr<UGSTN_PartSetDefinition> Set = MakeMinionSet();
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Hurtboxes(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
	for (const TCHAR* Name : { TEXT("TorsoBox"), TEXT("ArmBox"), TEXT("HandBox"), TEXT("HeadBox") })
	{
		Hurtboxes->Hurtboxes.AddDefaulted_GetRef().Name = FName(Name);
	}
	TStrongObjectPtr<UGSTN_CombatRules> Rules(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
	Rules->HitPipeline.Add(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_PartResolve::StaticClass()));
	Rules->ResolveHitPipeline();

	FFakeStoreRow Row;
	Row.BrokenMask = 0b0100;
	Row.PartDamage[1] = 120;
	Row.PartDamage[3] = 200;

	FGSTN_PartsState State;
	State.BrokenMask = Row.BrokenMask;
	FMemory::Memcpy(State.PartDamage, Row.PartDamage, sizeof(Row.PartDamage));
	FGSTN_PartsSnapshotExt FighterExt;
	FighterExt.PartSet = Set.Get();
	FighterExt.SetState(State);
	const FGSTN_PartsSnapshotExt MinionExt = FGSTN_PartsSnapshotExt::FromMinionStore(Set.Get(), Row.BrokenMask, Row.PartDamage);

	for (int32 Box = 0; Box < 4; ++Box)
	{
		FGSTN_StepResult Results[2];
		const FGSTN_PartsSnapshotExt* Exts[2] = { &FighterExt, &MinionExt };
		for (int32 Which = 0; Which < 2; ++Which)
		{
			FGSTN_HitResolutionContext Context;
			Context.Incoming.Hit.Damage = 25.f;
			Context.Incoming.Hit.BaseKnockback = 300.f;
			FGSTN_PartHitData PartHit;
			PartHit.Penetration = 5;
			Context.Incoming.Hit.ExtensionData.Add(FInstancedStruct::Make(PartHit));
			Context.Victim.Weight = 100.f;
			Context.Victim.SnapshotExtension = Exts[Which];
			Context.HurtboxSet = Hurtboxes.Get();
			Context.HurtboxIndex = static_cast<uint8>(Box);
			FGSTN_HitContextView View;
			FGSTN_HitPipelineRunner::MakeView(Rules.Get(), Context, View);
			GetDefault<UGSTN_HitStep_PartResolve>()->Resolve(View, Results[Which]);
		}
		const FGSTN_PartHitOutcome* A = FindMinionOutcome(Results[0]);
		const FGSTN_PartHitOutcome* B = FindMinionOutcome(Results[1]);
		if (!TestTrue(*FString::Printf(TEXT("box %d: both produce an outcome"), Box), A && B))
		{
			continue;
		}
		TestEqual(*FString::Printf(TEXT("box %d: mask"), Box), B->ResultBrokenMask, A->ResultBrokenMask);
		TestTrue(*FString::Printf(TEXT("box %d: damage bytes"), Box), FMemory::Memcmp(A->ResultPartDamage, B->ResultPartDamage, sizeof(A->ResultPartDamage)) == 0);
		TestEqual(*FString::Printf(TEXT("box %d: main delta"), Box), B->MainDelta, A->MainDelta);
		TestEqual(*FString::Printf(TEXT("box %d: flags"), Box), B->Flags, A->Flags);
		TestEqual(*FString::Printf(TEXT("box %d: weight scale"), Box), Results[1].WeightScale, Results[0].WeightScale);
		TestEqual(*FString::Printf(TEXT("box %d: stagger threshold"), Box), Results[1].StaggerThreshold.Value, Results[0].StaggerThreshold.Value);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionMaskOver8Test, "GSTNParts.Core.Minion.MaskOver8Rejected", GSTNPartsTest::Flags)
bool FGSTN_PartsMinionMaskOver8Test::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Parts;
	for (int32 Index = 0; Index < GSTN_PartsLimits::MaxMinionParts; ++Index)
	{
		Parts.Add(GSTNPartsTest::MakePart(*FString::Printf(TEXT("Part%d"), Index), { *FString::Printf(TEXT("Box%d"), Index) }, 0, 10.f));
	}
	FGSTN_PartsValidationReport Report;
	FGSTN_PartsValidation::ValidateForMinions(Parts, Report);
	TestEqual(TEXT("Eight parts fill the minion mask exactly and are valid"), Report.NumErrors(), 0);

	Parts.Add(GSTNPartsTest::MakePart("Part8", { "Box8" }, 0, 10.f));
	FGSTN_PartsValidationReport Over;
	FGSTN_PartsValidation::ValidateForMinions(Parts, Over);
	TestTrue(TEXT("Nine parts are an error for a minion"), Over.Contains(true, TEXT("limit is 8")));
	TestEqual(TEXT("Nine parts are fine for a fighter"), ValidateAsFighter(Parts).NumErrors(), 0);

	TStrongObjectPtr<UGSTN_PartSetDefinition> Set(NewObject<UGSTN_PartSetDefinition>(GetTransientPackage()));
	TestFalse(TEXT("A set is not for minions by default"), Set->bForMinions);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionStatsTest, "GSTNParts.Core.Minion.StatsEffectRejectedOrConverted", GSTNPartsTest::Flags)
bool FGSTN_PartsMinionStatsTest::RunTest(const FString& Parameters)
{
	// The chosen rule is "rejected": a Stats effect is an error on a minion set, the other one-shots and the derived effects are fine
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	FGSTN_PartBreakEffect_MainDamage MainDamage;
	MainDamage.AddPercent = 10.f;
	FGSTN_PartBreakEffect_Launch Launch;
	FGSTN_PartBreakEffect_Stagger Stagger;
	FGSTN_PartBreakEffect_Tags Tags;
	Parts[1].OnBreak.Add(FInstancedStruct::Make(MainDamage));
	Parts[1].OnBreak.Add(FInstancedStruct::Make(Launch));
	Parts[1].OnBreak.Add(FInstancedStruct::Make(Stagger));
	Parts[1].OnBreak.Add(FInstancedStruct::Make(Tags));
	FGSTN_PartsValidationReport Fine;
	FGSTN_PartsValidation::ValidateForMinions(Parts, Fine);
	TestEqual(TEXT("MainDamage, Launch, Stagger and Tags run on a minion"), Fine.NumErrors(), 0);
	for (const FInstancedStruct& Effect : Parts[1].OnBreak)
	{
		TestTrue(TEXT("A minion can run it"), FGSTN_PartsMinionRules::CanMinionRunBreakEffect(Effect));
	}

	FGSTN_PartBreakEffect_Stats Stats;
	Parts[3].OnBreak.Add(FInstancedStruct::Make(Stats));
	TestFalse(TEXT("A minion cannot run a Stats effect"), FGSTN_PartsMinionRules::CanMinionRunBreakEffect(Parts[3].OnBreak[0]));
	FGSTN_PartsValidationReport Rejected;
	FGSTN_PartsValidation::ValidateForMinions(Parts, Rejected);
	TestTrue(TEXT("The Stats effect is an error"), Rejected.Contains(true, TEXT("Stats effect")));
	TestEqual(TEXT("It is the only error"), Rejected.NumErrors(), 1);
	TestEqual(TEXT("A fighter's validation does not reject it"), ValidateAsFighter(Parts).NumErrors(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionSnapshotTest, "GSTNParts.Core.Minion.SnapshotExtFromStore", GSTNPartsTest::Flags)
bool FGSTN_PartsMinionSnapshotTest::RunTest(const FString& Parameters)
{
	static_assert(sizeof(FFakeStoreRow) == 9, "9 B per part-bearing minion");
	TStrongObjectPtr<UGSTN_PartSetDefinition> Set = MakeMinionSet();
	FFakeStoreRow Row;
	Row.BrokenMask = 0b1010;
	for (int32 Index = 0; Index < GSTN_PartsLimits::MaxMinionParts; ++Index)
	{
		Row.PartDamage[Index] = static_cast<uint8>(10 * Index + 1);
	}

	const FGSTN_PartsSnapshotExt Ext = FGSTN_PartsSnapshotExt::FromMinionStore(Set.Get(), Row.BrokenMask, Row.PartDamage);
	TestEqual(TEXT("Type word"), Ext.Type, FGSTN_PartsSnapshotExt::TypeWord);
	TestTrue(TEXT("Set"), Ext.PartSet == Set.Get());
	TestEqual(TEXT("Mask"), static_cast<int32>(Ext.BrokenMask), 0b1010);
	TestEqual(TEXT("Serial is zero"), static_cast<int32>(Ext.BreakSerial), 0);
	for (int32 Index = 0; Index < GSTN_PartsLimits::MaxParts; ++Index)
	{
		const int32 Expected = Index < GSTN_PartsLimits::MaxMinionParts ? Row.PartDamage[Index] : 0;
		TestEqual(*FString::Printf(TEXT("Damage %d"), Index), static_cast<int32>(Ext.PartDamage[Index]), Expected);
	}

	// Null damage means pristine
	const FGSTN_PartsSnapshotExt Pristine = FGSTN_PartsSnapshotExt::FromMinionStore(Set.Get(), 0, nullptr);
	TestTrue(TEXT("Pristine state"), Pristine.ToState() == FGSTN_PartsState());

	// Round trip through the state, and the state that does not fit
	uint8 Mask = 0;
	uint8 Damage[GSTN_PartsLimits::MaxMinionParts] = {};
	TestTrue(TEXT("The extension's state fits a minion"), FGSTN_PartsSnapshotExt::ToMinionStore(Ext.ToState(), Mask, Damage));
	TestEqual(TEXT("Round trip mask"), Mask, Row.BrokenMask);
	TestTrue(TEXT("Round trip damage"), FMemory::Memcmp(Damage, Row.PartDamage, sizeof(Damage)) == 0);
	FGSTN_PartsState Wide = Ext.ToState();
	Wide.BrokenMask |= 1u << 9;
	TestFalse(TEXT("A part past the eighth does not fit"), FGSTN_PartsSnapshotExt::ToMinionStore(Wide, Mask, Damage));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsMinionLethalityTest, "GSTNParts.Core.Minion.KeepsFightingAfterHeadLoss", GSTNPartsTest::Flags)
bool FGSTN_PartsMinionLethalityTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts(); // Torso 0, Arm 1, Hand 2, Head 3
	constexpr uint8 HeadBit = 1 << 3;
	constexpr uint8 TorsoBit = 1 << 0;

	FGSTN_PartsMinionExtension Extension;
	const uint8 DefaultMask = Extension.ResolveLethalMask(Parts);
	TestEqual(TEXT("By default no part loss is lethal"), static_cast<int32>(DefaultMask), 0);
	TestFalse(TEXT("The head lost: keeps fighting"), FGSTN_PartsMinionExtension::IsLethal(HeadBit, DefaultMask));
	TestFalse(TEXT("Every part lost: still keeps fighting by default"), FGSTN_PartsMinionExtension::IsLethal(0xFF, DefaultMask));

	Extension.LethalParts = { "Torso", "Missing" };
	const uint8 Mask = Extension.ResolveLethalMask(Parts);
	TestEqual(TEXT("The listed part is lethal, an unknown name is skipped"), static_cast<int32>(Mask), static_cast<int32>(TorsoBit));
	TestFalse(TEXT("Head lost with a lethal torso: keeps fighting"), FGSTN_PartsMinionExtension::IsLethal(HeadBit, Mask));
	TestTrue(TEXT("Torso lost: dies"), FGSTN_PartsMinionExtension::IsLethal(TorsoBit | HeadBit, Mask));

	FGSTN_PartsValidationReport Report;
	Extension.Validate(Parts, Report);
	TestTrue(TEXT("The unknown lethal name is reported"), Report.Contains(true, TEXT("Missing")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
