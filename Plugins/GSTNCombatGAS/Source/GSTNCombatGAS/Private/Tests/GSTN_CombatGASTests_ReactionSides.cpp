// Per-side reaction poses (hit-arms, owner decision 2A 2026-11-18): which clip a rule plays for the side a hit came from. Data and pure
// functions only: the clips are soft paths that are never loaded, so the tests need no content.

#include "Misc/AutomationTest.h"
#include "Cosmetics/GSTN_ReactionAnimSet.h"
#include "GSTN_CombatTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags SideTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	TSoftObjectPtr<UAnimSequenceBase> Clip(const TCHAR* Path)
	{
		return TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(Path));
	}

	FGSTN_ReactionAnimRule MakeHitStand()
	{
		FGSTN_ReactionAnimRule Rule;
		Rule.Name = TEXT("HitStand");
		Rule.RequiredTags.AddTag(GSTNCombatTags::State_Hitstun);
		Rule.Animation = Clip(TEXT("/Game/Test/AS_Base.AS_Base"));
		return Rule;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionSidesFallbackTest, "GSTNCombat.GAS.ReactionAnim.Sides.Fallback", SideTestFlags)
bool FGSTN_ReactionSidesFallbackTest::RunTest(const FString& Parameters)
{
	FGSTN_ReactionAnimRule Rule = MakeHitStand();
	TestFalse(TEXT("No side clip: HasAny is false"), Rule.SideAnimations.HasAny());
	for (EGSTN_HitSide Side : { EGSTN_HitSide::Front, EGSTN_HitSide::Back, EGSTN_HitSide::Left, EGSTN_HitSide::Right })
	{
		TestTrue(TEXT("No side clip: every side plays the base (the rule as it was)"), Rule.AnimationForSide(Side) == Rule.Animation);
	}

	Rule.SideAnimations.Left = Clip(TEXT("/Game/Test/AS_LeadL.AS_LeadL"));
	Rule.SideAnimations.Right = Clip(TEXT("/Game/Test/AS_LeadR.AS_LeadR"));
	TestTrue(TEXT("HasAny"), Rule.SideAnimations.HasAny());
	TestTrue(TEXT("Left hit: the left-lead clip"), Rule.AnimationForSide(EGSTN_HitSide::Left) == Rule.SideAnimations.Left);
	TestTrue(TEXT("Right hit: the right-lead clip"), Rule.AnimationForSide(EGSTN_HitSide::Right) == Rule.SideAnimations.Right);
	TestTrue(TEXT("Front hit: empty side falls back to the base"), Rule.AnimationForSide(EGSTN_HitSide::Front) == Rule.Animation);
	TestTrue(TEXT("Back hit: empty side falls back to the base"), Rule.AnimationForSide(EGSTN_HitSide::Back) == Rule.Animation);

	Rule.SideAnimations.Back = Clip(TEXT("/Game/Test/AS_Back.AS_Back"));
	TestTrue(TEXT("A side of its own is used"), Rule.AnimationForSide(EGSTN_HitSide::Back) == Rule.SideAnimations.Back);
	TestTrue(TEXT("Then animation is not touched"), Rule.ThenAnimation.IsNull());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionSidesSetTest, "GSTNCombat.GAS.ReactionAnim.Sides.SetQuery", SideTestFlags)
bool FGSTN_ReactionSidesSetTest::RunTest(const FString& Parameters)
{
	UGSTN_ReactionAnimSet* Set = NewObject<UGSTN_ReactionAnimSet>();
	FGSTN_ReactionAnimRule Rule = MakeHitStand();
	Rule.SideAnimations.Left = Clip(TEXT("/Game/Test/AS_LeadL.AS_LeadL"));
	Rule.SideAnimations.Right = Clip(TEXT("/Game/Test/AS_LeadR.AS_LeadR"));
	Set->Rules.Add(Rule);

	TestTrue(TEXT("Blueprint query: right side"), Set->GetRuleAnimationForSide(0, EGSTN_HitSide::Right) == Rule.SideAnimations.Right);
	TestTrue(TEXT("Blueprint query: front falls back"), Set->GetRuleAnimationForSide(0, EGSTN_HitSide::Front) == Rule.Animation);
	TestTrue(TEXT("Bad index: null"), Set->GetRuleAnimationForSide(3, EGSTN_HitSide::Left).IsNull());
	TestTrue(TEXT("Negative index: null"), Set->GetRuleAnimationForSide(INDEX_NONE, EGSTN_HitSide::Left).IsNull());

	// The full chain a reaction runs: the push of the hit, the victim's facing, the side, the clip
	const FVector Forward = FVector::ForwardVector;   // victim faces +X, its right is +Y
	const FVector Right = FVector::RightVector;
	const FVector FromTheLeft = Right;                 // attacker at -Y pushes the victim toward +Y
	TestEqual(TEXT("Attacker on the left: Left"), UGSTN_ReactionAnimSet::SelectHitSide(FromTheLeft, Forward, Right), EGSTN_HitSide::Left);
	TestTrue(TEXT("and the left-lead clip"), Set->GetRuleAnimationForSide(0, UGSTN_ReactionAnimSet::SelectHitSide(FromTheLeft, Forward, Right)) == Rule.SideAnimations.Left);
	TestTrue(TEXT("Attacker on the right: the right-lead clip"), Set->GetRuleAnimationForSide(0, UGSTN_ReactionAnimSet::SelectHitSide(-Right, Forward, Right)) == Rule.SideAnimations.Right);

	// The same push read after the victim turned to face the attacker (the back-hit turn): a hit from behind becomes a front hit, which is
	// why the component reads the side again during SideSettleFrames
	TestEqual(TEXT("Hit from behind"), UGSTN_ReactionAnimSet::SelectHitSide(Forward, Forward, Right), EGSTN_HitSide::Back);
	TestEqual(TEXT("after the turn: Front"), UGSTN_ReactionAnimSet::SelectHitSide(Forward, -Forward, -Right), EGSTN_HitSide::Front);
	// The same push, the victim turned 90 degrees: the left hit becomes a front hit, the clip changes back to the base
	TestEqual(TEXT("Left push, victim turned toward the attacker: Front"), UGSTN_ReactionAnimSet::SelectHitSide(FromTheLeft, -Right, Forward), EGSTN_HitSide::Front);
	TestEqual(TEXT("Default settle window is 3 frames"), FGSTN_ReactionSideAnimations().SideSettleFrames, 3);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ReactionSidesValidationTest, "GSTNCombat.GAS.ReactionAnim.Sides.Validation", SideTestFlags)
bool FGSTN_ReactionSidesValidationTest::RunTest(const FString& Parameters)
{
	UGSTN_ReactionAnimSet* Set = NewObject<UGSTN_ReactionAnimSet>();
	FGSTN_ReactionAnimRule Rule = MakeHitStand();
	Set->Rules.Add(Rule);
	{
		FDataValidationContext Context;
		Set->IsDataValid(Context);
		TestEqual(TEXT("No side clips: no warning"), static_cast<int32>(Context.GetNumWarnings()), 0);
	}
	Set->Rules[0].SideAnimations.Left = Clip(TEXT("/Game/Test/AS_LeadL.AS_LeadL"));
	{
		FDataValidationContext Context;
		Set->IsDataValid(Context);
		TestEqual(TEXT("Left without Right: one warning"), static_cast<int32>(Context.GetNumWarnings()), 1);
	}
	Set->Rules[0].SideAnimations.Right = Clip(TEXT("/Game/Test/AS_LeadR.AS_LeadR"));
	{
		FDataValidationContext Context;
		Set->IsDataValid(Context);
		TestEqual(TEXT("Both sides: no warning"), static_cast<int32>(Context.GetNumWarnings()), 0);
	}
	return true;
}
#endif

#endif
