// Piggyback tests: hits while mounted (M7 G7).

#include "GSTN_HitStep_MountHit.h"
#include "GSTN_PiggybackContentLibrary.h"
#include "GSTN_PiggybackTags.h"
#include "Hits/GSTN_HitSteps.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_CombatRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags HitFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A hit with knockback 1000 cm/s (a launch of 1000 along +X, 20 hitstun frames) on a fighter of Role, from a third fighter */
	FGSTN_MountHitInput MakeInput(uint8 Role, bool bAboveBreak)
	{
		FGSTN_MountHitInput Input;
		Input.VictimRole = Role;
		Input.VictimEntityId = 7;
		Input.bAboveBreak = bAboveBreak;
		Input.bHasKnockback = true;
		Input.Knockback.Knockback = 1000.f;
		Input.Knockback.LaunchVelocity = FVector(1000.0, 0.0, 0.0);
		Input.Knockback.HitstunFrames = 20;
		Input.Knockback.Reaction = EGSTN_HitReaction::Launch;
		return Input;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MountHitRiderBelowTest, "GSTNPiggyback.Hits.MountHit.RiderBelowThreshold", HitFlags)
bool FGSTN_MountHitRiderBelowTest::RunTest(const FString& Parameters)
{
	const FGSTN_MountRules Rules;   // knockback scale 0, hitstun scale 0.5
	const FGSTN_MountHitDecision Decision = UGSTN_HitStep_MountHit::Evaluate(MakeInput(GSTNMountRole::Rider, false), Rules);
	TestFalse(TEXT("lands"), Decision.bDrop);
	TestFalse(TEXT("no break"), Decision.BreakReason.IsValid());
	TestTrue(TEXT("scaled"), Decision.bScaled);
	TestEqual(TEXT("knockback x 0"), Decision.Knockback.Knockback, 0.f);
	TestTrue(TEXT("no launch"), Decision.Knockback.LaunchVelocity.IsZero());
	TestEqual(TEXT("hitstun x 0.5"), Decision.Knockback.HitstunFrames, 10);
	TestTrue(TEXT("flinches in place"), Decision.Knockback.Reaction == EGSTN_HitReaction::StaggerInPlace);

	FGSTN_MountRules Half;
	Half.RiderKnockbackScale = 0.5f;
	Half.RiderHitstunScale = 1.f;
	const FGSTN_MountHitDecision Scaled = UGSTN_HitStep_MountHit::Evaluate(MakeInput(GSTNMountRole::Rider, false), Half);
	TestEqual(TEXT("knockback x 0.5"), Scaled.Knockback.Knockback, 500.f);
	TestEqual(TEXT("launch x 0.5"), Scaled.Knockback.LaunchVelocity.X, 500.0);
	TestEqual(TEXT("hitstun x 1"), Scaled.Knockback.HitstunFrames, 20);
	TestTrue(TEXT("a launch stays a launch"), Scaled.Knockback.Reaction == EGSTN_HitReaction::Launch);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MountHitRiderAboveTest, "GSTNPiggyback.Hits.MountHit.RiderAboveThreshold", HitFlags)
bool FGSTN_MountHitRiderAboveTest::RunTest(const FString& Parameters)
{
	const FGSTN_MountHitDecision Decision = UGSTN_HitStep_MountHit::Evaluate(MakeInput(GSTNMountRole::Rider, true), FGSTN_MountRules());
	TestFalse(TEXT("lands"), Decision.bDrop);
	TestTrue(TEXT("breaks as RiderHit"), Decision.BreakReason == GSTNPiggybackTags::BreakReason_RiderHit);
	TestFalse(TEXT("the computed knockback stands"), Decision.bScaled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MountHitCarrierTest, "GSTNPiggyback.Hits.MountHit.CarrierThreshold", HitFlags)
bool FGSTN_MountHitCarrierTest::RunTest(const FString& Parameters)
{
	const FGSTN_MountHitDecision Below = UGSTN_HitStep_MountHit::Evaluate(MakeInput(GSTNMountRole::Carrier, false), FGSTN_MountRules());
	TestFalse(TEXT("below: lands"), Below.bDrop);
	TestFalse(TEXT("below: no break"), Below.BreakReason.IsValid());
	TestFalse(TEXT("below: a normal hit"), Below.bScaled);
	const FGSTN_MountHitDecision Above = UGSTN_HitStep_MountHit::Evaluate(MakeInput(GSTNMountRole::Carrier, true), FGSTN_MountRules());
	TestTrue(TEXT("above: breaks as CarrierHit"), Above.BreakReason == GSTNPiggybackTags::BreakReason_CarrierHit);
	TestFalse(TEXT("above: a normal hit"), Above.bScaled);
	const FGSTN_MountHitDecision Free = UGSTN_HitStep_MountHit::Evaluate(MakeInput(GSTNMountRole::None, true), FGSTN_MountRules());
	TestFalse(TEXT("not mounted: nothing"), Free.bDrop || Free.bScaled || Free.BreakReason.IsValid() || Free.Credit.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MountHitCoverTest, "GSTNPiggyback.Hits.MountHit.Cover", HitFlags)
bool FGSTN_MountHitCoverTest::RunTest(const FString& Parameters)
{
	// Facing +X (yaw 0): a hitbox travelling along -X comes from the front
	TestTrue(TEXT("front, arc 90"), UGSTN_HitStep_MountHit::IsInFrontArc(FVector(-1, 0, 0), 0.f, 90.f));
	TestTrue(TEXT("40 deg off, arc 90"), UGSTN_HitStep_MountHit::IsInFrontArc(FVector(-1, -FMath::Tan(FMath::DegreesToRadians(40.f)), 0), 0.f, 90.f));
	TestFalse(TEXT("50 deg off, arc 90"), UGSTN_HitStep_MountHit::IsInFrontArc(FVector(-1, -FMath::Tan(FMath::DegreesToRadians(50.f)), 0), 0.f, 90.f));
	TestFalse(TEXT("from behind, arc 90"), UGSTN_HitStep_MountHit::IsInFrontArc(FVector(1, 0, 0), 0.f, 90.f));
	TestFalse(TEXT("arc 0 = off"), UGSTN_HitStep_MountHit::IsInFrontArc(FVector(-1, 0, 0), 0.f, 0.f));
	TestTrue(TEXT("facing +Y, from +Y"), UGSTN_HitStep_MountHit::IsInFrontArc(FVector(0, -1, 0), 90.f, 30.f));
	TestTrue(TEXT("arc 360 = everywhere"), UGSTN_HitStep_MountHit::IsInFrontArc(FVector(1, 0, 0), 0.f, 360.f));

	FGSTN_MountHitInput Input = MakeInput(GSTNMountRole::Rider, false);
	Input.bInCoverArc = true;
	const FGSTN_MountHitDecision On = UGSTN_HitStep_MountHit::Evaluate(Input, FGSTN_MountRules());
	TestTrue(TEXT("cover on: the rider is not hit"), On.bDrop && On.bCovered);
	Input.bInCoverArc = false;
	const FGSTN_MountHitDecision Off = UGSTN_HitStep_MountHit::Evaluate(Input, FGSTN_MountRules());
	TestFalse(TEXT("cover off: the rider is hit"), Off.bDrop);
	FGSTN_MountHitInput Carrier = MakeInput(GSTNMountRole::Carrier, false);
	Carrier.bInCoverArc = true;
	TestFalse(TEXT("the carrier is never covered"), UGSTN_HitStep_MountHit::Evaluate(Carrier, FGSTN_MountRules()).bDrop);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MountHitPartnerTest, "GSTNPiggyback.Hits.MountHit.PartnerExclusion", HitFlags)
bool FGSTN_MountHitPartnerTest::RunTest(const FString& Parameters)
{
	// With friendly fire on the team filter lets an ally's hit through: the mount step is what keeps partners apart (M7.12)
	for (const uint8 Role : { GSTNMountRole::Rider, GSTNMountRole::Carrier })
	{
		FGSTN_MountHitInput Input = MakeInput(Role, true);
		Input.bFromPartner = true;
		Input.LinkFilter = EGSTN_LinkFilter::Any;
		TestTrue(TEXT("Any: dropped"), UGSTN_HitStep_MountHit::Evaluate(Input, FGSTN_MountRules()).bDrop);
		Input.LinkFilter = EGSTN_LinkFilter::ExcludeLinkedPartner;
		TestTrue(TEXT("ExcludeLinkedPartner: dropped"), UGSTN_HitStep_MountHit::Evaluate(Input, FGSTN_MountRules()).bDrop);
		Input.LinkFilter = EGSTN_LinkFilter::LinkedPartnerOnly;
		const FGSTN_MountHitDecision Only = UGSTN_HitStep_MountHit::Evaluate(Input, FGSTN_MountRules());
		TestFalse(TEXT("LinkedPartnerOnly: lands"), Only.bDrop);
		TestFalse(TEXT("LinkedPartnerOnly: a partner hit never breaks the mount here"), Only.BreakReason.IsValid());
		TestFalse(TEXT("LinkedPartnerOnly: not scaled"), Only.bScaled);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MountHitCreditTest, "GSTNPiggyback.Hits.MountHit.Credit", HitFlags)
bool FGSTN_MountHitCreditTest::RunTest(const FString& Parameters)
{
	FGSTN_MountHitInput Input = MakeInput(GSTNMountRole::None, false);
	Input.AttackerRole = GSTNMountRole::Rider;
	Input.AttackerPartnerId = 3;

	FGSTN_MountRules Rules;
	TestFalse(TEXT("OwnerOnly (default): no credit"), UGSTN_HitStep_MountHit::Evaluate(Input, Rules).Credit.IsSet());
	Rules.DefaultCredit = EGSTN_PairCredit::InitiatorAndOwner;
	FGSTN_HitCredit Credit = UGSTN_HitStep_MountHit::Evaluate(Input, Rules).Credit;
	TestEqual(TEXT("InitiatorAndOwner: partner named"), Credit.PartnerEntityId, 3);
	TestEqual(TEXT("InitiatorAndOwner: no share"), static_cast<int32>(Credit.PartnerShare), 0);
	Rules.DefaultCredit = EGSTN_PairCredit::Split;
	Credit = UGSTN_HitStep_MountHit::Evaluate(Input, Rules).Credit;
	TestEqual(TEXT("Split: partner named"), Credit.PartnerEntityId, 3);
	TestEqual(TEXT("Split: half"), static_cast<int32>(Credit.PartnerShare), 128);
	Input.VictimEntityId = 3;
	TestFalse(TEXT("a hit on the partner itself credits nobody"), UGSTN_HitStep_MountHit::Evaluate(Input, Rules).Credit.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MountHitPipelineTest, "GSTNPiggyback.Hits.MountHit.AddToRules", HitFlags)
bool FGSTN_MountHitPipelineTest::RunTest(const FString& Parameters)
{
	UGSTN_CombatRules* Rules = NewObject<UGSTN_CombatRules>();
	Rules->HitPipeline = { TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_ComputeKnockback::StaticClass()),
		TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_ApplyDamage::StaticClass()),
		TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_MeterGain::StaticClass()) };
	TestTrue(TEXT("added"), UGSTN_PiggybackContentLibrary::AddMountStepToRules(Rules));
	TestTrue(TEXT("idempotent"), UGSTN_PiggybackContentLibrary::AddMountStepToRules(Rules));
	TestEqual(TEXT("one step more"), Rules->HitPipeline.Num(), 4);
	TestTrue(TEXT("right after ComputeKnockback"), Rules->HitPipeline[1].ToSoftObjectPath() == FSoftObjectPath(UGSTN_HitStep_MountHit::StaticClass()));
	TestTrue(TEXT("resolved"), Rules->GetResolvedHitPipeline().Contains(UGSTN_HitStep_MountHit::StaticClass()));
	TestTrue(TEXT("removed"), UGSTN_PiggybackContentLibrary::RemoveMountStepFromRules(Rules));
	TestEqual(TEXT("back to three"), Rules->HitPipeline.Num(), 3);
	UGSTN_CombatRules* Empty = NewObject<UGSTN_CombatRules>();
	TestFalse(TEXT("no ComputeKnockback: not added"), UGSTN_PiggybackContentLibrary::AddMountStepToRules(Empty));
	return true;
}

#endif
