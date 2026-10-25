// Automation tests of the part resolve step (plan P4): the step alone, and through the combat plugin's pipeline runner with the
// weight driven knockback model, so the stagger test and the reaction tier are the real ones.

#include "Data/GSTN_PartHitData.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "GSTN_PartsTestUtils.h"
#include "Hits/GSTN_HitRunner.h"
#include "Hits/GSTN_HitSteps.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hits/GSTN_KnockbackModels.h"
#include "Misc/AutomationTest.h"
#include "Pipeline/GSTN_HitStep_PartResolve.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Pipeline/GSTN_PartsPipelineValidator.h"
#include "Pipeline/GSTN_PartsSnapshotExt.h"
#include "Rules/GSTN_CombatRules.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Hurtbox indices of the fixture: the four parts' boxes in the enemy's order, then a box no part names (the body) */
	constexpr int32 TorsoBox = 0;
	constexpr int32 ArmBox = 1;
	constexpr int32 HandBox = 2;
	constexpr int32 HeadBox = 3;
	constexpr int32 BodyBox = 4;

	struct FStepFixture
	{
		TStrongObjectPtr<UGSTN_PartSetDefinition> Set;
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Hurtboxes;
		TStrongObjectPtr<UGSTN_CombatRules> Rules;
		TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model;
		FGSTN_PartsSnapshotExt Ext;

		FStepFixture()
		{
			Set.Reset(NewObject<UGSTN_PartSetDefinition>(GetTransientPackage()));
			Set->Parts = GSTNPartsTest::MakeEnemyParts();
			Set->Parts[0].Reaction.StaggerThreshold = 40.f;                                    // torso
			Set->Parts[1].Reaction.StaggerThreshold = 100.f;                                   // armoured arm
			Set->Parts[1].Reaction.MaxReaction = EGSTN_PartReactionTier::PushBack;
			Set->Parts[1].LostMassFraction = 0.3f;
			Set->Parts[2].Reaction.StaggerThreshold = -1.f;                                    // hand inherits the body's
			Set->Parts[3].Reaction.StaggerThreshold = 5.f;                                     // weak point
			Set->Parts[3].Reaction.HitstopScale = 1.5f;
			Set->BodyReaction.StaggerThreshold = 20.f;
			Set->RebakeWeightCurve();

			Hurtboxes.Reset(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
			for (const TCHAR* Name : { TEXT("TorsoBox"), TEXT("ArmBox"), TEXT("HandBox"), TEXT("HeadBox"), TEXT("BodyBox") })
			{
				FGSTN_Hurtbox& Box = Hurtboxes->Hurtboxes.AddDefaulted_GetRef();
				Box.Name = FName(Name);
			}

			Model.Reset(NewObject<UGSTN_KnockbackModel_WeightDriven>(GetTransientPackage()));
			Model->Tuning.StaggerPerDamage = 0.f;
			Model->Tuning.StaggerPerKnockback = 0.f;
			Model->Tuning.PushBackThreshold = 0.f;
			Model->Tuning.LaunchThreshold = 0.f;

			Rules.Reset(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
			Rules->KnockbackModel = Model.Get();
			Rules->HitPipeline.Add(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_PartResolve::StaticClass()));
			Rules->HitPipeline.Add(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_ComputeKnockback::StaticClass()));
			Rules->HitPipeline.Add(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_Hitstop::StaticClass()));
			Rules->ResolveHitPipeline();

			Ext.PartSet = Set.Get();
		}

		/** A hit of `Damage` with an authored stagger force on a hurtbox of the fixture, the victim carrying the extension */
		FGSTN_HitResolutionContext MakeContext(int32 HurtboxIndex, float Damage, float StaggerForce, int32 Penetration = 1) const
		{
			FGSTN_HitResolutionContext Context;
			Context.Incoming.Hit.Damage = Damage;
			Context.Incoming.Hit.BaseKnockback = 300.f;
			Context.Incoming.Hit.StaggerForceMode = EGSTN_StaggerForceMode::Override;
			Context.Incoming.Hit.StaggerForce = StaggerForce;
			FGSTN_PartHitData PartHit;
			PartHit.Penetration = static_cast<uint8>(Penetration);
			Context.Incoming.Hit.ExtensionData.Add(FInstancedStruct::Make(PartHit));
			Context.Victim.Weight = 100.f;
			Context.Victim.SnapshotExtension = &Ext;
			Context.HurtboxSet = Hurtboxes.Get();
			Context.HurtboxIndex = static_cast<uint8>(HurtboxIndex);
			return Context;
		}

		/** The step alone on a context: the result it writes */
		FGSTN_StepResult RunStep(const FGSTN_HitResolutionContext& Context, EGSTN_StepResult* OutVerdict = nullptr) const
		{
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

	bool SameResult(const FGSTN_StepResult& A, const FGSTN_StepResult& B)
	{
		const FGSTN_PartHitOutcome* OutcomeA = FindOutcome(A);
		const FGSTN_PartHitOutcome* OutcomeB = FindOutcome(B);
		const bool bSameOutcome = (OutcomeA == nullptr) == (OutcomeB == nullptr)
			&& (!OutcomeA || (OutcomeA->PartIndex == OutcomeB->PartIndex && OutcomeA->ResultBrokenMask == OutcomeB->ResultBrokenMask
				&& OutcomeA->NewPartDamage == OutcomeB->NewPartDamage && OutcomeA->MainDelta == OutcomeB->MainDelta && OutcomeA->Flags == OutcomeB->Flags));
		return bSameOutcome && A.WeightScale == B.WeightScale && A.KnockbackScale == B.KnockbackScale && A.HitstopScale == B.HitstopScale
			&& A.StaggerScale == B.StaggerScale && A.StaggerThreshold.Value == B.StaggerThreshold.Value && A.MaxReaction == B.MaxReaction
			&& A.DamageAdd == B.DamageAdd && A.Requests.Num() == B.Requests.Num() && A.bHit == B.bHit;
	}

	/** The pipeline of the fixture over a context (PartResolve, ComputeKnockback, Hitstop) */
	FGSTN_HitRunResult Run(const FStepFixture& Fixture, FGSTN_HitResolutionContext& Context)
	{
		return FGSTN_HitPipelineRunner::Run(*Fixture.Rules, Context);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsStepViewsTest, "GSTNParts.Core.Step.FighterViewAndMinionViewSameOutput", GSTNPartsTest::Flags)
bool FGSTN_PartsStepViewsTest::RunTest(const FString& Parameters)
{
	// The step reads the victim through the view and the extension only: a fighter's view and a minion's (no proxy id, other
	// team, other position, nothing but the extension filled in) give the same result
	FStepFixture Fixture;
	Fixture.Ext.BrokenMask = 0b0100;
	Fixture.Ext.PartDamage[1] = 120;

	FGSTN_HitResolutionContext Fighter = Fixture.MakeContext(ArmBox, 10.f, 30.f, 5);
	Fighter.Victim.EntityId = 7;
	Fighter.Victim.TeamId = 1;
	Fighter.Victim.Location = FVector(100.0, 200.0, 0.0);

	FGSTN_HitResolutionContext Minion = Fixture.MakeContext(ArmBox, 10.f, 30.f, 5);
	Minion.Victim.EntityId = INDEX_NONE;
	Minion.Victim.TeamId = 255;

	TestTrue(TEXT("same result for a fighter's view and a minion's"), SameResult(Fixture.RunStep(Fighter), Fixture.RunStep(Minion)));
	TestNotNull(TEXT("an outcome was produced"), FindOutcome(Fixture.RunStep(Fighter)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsStepBlockedTest, "GSTNParts.Core.Step.BlockedHitDamagesNoPart", GSTNPartsTest::Flags)
bool FGSTN_PartsStepBlockedTest::RunTest(const FString& Parameters)
{
	FStepFixture Fixture;
	const struct { const TCHAR* Name; bool bBlocked; bool bParried; bool bNoDamage; bool bHit; } Cases[] = {
		{ TEXT("blocked"), true, false, false, true }, { TEXT("parried"), false, true, false, true },
		{ TEXT("invincible"), false, false, true, true }, { TEXT("did not land"), false, false, false, false } };
	for (const auto& Case : Cases)
	{
		FGSTN_HitResolutionContext Context = Fixture.MakeContext(TorsoBox, 10.f, 30.f);
		Context.Outcome.bBlocked = Case.bBlocked;
		Context.Outcome.bParried = Case.bParried;
		Context.bNoDamage = Case.bNoDamage;
		Context.Outcome.bHit = Case.bHit;
		const FGSTN_StepResult Result = Fixture.RunStep(Context);
		TestNull(FString::Printf(TEXT("%s: no part outcome"), Case.Name), FindOutcome(Result));
		TestEqual(FString::Printf(TEXT("%s: no damage"), Case.Name), Result.DamageAdd, 0.f);
		TestTrue(FString::Printf(TEXT("%s: no request"), Case.Name), Result.Requests.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsStepIdentityTest, "GSTNParts.Core.Step.IdentityWithoutPartSet", GSTNPartsTest::Flags)
bool FGSTN_PartsStepIdentityTest::RunTest(const FString& Parameters)
{
	// An entity without a part set gets exactly what ApplyDamage gives, and no modifier
	FStepFixture Fixture;
	FGSTN_HitResolutionContext Context = Fixture.MakeContext(TorsoBox, 10.f, 30.f);
	Context.Victim.SnapshotExtension = nullptr;
	Context.Incoming.Hit.ExtraEffects.Add(TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/Test/Effect.Effect_C"))));

	FGSTN_HitContextView View;
	FGSTN_HitPipelineRunner::MakeView(Fixture.Rules.Get(), Context, View);
	FGSTN_StepResult Parts;
	FGSTN_StepResult Plain;
	GetDefault<UGSTN_HitStep_PartResolve>()->Resolve(View, Parts);
	GetDefault<UGSTN_HitStep_ApplyDamage>()->Resolve(View, Plain);

	TestEqual(TEXT("same damage"), Parts.DamageAdd, Plain.DamageAdd);
	TestEqual(TEXT("one request each"), Parts.Requests.Num(), Plain.Requests.Num());
	if (Parts.Requests.Num() == 1 && Plain.Requests.Num() == 1)
	{
		TestEqual(TEXT("same magnitude"), Parts.Requests[0].Magnitude, Plain.Requests[0].Magnitude);
		TestTrue(TEXT("same kind and target"), Parts.Requests[0].Kind == Plain.Requests[0].Kind && Parts.Requests[0].Target == Plain.Requests[0].Target);
	}
	TestEqual(TEXT("same extra effects"), Parts.Effects.Num(), Plain.Effects.Num());
	TestTrue(TEXT("no part outcome"), Parts.OutcomeExtensions.IsEmpty());
	TestTrue(TEXT("no modifier"), Parts.WeightScale == 1.f && Parts.KnockbackScale == 1.f && Parts.HitstopScale == 1.f && Parts.StaggerScale == 1.f && !Parts.StaggerThreshold.IsSet());
	TestTrue(TEXT("the pipeline goes on"), Parts.bHit);

	// A snapshot extension with no part set is the same thing
	FGSTN_PartsSnapshotExt Empty;
	Context.Victim.SnapshotExtension = &Empty;
	FGSTN_HitPipelineRunner::MakeView(Fixture.Rules.Get(), Context, View);
	FGSTN_StepResult WithEmptyExt;
	GetDefault<UGSTN_HitStep_PartResolve>()->Resolve(View, WithEmptyExt);
	TestEqual(TEXT("an extension without a set damages like the body"), WithEmptyExt.DamageAdd, Plain.DamageAdd);
	TestTrue(TEXT("and has no outcome"), WithEmptyExt.OutcomeExtensions.IsEmpty());

	// A pointer that is not a parts extension is ignored
	uint32 Junk[4] = { 1, 2, 3, 4 };
	Context.Victim.SnapshotExtension = Junk;
	FGSTN_HitPipelineRunner::MakeView(Fixture.Rules.Get(), Context, View);
	FGSTN_StepResult WithJunk;
	GetDefault<UGSTN_HitStep_PartResolve>()->Resolve(View, WithJunk);
	TestEqual(TEXT("a foreign extension is not read"), WithJunk.DamageAdd, Plain.DamageAdd);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsStepStaggerTest, "GSTNParts.Core.Step.StaggerThresholdEqualStaggersOneBelowDoesNot", GSTNPartsTest::Flags)
bool FGSTN_PartsStepStaggerTest::RunTest(const FString& Parameters)
{
	// The torso's threshold is 40 and the band above leaves the stagger force alone: the hit staggers at 40 and not at 39
	FStepFixture Fixture;
	{
		FGSTN_HitResolutionContext Equal = Fixture.MakeContext(TorsoBox, 10.f, 40.f);
		Run(Fixture, Equal);
		TestTrue(TEXT("the knockback ran"), Equal.Outcome.bHasKnockback);
		TestTrue(TEXT("force equal to the threshold staggers"), Equal.Outcome.Knockback.bStaggered);
		TestEqual(TEXT("the part's threshold reached the model"), Equal.Reaction.StaggerThreshold.Value, 40.f);

		FGSTN_HitResolutionContext Below = Fixture.MakeContext(TorsoBox, 10.f, 39.f);
		Run(Fixture, Below);
		TestFalse(TEXT("one below does not"), Below.Outcome.Knockback.bStaggered);
		TestTrue(TEXT("a flinch still has its hitstop"), Below.Outcome.Knockback.HitstopFrames > 0);
	}

	// The band's stagger scale multiplies the force before the test: a table whose "equal" band halves it needs 80
	{
		TStrongObjectPtr<UGSTN_PenetrationTable> Table(NewObject<UGSTN_PenetrationTable>(GetTransientPackage()));
		Table->Bands = UGSTN_PenetrationTable::MakeDefaultBands();
		for (FGSTN_PenetrationBand& Band : Table->Bands)
		{
			if (Band.MinDelta == 0)
			{
				Band.StaggerScale = 0.5f;
			}
		}
		Table->Normalize();
		Fixture.Set->Table = Table.Get();
		FGSTN_HitResolutionContext Halved = Fixture.MakeContext(TorsoBox, 10.f, 60.f, 0);   // penetration 0 against hardness 0: the equal band
		Run(Fixture, Halved);
		TestFalse(TEXT("60 x 0.5 = 30 is below 40"), Halved.Outcome.Knockback.bStaggered);
		FGSTN_HitResolutionContext Enough = Fixture.MakeContext(TorsoBox, 10.f, 80.f, 0);
		Run(Fixture, Enough);
		TestTrue(TEXT("80 x 0.5 = 40 staggers"), Enough.Outcome.Knockback.bStaggered);
		Fixture.Set->Table.Reset();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsStepFallbackTest, "GSTNParts.Core.Step.PartBodyRulesFallback", GSTNPartsTest::Flags)
bool FGSTN_PartsStepFallbackTest::RunTest(const FString& Parameters)
{
	FStepFixture Fixture;

	// The part's own threshold, else the body's, else the rules' (here left unset by the step)
	FGSTN_StepResult Torso = Fixture.RunStep(Fixture.MakeContext(TorsoBox, 10.f, 30.f));
	TestEqual(TEXT("the part's threshold"), Torso.StaggerThreshold.Value, 40.f);
	FGSTN_StepResult Hand = Fixture.RunStep(Fixture.MakeContext(HandBox, 10.f, 30.f, 5));
	TestEqual(TEXT("a part that inherits takes the body's"), Hand.StaggerThreshold.Value, 20.f);
	FGSTN_StepResult Body = Fixture.RunStep(Fixture.MakeContext(BodyBox, 10.f, 30.f));
	TestEqual(TEXT("a hurtbox no part names takes the body's"), Body.StaggerThreshold.Value, 20.f);
	const FGSTN_PartHitOutcome* BodyOutcome = FindOutcome(Body);
	TestTrue(TEXT("and resolves on no part"), BodyOutcome && BodyOutcome->PartIndex == INDEX_NONE);
	TestEqual(TEXT("the body takes the whole hit"), Body.DamageAdd, 10.f);

	Fixture.Set->BodyReaction.StaggerThreshold = -1.f;
	FGSTN_StepResult Unset = Fixture.RunStep(Fixture.MakeContext(HandBox, 10.f, 30.f, 5));
	TestFalse(TEXT("nothing set: unset, the rules' global value applies"), Unset.StaggerThreshold.IsSet());

	// A hurtbox index the set does not have is a body hit as well
	FGSTN_StepResult OutOfRange = Fixture.RunStep(Fixture.MakeContext(200, 10.f, 30.f));
	TestEqual(TEXT("an unknown hurtbox is the body"), OutOfRange.DamageAdd, 10.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsStepWeakPointTest, "GSTNParts.Core.Step.WeakPointStaggersWhereArmourDoesNot", GSTNPartsTest::Flags)
bool FGSTN_PartsStepWeakPointTest::RunTest(const FString& Parameters)
{
	// The same blow (stagger force 30): the weak point (threshold 5) staggers, the armoured arm (threshold 100) does not, and the weak point freezes longer
	FStepFixture Fixture;
	FGSTN_HitResolutionContext Head = Fixture.MakeContext(HeadBox, 10.f, 30.f, 5);
	FGSTN_HitResolutionContext Arm = Fixture.MakeContext(ArmBox, 10.f, 30.f, 5);
	Run(Fixture, Head);
	Run(Fixture, Arm);
	TestTrue(TEXT("the weak point staggers"), Head.Outcome.Knockback.bStaggered);
	TestFalse(TEXT("the armoured arm does not"), Arm.Outcome.Knockback.bStaggered);
	TestTrue(TEXT("the weak point freezes longer"), Head.Outcome.Knockback.HitstopFrames > Arm.Outcome.Knockback.HitstopFrames);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsStepRicochetTest, "GSTNParts.Core.Step.RicochetBandNoStaggerNoHitstop", GSTNPartsTest::Flags)
bool FGSTN_PartsStepRicochetTest::RunTest(const FString& Parameters)
{
	// Penetration 0 against the arm's hardness 4: the ricochet band. Nothing lands, nothing freezes, the pipeline ends
	FStepFixture Fixture;
	FGSTN_HitResolutionContext Context = Fixture.MakeContext(ArmBox, 10.f, 500.f, 0);
	const FGSTN_HitRunResult Run = FGSTN_HitPipelineRunner::Run(*Fixture.Rules, Context);
	TestTrue(TEXT("the pipeline stopped at the part step"), Run.bStopped && Run.StoppedAtStep == 0);
	TestFalse(TEXT("the hit did not land"), Context.Outcome.bHit);
	TestEqual(TEXT("no damage"), Context.Outcome.Damage, 0.f);
	TestTrue(TEXT("no request, so no freeze on either side"), Context.Outcome.Requests.IsEmpty());
	TestFalse(TEXT("no knockback, no stagger"), Context.Outcome.bHasKnockback);

	const FGSTN_StepResult Result = Fixture.RunStep(Fixture.MakeContext(ArmBox, 10.f, 500.f, 0));
	const FGSTN_PartHitOutcome* Outcome = FindOutcome(Result);
	TestTrue(TEXT("the entry says ricochet and changes nothing"), Outcome && Outcome->HasFlag(GSTNPartOutcomeFlags::Ricochet) && Outcome->ResultBrokenMask == 0
		&& Outcome->NewPartDamage == 0 && Outcome->PartIndex == ArmBox);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsStepMaxReactionTest, "GSTNParts.Core.Step.MaxReactionCapsTier", GSTNPartsTest::Flags)
bool FGSTN_PartsStepMaxReactionTest::RunTest(const FString& Parameters)
{
	// With the thresholds at 0 every staggering hit launches; the arm's cap makes it a push back, the torso (no cap) launches
	FStepFixture Fixture;
	FGSTN_HitResolutionContext Arm = Fixture.MakeContext(ArmBox, 10.f, 500.f, 5);
	FGSTN_HitResolutionContext Torso = Fixture.MakeContext(TorsoBox, 10.f, 500.f, 5);
	Run(Fixture, Arm);
	Run(Fixture, Torso);
	TestTrue(TEXT("both staggered"), Arm.Outcome.Knockback.bStaggered && Torso.Outcome.Knockback.bStaggered);
	TestTrue(TEXT("the torso launches"), Torso.Outcome.Knockback.Reaction == EGSTN_HitReaction::Launch);
	TestTrue(TEXT("the arm is capped at a push back"), Arm.Outcome.Knockback.Reaction == EGSTN_HitReaction::PushBack);

	// The body's cap applies too, the lower of the two wins
	Fixture.Set->BodyReaction.MaxReaction = EGSTN_PartReactionTier::StaggerInPlace;
	FGSTN_HitResolutionContext Capped = Fixture.MakeContext(TorsoBox, 10.f, 500.f, 5);
	Run(Fixture, Capped);
	TestTrue(TEXT("the body's cap is the lower"), Capped.Outcome.Knockback.Reaction == EGSTN_HitReaction::StaggerInPlace);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsStepLowerWeightTest, "GSTNParts.Core.Step.LowerWeightNeverMilderTier", GSTNPartsTest::Flags)
bool FGSTN_PartsStepLowerWeightTest::RunTest(const FString& Parameters)
{
	// A battered or mutilated entity (less mass) is knocked at least as far, with the same blow: the knockback only grows and the tier never drops
	FStepFixture Fixture;
	Fixture.Model->Tuning.PushBackThreshold = 400.f;
	Fixture.Model->Tuning.LaunchThreshold = 700.f;
	Fixture.Set->Parts[0].Reaction.StaggerThreshold = 0.f;

	double LastKnockback = -1.0;
	EGSTN_HitReaction LastTier = EGSTN_HitReaction::StaggerInPlace;
	const uint16 Masks[] = { 0, 0b0010, 0b0110 };           // intact; the arm broken (30 % of the mass); the arm and its hand broken
	for (const uint16 Mask : Masks)
	{
		Fixture.Ext.BrokenMask = Mask;
		FGSTN_HitResolutionContext Context = Fixture.MakeContext(TorsoBox, 10.f, 500.f, 5);
		Run(Fixture, Context);
		TestTrue(TEXT("the knockback never drops"), Context.Outcome.Knockback.Knockback >= LastKnockback);
		TestTrue(TEXT("the tier never drops"), static_cast<uint8>(Context.Outcome.Knockback.Reaction) >= static_cast<uint8>(LastTier));
		LastKnockback = Context.Outcome.Knockback.Knockback;
		LastTier = Context.Outcome.Knockback.Reaction;
	}
	Fixture.Ext.BrokenMask = 0b0010;
	const FGSTN_StepResult Mutilated = Fixture.RunStep(Fixture.MakeContext(TorsoBox, 10.f, 500.f, 5));
	TestTrue(TEXT("the lost mass lowers the weight scale"), Mutilated.WeightScale < 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsStepAttackerHitstopTest, "GSTNParts.Core.Step.AttackerHitstopIndependentOfDynamicPartState", GSTNPartsTest::Flags)
bool FGSTN_PartsStepAttackerHitstopTest::RunTest(const FString& Parameters)
{
	// The scales the attacker predicts its hitstop from (the band's and the part's) do not move with the broken mask, the part damage or the main pool
	FStepFixture Fixture;
	const FGSTN_StepResult Fresh = Fixture.RunStep(Fixture.MakeContext(HeadBox, 10.f, 30.f, 5));

	Fixture.Ext.BrokenMask = 0b0010;
	Fixture.Ext.PartDamage[0] = 200;
	Fixture.Ext.PartDamage[1] = 255;
	FGSTN_HitResolutionContext Battered = Fixture.MakeContext(HeadBox, 10.f, 30.f, 5);
	Battered.Victim.ReactionState = FGSTN_VictimReactionState::Make(250.0, 1.0, 0, FGSTN_StaggerThreshold());
	const FGSTN_StepResult Worn = Fixture.RunStep(Battered);

	TestEqual(TEXT("hitstop scale"), Worn.HitstopScale, Fresh.HitstopScale);
	TestEqual(TEXT("stagger scale"), Worn.StaggerScale, Fresh.StaggerScale);
	TestEqual(TEXT("knockback scale"), Worn.KnockbackScale, Fresh.KnockbackScale);
	TestEqual(TEXT("the part's scale is in it"), Fresh.HitstopScale, 1.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsStepOrderTest, "GSTNParts.Core.Step.OrderValidator", GSTNPartsTest::Flags)
bool FGSTN_PartsStepOrderTest::RunTest(const FString& Parameters)
{
	FStepFixture Fixture;
	auto Soft = [](const TCHAR* Name) { return TSoftClassPtr<UGSTN_HitResolutionStep>(FSoftObjectPath(FString::Printf(TEXT("/Script/GSTNCombatCore.GSTN_HitStep_%s"), Name))); };
	auto Make = [&](std::initializer_list<const TCHAR*> Names)
	{
		TStrongObjectPtr<UGSTN_CombatRules> Rules(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
		for (const TCHAR* Name : Names)
		{
			Rules->HitPipeline.Add(Name == FStringView(TEXT("PartResolve")) ? TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_PartResolve::StaticClass()) : Soft(Name));
		}
		return Rules;
	};
	auto Errors = [](const UGSTN_CombatRules& Rules)
	{
		TArray<FGSTN_ReactionMessage> Messages;
		FGSTN_PartsPipelineValidator::Validate(Rules, Messages);
		int32 Count = 0;
		for (const FGSTN_ReactionMessage& Message : Messages)
		{
			Count += Message.bError ? 1 : 0;
		}
		return Count;
	};

	TestEqual(TEXT("Shield then PartResolve is valid"), Errors(*Make({ TEXT("TeamFilter"), TEXT("Parry"), TEXT("Shield"), TEXT("PartResolve"), TEXT("ComputeKnockback") })), 0);
	TestEqual(TEXT("PartResolve before Shield is an error"), Errors(*Make({ TEXT("PartResolve"), TEXT("Shield"), TEXT("ComputeKnockback") })), 1);
	TestEqual(TEXT("PartResolve before Parry and Shield is two errors"), Errors(*Make({ TEXT("PartResolve"), TEXT("Parry"), TEXT("Shield") })), 2);
	TestEqual(TEXT("ApplyDamage next to PartResolve is an error"), Errors(*Make({ TEXT("PartResolve"), TEXT("ComputeKnockback"), TEXT("ApplyDamage") })), 1);
	TestEqual(TEXT("rules without the step are not this validator's business"), Errors(*Make({ TEXT("ComputeKnockback"), TEXT("ApplyDamage") })), 0);

	TArray<FGSTN_ReactionMessage> Messages;
	FGSTN_PartsPipelineValidator::ValidateKit(*Fixture.Set, *Make({ TEXT("ComputeKnockback"), TEXT("ApplyDamage") }), Messages);
	TestEqual(TEXT("a part set under rules without the step is an error"), Messages.Num(), 1);
	Messages.Reset();
	FGSTN_PartsPipelineValidator::ValidateKit(*Fixture.Set, *Make({ TEXT("PartResolve"), TEXT("ComputeKnockback") }), Messages);
	TestEqual(TEXT("and with the step it is fine"), Messages.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsStepOutcomeTest, "GSTNParts.Core.Step.OutcomeCarriesTheResultState", GSTNPartsTest::Flags)
bool FGSTN_PartsStepOutcomeTest::RunTest(const FString& Parameters)
{
	// The entry holds the whole state the hit left, copies property by property through an FInstancedStruct, and two hits in
	// one drain chain through it: the second is built on the state the first produced
	FStepFixture Fixture;
	Fixture.Set->Parts[1].PartCap = 20.f;       // the arm takes 10 against a cap of 20: damaged by the first hit, broken by the second
	FGSTN_StepResult First = Fixture.RunStep(Fixture.MakeContext(ArmBox, 10.f, 30.f, 6));
	const FGSTN_PartHitOutcome* FirstOutcome = FindOutcome(First);
	if (!TestNotNull(TEXT("first outcome"), FirstOutcome))
	{
		return false;
	}
	TestTrue(TEXT("the arm took damage"), FirstOutcome->HasFlag(GSTNPartOutcomeFlags::PartDamaged) && FirstOutcome->NewPartDamage > 0 && FirstOutcome->NewPartDamage < 255);

	// Feed the result state back as the next entry's extension
	Fixture.Ext.SetState(FirstOutcome->GetResultState());
	FGSTN_StepResult Second = Fixture.RunStep(Fixture.MakeContext(ArmBox, 10.f, 30.f, 6));
	const FGSTN_PartHitOutcome* SecondOutcome = FindOutcome(Second);
	if (!TestNotNull(TEXT("second outcome"), SecondOutcome))
	{
		return false;
	}
	TestTrue(TEXT("the second hit breaks the arm and its hand"), (SecondOutcome->NewlyBrokenMask & 0b0110) == 0b0110 && (SecondOutcome->ResultBrokenMask & 0b0110) == 0b0110);
	TestTrue(TEXT("the break moved the serial"), SecondOutcome->ResultBreakSerial != FirstOutcome->ResultBreakSerial);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
