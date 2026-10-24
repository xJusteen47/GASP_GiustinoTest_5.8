// Automation tests of the hit resolution pipeline (checkpoint H4): the runner, the standard steps, the rules asset and its
// validator. Pure: in-memory structs and transient objects, no world and no content asset. Hand values are worked out in the
// comments so a changed rule is a visible decision.

#include "GSTN_PipelineTestTypes.h"
#include "GSTN_CombatTestUtils.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitRunner.h"
#include "Hits/GSTN_HitSteps.h"
#include "Hits/GSTN_KnockbackModels.h"
#include "Hits/GSTN_RelationshipResolver.h"
#include "Misc/PackageName.h"
#include "Rules/GSTN_CombatRules.h"
#include "Rules/GSTN_CombatRulesValidation.h"
#include "UObject/SoftObjectPath.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPipelineTest
{
	using FStepPtr = TSoftClassPtr<UGSTN_HitResolutionStep>;

	FStepPtr Step(UClass* Class)
	{
		return FStepPtr(Class);
	}

	TStrongObjectPtr<UGSTN_CombatRules> MakeRules(std::initializer_list<FStepPtr> Steps)
	{
		TStrongObjectPtr<UGSTN_CombatRules> Rules(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
		Rules->HitPipeline = TArray<FStepPtr>(Steps);
		Rules->ResolveHitPipeline();
		return Rules;
	}

	/** The standard rules with a pipeline reduced to the steps under test (the model, curve and tuning stay the standard's) */
	TStrongObjectPtr<UGSTN_CombatRules> MakeStandard(std::initializer_list<FStepPtr> Steps)
	{
		TStrongObjectPtr<UGSTN_CombatRules> Rules(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
		Rules->HitPipeline = TArray<FStepPtr>(Steps);
		Rules->ResolveHitPipeline();
		return Rules;
	}

	/** Damage 10, base 600, growth 0: the authored bracket is 600 and the stagger force 10 (StaggerPerDamage 1) */
	FGSTN_HitResolutionContext MakeContext(float Weight = 100.f)
	{
		FGSTN_HitResolutionContext Context;
		Context.Incoming.Hit.Damage = 10.f;
		Context.Incoming.Hit.BaseKnockback = 600.f;
		Context.Incoming.Hit.KnockbackGrowth = 0.f;
		Context.Incoming.Hit.AngleDeg = 45.f;
		Context.Victim.Weight = Weight;
		return Context;
	}

	bool LogIs(std::initializer_list<int32> Expected)
	{
		return RunLog() == TArray<int32>(Expected);
	}

	FGameplayTag Rel(const FNativeGameplayTag& Tag)
	{
		return Tag.GetTag();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PipelineStepOrderTest, "GSTNCombat.Core.Hits.Pipeline.StepOrderFromAsset", GSTN_CORE_TEST_FLAGS)

bool FGSTN_PipelineStepOrderTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPipelineTest;
	{
		const auto Rules = MakeRules({ Step(UGSTN_TestHitStepOne::StaticClass()), Step(UGSTN_TestHitStepTwo::StaticClass()), Step(UGSTN_TestHitStepThree::StaticClass()) });
		RunLog().Reset();
		FGSTN_HitResolutionContext Context = MakeContext();
		const FGSTN_HitRunResult Result = FGSTN_HitPipelineRunner::Run(*Rules, Context);
		TestTrue(TEXT("1, 2, 3"), LogIs({ 1, 2, 3 }));
		TestEqual(TEXT("three steps ran"), Result.StepsRun, 3);
		TestFalse(TEXT("nothing stopped it"), Result.bStopped);
	}
	{
		const auto Rules = MakeRules({ Step(UGSTN_TestHitStepThree::StaticClass()), Step(UGSTN_TestHitStepOne::StaticClass()), Step(UGSTN_TestHitStepTwo::StaticClass()) });
		RunLog().Reset();
		FGSTN_HitResolutionContext Context = MakeContext();
		FGSTN_HitPipelineRunner::Run(*Rules, Context);
		TestTrue(TEXT("3, 1, 2: the order of the list, not of the declaration"), LogIs({ 3, 1, 2 }));
	}
	{
		// An empty pipeline is the identity: the hit lands and nothing else is written
		const auto Rules = MakeRules({});
		FGSTN_HitResolutionContext Context = MakeContext();
		FGSTN_HitPipelineRunner::Run(*Rules, Context);
		TestTrue(TEXT("lands"), Context.Outcome.bHit);
		TestEqual(TEXT("no damage"), Context.Outcome.Damage, 0.f);
		TestFalse(TEXT("no knockback"), Context.Outcome.bHasKnockback);
		TestTrue(TEXT("no requests"), Context.Outcome.Requests.IsEmpty());
		TestEqual(TEXT("weight scale untouched"), Context.Reaction.WeightScale, 1.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PipelineUnknownStepTest, "GSTNCombat.Core.Hits.Pipeline.UnknownStepSkippedReported", GSTN_CORE_TEST_FLAGS)

bool FGSTN_PipelineUnknownStepTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPipelineTest;
	const FString MissingPackage = FPackageName::GetModuleScriptPackageName(FStringView(TEXT("GSTNNoSuchModule")));
	AddExpectedError(TEXT("GSTNNoSuchModule"), EAutomationExpectedErrorFlags::Contains, -1);
	const FStepPtr Missing{ FSoftObjectPath(FTopLevelAssetPath(FName(*MissingPackage), FName(TEXT("GSTN_HitStep_NoSuchStep")))) };
	const FStepPtr Placeholder{ FSoftObjectPath(FTopLevelAssetPath(FName(*MissingPackage), FName(TEXT("GSTN_HitStep_PartResolve")))) };

	TStrongObjectPtr<UGSTN_CombatRules> Rules(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
	Rules->HitPipeline = { Step(UGSTN_TestHitStepOne::StaticClass()), FStepPtr(), Missing, Step(UGSTN_HitResolutionStep::StaticClass()), Placeholder, Step(UGSTN_TestHitStepTwo::StaticClass()) };
	TArray<FString> Reports;
	Rules->ResolveHitPipeline(&Reports);

	TestEqual(TEXT("the list keeps its six entries"), Rules->HitPipeline.Num(), 6);
	TestEqual(TEXT("only the two usable steps are resolved"), Rules->GetResolvedHitPipeline().Num(), 2);
	TestEqual(TEXT("four skips are reported"), Reports.Num(), 4);

	RunLog().Reset();
	FGSTN_HitResolutionContext Context = MakeContext();
	FGSTN_HitPipelineRunner::Run(*Rules, Context);
	TestTrue(TEXT("the usable steps ran in order"), LogIs({ 1, 2 }));

	// The validator says the same: an error per unknown entry, a warning for the placeholder
	TArray<FGSTN_ReactionMessage> Messages;
	FGSTN_CombatRulesValidation::Validate(*Rules, Messages);
	int32 Errors = 0, Warnings = 0;
	for (const FGSTN_ReactionMessage& Message : Messages)
	{
		(Message.bError ? Errors : Warnings)++;
	}
	TestEqual(TEXT("null, missing and abstract are errors"), Errors, 3);
	TestEqual(TEXT("the placeholder is a warning"), Warnings, 1);
	TestTrue(TEXT("PartResolve is a known name"), FGSTN_CombatRulesValidation::IsKnownPlaceholderName(TEXT("PartResolve")));
	TestFalse(TEXT("an arbitrary name is not"), FGSTN_CombatRulesValidation::IsKnownPlaceholderName(TEXT("GSTN_HitStep_NoSuchStep")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PipelineStopHaltsTest, "GSTNCombat.Core.Hits.Pipeline.StopHalts", GSTN_CORE_TEST_FLAGS)

bool FGSTN_PipelineStopHaltsTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPipelineTest;
	const auto Rules = MakeRules({ Step(UGSTN_TestHitStepOne::StaticClass()), Step(UGSTN_TestHitStepStop::StaticClass()), Step(UGSTN_TestHitStepThree::StaticClass()) });
	RunLog().Reset();
	FGSTN_HitResolutionContext Context = MakeContext();
	const FGSTN_HitRunResult Result = FGSTN_HitPipelineRunner::Run(*Rules, Context);
	TestTrue(TEXT("the step after the stop does not run"), LogIs({ 1, 9 }));
	TestTrue(TEXT("stopped"), Result.bStopped);
	TestEqual(TEXT("at the second step"), Result.StoppedAtStep, 1);
	TestEqual(TEXT("two steps ran"), Result.StepsRun, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PipelineFakeStepModifiersTest, "GSTNCombat.Core.Hits.Pipeline.FakeStepWritesModifiers", GSTN_CORE_TEST_FLAGS)

bool FGSTN_PipelineFakeStepModifiersTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPipelineTest;
	// Standard tuning: WeightReference 100, thresholds 540 / 1080. The fake step halves the weight, doubles the knockback,
	// sets a threshold of 5 and caps the tier at PushBack. K = 600 x 100 / (100 x 0.5) = 1200, x 2 = 2400 (once, not 4800)
	const auto Rules = MakeStandard({ Step(UGSTN_TestHitStepModifiers::StaticClass()), Step(UGSTN_HitStep_ComputeKnockback::StaticClass()) });
	FGSTN_HitResolutionContext Context = MakeContext(100.f);
	FGSTN_HitPipelineRunner::Run(*Rules, Context);

	TestEqual(TEXT("weight scale merged once"), Context.Reaction.WeightScale, 0.5f);
	TestEqual(TEXT("knockback scale merged once"), Context.Reaction.KnockbackScale, 2.f);
	TestEqual(TEXT("threshold replaced"), Context.Reaction.StaggerThreshold.Value, 5.f);
	TestTrue(TEXT("cap lowered"), Context.Reaction.MaxReaction == EGSTN_HitReaction::PushBack);
	TestTrue(TEXT("the model ran"), Context.Outcome.bHasKnockback);
	TestEqual(TEXT("K reaches the model exactly once"), Context.Outcome.Knockback.Knockback, 2400.f);
	TestTrue(TEXT("force 10 against threshold 5 staggers"), Context.Outcome.Knockback.bStaggered);
	TestTrue(TEXT("the cap holds although K is above the launch threshold"), Context.Outcome.Knockback.Reaction == EGSTN_HitReaction::PushBack);

	// A second hit through a fresh context gets the same bits: the step objects keep nothing
	FGSTN_HitResolutionContext Again = MakeContext(100.f);
	FGSTN_HitPipelineRunner::Run(*Rules, Again);
	TestEqual(TEXT("same K"), Again.Outcome.Knockback.Knockback, Context.Outcome.Knockback.Knockback);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PipelineHurtboxReactionTest, "GSTNCombat.Core.Hits.Pipeline.HurtboxReactionResolution", GSTN_CORE_TEST_FLAGS)

bool FGSTN_PipelineHurtboxReactionTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPipelineTest;
	const auto Rules = MakeRules({ Step(UGSTN_HitStep_HurtboxReaction::StaticClass()) });
	Rules->StaggerThreshold = 3.f;

	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
	Set->StaggerThreshold.Value = 8.f;
	Set->MaxReaction = EGSTN_HitReaction::PushBack;
	FGSTN_Hurtbox& Head = Set->Hurtboxes.AddDefaulted_GetRef();
	Head.ReactionScale.StaggerThreshold.Value = 20.f;
	Head.ReactionScale.KnockbackScale = 0.5f;
	Head.ReactionScale.bNoFlinch = false;
	Set->Hurtboxes.AddDefaulted();

	auto Run = [&](const UGSTN_HurtboxSetDefinition* UseSet, uint8 Index, EGSTN_HurtState State = EGSTN_HurtState::Normal, float Armor = 0.f)
	{
		FGSTN_HitResolutionContext Context = MakeContext();
		Context.HurtboxSet = UseSet;
		Context.HurtboxIndex = Index;
		Context.HurtState = State;
		Context.ArmorThreshold.Value = Armor;
		FGSTN_HitPipelineRunner::Run(*Rules, Context);
		return Context.Reaction;
	};

	const FGSTN_HitReactionModifiers OnHead = Run(Set.Get(), 0);
	TestEqual(TEXT("the hurtbox's own threshold wins"), OnHead.StaggerThreshold.Value, 20.f);
	TestEqual(TEXT("its knockback scale applies"), OnHead.KnockbackScale, 0.5f);
	TestTrue(TEXT("the set's cap is the entity default"), OnHead.MaxReaction == EGSTN_HitReaction::PushBack);

	TestEqual(TEXT("a hurtbox with no threshold takes the set's"), Run(Set.Get(), 1).StaggerThreshold.Value, 8.f);
	TestEqual(TEXT("no set takes the rules'"), Run(nullptr, 255).StaggerThreshold.Value, 3.f);
	TestEqual(TEXT("an index outside the set takes the set's, not a hurtbox's"), Run(Set.Get(), 7).StaggerThreshold.Value, 8.f);

	TestEqual(TEXT("an Armor window above the threshold raises it"), Run(Set.Get(), 1, EGSTN_HurtState::Armor, 50.f).StaggerThreshold.Value, 50.f);
	TestEqual(TEXT("an Armor window below it changes nothing"), Run(Set.Get(), 1, EGSTN_HurtState::Armor, 5.f).StaggerThreshold.Value, 8.f);

	Set->Hurtboxes[1].ReactionScale.StaggerThreshold.bInfinite = true;
	TestTrue(TEXT("super armor on a hurtbox is infinite"), Run(Set.Get(), 1).StaggerThreshold.bInfinite);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PipelineHealthWeightTest, "GSTNCombat.Core.Hits.Pipeline.HealthWeightUsesDamageBeforeHit", GSTN_CORE_TEST_FLAGS)

bool FGSTN_PipelineHealthWeightTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPipelineTest;
	// The standard rules, whole pipeline. The victim has 50 % before the hit: the curve falls 1.0 -> 0.6 over 0 - 100 %, so
	// the weight scale is 0.8 and K = 600 x 100 / (100 x 0.8) = 750, a push back. The hit's own 10 damage would make it 60 %
	// (0.76, K = 789): it does not count until the next hit
	const TStrongObjectPtr<UGSTN_CombatRules> Rules(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
	FGSTN_HitResolutionContext Context = MakeContext(100.f);
	Context.Victim.ReactionState = FGSTN_VictimReactionState::Make(50.0, 1.0, 0, FGSTN_StaggerThreshold());
	FGSTN_HitPipelineRunner::Run(*Rules, Context);

	TestTrue(TEXT("lands"), Context.Outcome.bHit);
	TestEqual(TEXT("the damage of the hit is written"), Context.Outcome.Damage, 10.f);
	TestTrue(TEXT("K uses the damage before the hit"), FMath::IsNearlyEqual(Context.Outcome.Knockback.Knockback, 750.f, 1.f));
	TestTrue(TEXT("750 is between the push back and launch thresholds"), Context.Outcome.Knockback.Reaction == EGSTN_HitReaction::PushBack);

	// A hurtbox set's own curve replaces the rules': a flat one never gets lighter
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));
	Set->WeightCurve.BakeConstant(1.0);
	FGSTN_HitResolutionContext Flat = MakeContext(100.f);
	Flat.Victim.ReactionState = Context.Victim.ReactionState;
	Flat.HurtboxSet = Set.Get();
	FGSTN_HitPipelineRunner::Run(*Rules, Flat);
	TestEqual(TEXT("a flat curve gives the authored K"), Flat.Outcome.Knockback.Knockback, 600.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PipelineTwoEntriesTest, "GSTNCombat.Core.Hits.Pipeline.TwoEntriesSeeEachOther", GSTN_CORE_TEST_FLAGS)

bool FGSTN_PipelineTwoEntriesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPipelineTest;
	// Two hits of 40 damage drain from one inbox in order. The first sees a full weight (K = 600 exactly); the victim then
	// stands at 40 %, weight scale 0.84, and the second hit's K is 600 / 0.84 = 714.3
	const TStrongObjectPtr<UGSTN_CombatRules> Rules(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
	FGSTN_HitVictimView Victim;
	Victim.Weight = 100.f;

	FGSTN_HitResolutionContext First = MakeContext(100.f);
	First.Incoming.Hit.Damage = 40.f;
	First.Victim = Victim;
	FGSTN_HitPipelineRunner::Run(*Rules, First);
	TestEqual(TEXT("the first hit sees full weight"), First.Outcome.Knockback.Knockback, 600.f);

	FGSTN_HitPipelineRunner::AdvanceVictim(*Rules, First.Outcome, Victim);
	TestTrue(TEXT("the victim stands at 40 %"), FMath::IsNearlyEqual(Victim.ReactionState.GetDamagePercent(), 40.f, 0.01f));

	FGSTN_HitResolutionContext Second = MakeContext(100.f);
	Second.Incoming.Hit.Damage = 40.f;
	Second.Victim = Victim;
	FGSTN_HitPipelineRunner::Run(*Rules, Second);
	TestTrue(TEXT("the second hit sees what the first left"), FMath::IsNearlyEqual(Second.Outcome.Knockback.Knockback, 714.3f, 1.f));
	TestTrue(TEXT("lighter victim, larger K"), Second.Outcome.Knockback.Knockback > First.Outcome.Knockback.Knockback);

	// A hit that did not land moves nothing
	FGSTN_HitOutcome Missed;
	Missed.bHit = false;
	Missed.Damage = 25.f;
	FGSTN_HitVictimView Untouched = Victim;
	FGSTN_HitPipelineRunner::AdvanceVictim(*Rules, Missed, Untouched);
	TestEqual(TEXT("percent unchanged"), Untouched.ReactionState.DamagePercentQ, Victim.ReactionState.DamagePercentQ);

	// The capped mode stops at the cap, the health mode counts down
	Rules->DamageMode = EGSTN_DamageMode::PercentCapped;
	FGSTN_HitOutcome Big;
	Big.Damage = 500.f;
	FGSTN_HitVictimView Capped;
	FGSTN_HitPipelineRunner::AdvanceVictim(*Rules, Big, Capped);
	TestTrue(TEXT("capped at MaxHealth"), FMath::IsNearlyEqual(Capped.ReactionState.GetDamagePercent(), Rules->MaxHealth, 0.01f));
	Rules->DamageMode = EGSTN_DamageMode::Health;
	FGSTN_HitOutcome Quarter;
	Quarter.Damage = 25.f;
	FGSTN_HitVictimView Health;
	FGSTN_HitPipelineRunner::AdvanceVictim(*Rules, Quarter, Health);
	TestEqual(TEXT("health falls by a quarter"), Health.HealthFraction, 0.75f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PipelineRelationshipTest, "GSTNCombat.Core.Hits.Pipeline.RelationshipResolverDefault", GSTN_CORE_TEST_FLAGS)

bool FGSTN_PipelineRelationshipTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPipelineTest;
	auto Query = [](int32 AttackerId, uint8 AttackerTeam, int32 VictimId, uint8 VictimTeam)
	{
		FGSTN_RelationshipQuery Result;
		Result.AttackerEntityId = AttackerId;
		Result.AttackerTeamId = AttackerTeam;
		Result.VictimEntityId = VictimId;
		Result.VictimTeamId = VictimTeam;
		return Result;
	};

	TestTrue(TEXT("same entity is Self"), UGSTN_RelationshipResolver::DefaultRule(Query(4, 1, 4, 1)) == Rel(GSTNCombatHitTags::Relationship_Self));
	TestTrue(TEXT("same team is Ally"), UGSTN_RelationshipResolver::DefaultRule(Query(4, 1, 5, 1)) == Rel(GSTNCombatHitTags::Relationship_Ally));
	TestTrue(TEXT("different teams are Enemy"), UGSTN_RelationshipResolver::DefaultRule(Query(4, 1, 5, 2)) == Rel(GSTNCombatHitTags::Relationship_Enemy));
	TestTrue(TEXT("a victim with no team is Neutral"), UGSTN_RelationshipResolver::DefaultRule(Query(4, 1, 5, 255)) == Rel(GSTNCombatHitTags::Relationship_Neutral));
	TestTrue(TEXT("an attacker with no team is Neutral"), UGSTN_RelationshipResolver::DefaultRule(Query(4, 255, 5, 2)) == Rel(GSTNCombatHitTags::Relationship_Neutral));
	TestTrue(TEXT("two with no team are Neutral, not Allies"), UGSTN_RelationshipResolver::DefaultRule(Query(4, 255, 5, 255)) == Rel(GSTNCombatHitTags::Relationship_Neutral));
	TestTrue(TEXT("two attackers that are not proxies are not Self"), UGSTN_RelationshipResolver::DefaultRule(Query(INDEX_NONE, 1, INDEX_NONE, 2)) == Rel(GSTNCombatHitTags::Relationship_Enemy));

	// The TeamFilter step over the standard rules: Enemy and Neutral are hit, Ally and Self are not; friendly fire adds Ally
	const auto Rules = MakeStandard({ Step(UGSTN_HitStep_TeamFilter::StaticClass()) });
	auto Lands = [&](int32 VictimId, uint8 VictimTeam)
	{
		FGSTN_HitResolutionContext Context = MakeContext();
		Context.Incoming.AttackerEntityId = 4;
		Context.Incoming.AttackerTeamId = 1;
		Context.Victim.EntityId = VictimId;
		Context.Victim.TeamId = VictimTeam;
		FGSTN_HitPipelineRunner::Run(*Rules, Context);
		return Context.Outcome.bHit;
	};
	TestTrue(TEXT("enemy lands"), Lands(5, 2));
	TestTrue(TEXT("neutral lands"), Lands(5, 255));
	TestFalse(TEXT("ally does not"), Lands(5, 1));
	TestFalse(TEXT("self does not"), Lands(4, 1));
	Rules->bFriendlyFire = true;
	TestTrue(TEXT("friendly fire lets an ally in"), Lands(5, 1));
	TestFalse(TEXT("but not the attacker itself"), Lands(4, 1));

	// A relationship the attacker side already resolved is not resolved again
	FGSTN_HitResolutionContext Preset = MakeContext();
	Preset.Incoming.Relationship = Rel(GSTNCombatHitTags::Relationship_Enemy);
	Preset.Victim.TeamId = 1;
	Preset.Incoming.AttackerTeamId = 1;
	Rules->bFriendlyFire = false;
	FGSTN_HitPipelineRunner::Run(*Rules, Preset);
	TestTrue(TEXT("the preset Enemy relationship stands"), Preset.Outcome.bHit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PipelineStandardRulesTest, "GSTNCombat.Core.Hits.Pipeline.StandardRulesBuiltIn", GSTN_CORE_TEST_FLAGS)

bool FGSTN_PipelineStandardRulesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPipelineTest;
	// The built-in standard rules are what UGSTN_CombatSettings::DefaultRules null means
	const TStrongObjectPtr<UGSTN_CombatRules> Rules(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
	TestEqual(TEXT("eight standard steps resolve"), Rules->GetResolvedHitPipeline().Num(), 8);
	TestTrue(TEXT("weight driven model"), Rules->KnockbackModel && Rules->KnockbackModel->IsA<UGSTN_KnockbackModel_WeightDriven>());
	TestFalse(TEXT("friendly fire off"), Rules->bFriendlyFire);
	TestTrue(TEXT("a health weight curve is authored"), Rules->DefaultWeightCurve.IsAuthored());
	if (Rules->KnockbackModel)
	{
		const FGSTN_ReactionTuning& Tuning = Rules->KnockbackModel->Tuning;
		TestTrue(TEXT("push back at 0.3 x tumble"), FMath::IsNearlyEqual(Tuning.PushBackThreshold, 0.3f * Tuning.TumbleThreshold, 0.01f));
		TestTrue(TEXT("launch at 0.6 x tumble"), FMath::IsNearlyEqual(Tuning.LaunchThreshold, 0.6f * Tuning.TumbleThreshold, 0.01f));
	}

	TArray<FGSTN_ReactionMessage> Messages;
	FGSTN_CombatRulesValidation::Validate(*Rules, Messages);
	TestTrue(TEXT("the standard rules validate clean"), Messages.IsEmpty());

	// Class defaults are the identity: an untouched asset resolves nothing
	const TStrongObjectPtr<UGSTN_CombatRules> Plain(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
	Plain->ResolveHitPipeline();
	FGSTN_HitResolutionContext Context = MakeContext();
	FGSTN_HitPipelineRunner::Run(*Plain, Context);
	TestTrue(TEXT("lands, nothing else"), Context.Outcome.bHit && !Context.Outcome.bHasKnockback && Context.Outcome.Damage == 0.f);

	// Whole pipeline, no hurtbox set: damage, knockback K = 600 (full weight), requests for damage and both hitstops
	FGSTN_HitResolutionContext Hit = MakeContext();
	FGSTN_HitPipelineRunner::Run(*Rules, Hit);
	TestEqual(TEXT("damage"), Hit.Outcome.Damage, 10.f);
	TestEqual(TEXT("K"), Hit.Outcome.Knockback.Knockback, 600.f);
	int32 DamageRequests = 0, FreezeRequests = 0;
	for (const FGSTN_HitRequest& Request : Hit.Outcome.Requests)
	{
		DamageRequests += Request.Kind == EGSTN_HitRequestKind::Damage ? 1 : 0;
		FreezeRequests += Request.Kind == EGSTN_HitRequestKind::Freeze ? 1 : 0;
	}
	TestEqual(TEXT("one damage request"), DamageRequests, 1);
	TestEqual(TEXT("a freeze for each side"), FreezeRequests, 2);

	// Invincible keeps the hit and its hitstop but no damage, launch or hitstun
	FGSTN_HitResolutionContext Invincible = MakeContext();
	Invincible.HurtState = EGSTN_HurtState::Invincible;
	FGSTN_HitPipelineRunner::Run(*Rules, Invincible);
	TestTrue(TEXT("lands"), Invincible.Outcome.bHit);
	TestEqual(TEXT("no damage"), Invincible.Outcome.Damage, 0.f);
	TestFalse(TEXT("no stagger"), Invincible.Outcome.Knockback.bStaggered);
	TestTrue(TEXT("hitstop still applies"), Invincible.Outcome.Knockback.HitstopFrames > 0);

	// Intangible: the hit passes through and the run stops
	FGSTN_HitResolutionContext Intangible = MakeContext();
	Intangible.HurtState = EGSTN_HurtState::Intangible;
	const FGSTN_HitRunResult Run = FGSTN_HitPipelineRunner::Run(*Rules, Intangible);
	TestFalse(TEXT("did not land"), Intangible.Outcome.bHit);
	TestTrue(TEXT("stopped"), Run.bStopped);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PipelineMatrixGoldenTest, "GSTNCombat.Core.Hits.Pipeline.ValidatorRulesMatrixGolden", GSTN_CORE_TEST_FLAGS)

bool FGSTN_PipelineMatrixGoldenTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPipelineTest;
	// Standard rules: K = bracket x 100 / weight; push back from 540, launch from 1080, tumble from 1800; force = damage
	//   Jab   (damage 5,  bracket 400):  weight 50 -> 800 Push, 100 -> 400 In, 200 -> 200 In
	//   Smash (damage 10, bracket 1100): weight 50 -> 2200 LaunchT, 100 -> 1100 Launch, 200 -> 550 Push
	//   Armored (weight 100, threshold 8): the Jab (force 5) does not stagger it, the Smash (force 10) launches it
	const TStrongObjectPtr<UGSTN_CombatRules> Rules(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));

	TArray<FGSTN_ReactionMatrixVictim> Victims;
	auto AddVictim = [&Victims](const TCHAR* Name, float Weight, float Threshold)
	{
		FGSTN_ReactionMatrixVictim& Victim = Victims.AddDefaulted_GetRef();
		Victim.Name = Name;
		Victim.Weight = Weight;
		Victim.StaggerThreshold.Value = Threshold;
	};
	AddVictim(TEXT("Light"), 50.f, -1.f);
	AddVictim(TEXT("Mid"), 100.f, -1.f);
	AddVictim(TEXT("Heavy"), 200.f, -1.f);
	AddVictim(TEXT("Armored"), 100.f, 8.f);

	TArray<FGSTN_ReactionMatrixHit> Hits;
	FGSTN_ReactionMatrixHit& Jab = Hits.AddDefaulted_GetRef();
	Jab.Name = TEXT("Jab");
	Jab.Hit.Damage = 5.f;
	Jab.Hit.BaseKnockback = 400.f;
	FGSTN_ReactionMatrixHit& Smash = Hits.AddDefaulted_GetRef();
	Smash.Name = TEXT("Smash");
	Smash.Hit.Damage = 10.f;
	Smash.Hit.BaseKnockback = 1000.f;
	Smash.Hit.KnockbackGrowth = 10.f;

	const FString Expected =
		TEXT("Victim | Jab | Smash\n")
		TEXT("Light (weight 50) | Push | LaunchT\n")
		TEXT("Mid (weight 100) | In | Launch\n")
		TEXT("Heavy (weight 200) | In | Push\n")
		TEXT("Armored (weight 100) | - | Launch");
	TestEqual(TEXT("reaction matrix"), FGSTN_CombatRulesValidation::PrintReactionMatrix(*Rules, Victims, Hits), Expected);

	// Validator rules: order, duplicates, both weights, a hitstop override above the limit
	{
		const auto Misordered = MakeStandard({ Step(UGSTN_HitStep_ComputeKnockback::StaticClass()), Step(UGSTN_HitStep_HealthWeight::StaticClass()), Step(UGSTN_HitStep_ComputeKnockback::StaticClass()) });
		TArray<FGSTN_ReactionMessage> Messages;
		FGSTN_CombatRulesValidation::Validate(*Misordered, Messages);
		int32 Errors = 0, Warnings = 0;
		for (const FGSTN_ReactionMessage& Message : Messages)
		{
			(Message.bError ? Errors : Warnings)++;
		}
		TestEqual(TEXT("HealthWeight after ComputeKnockback is an error"), Errors, 1);
		TestEqual(TEXT("the duplicate is a warning"), Warnings, 1);
	}
	{
		TArray<FGSTN_ReactionMessage> Messages;
		FGSTN_HitData Hit;
		Hit.HitstopMode = EGSTN_HitstopMode::Override;
		Hit.HitstopFramesOverride = 99;
		FGSTN_CombatRulesValidation::ValidateHitstopOverride(Hit, Rules->KnockbackModel->Tuning, Messages);
		TestEqual(TEXT("an override above MaxHitstop warns"), Messages.Num(), 1);
		Hit.HitstopFramesOverride = 5;
		Messages.Reset();
		FGSTN_CombatRulesValidation::ValidateHitstopOverride(Hit, Rules->KnockbackModel->Tuning, Messages);
		TestTrue(TEXT("within the limit is silent"), Messages.IsEmpty());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
