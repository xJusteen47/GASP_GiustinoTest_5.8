// Directional hit additives (hit-react-directional): side selection, intensity weight, placeholder curve. Pure functions only.

#include "Misc/AutomationTest.h"
#include "Cosmetics/GSTN_ReactionAnimSet.h"
#include "GSTN_AnimNode_DirectionalHitAdditive.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags DirectionalTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DirectionalHitSideTest, "GSTNCombat.GAS.ReactionAnim.DirectionalSide", DirectionalTestFlags)
bool FGSTN_DirectionalHitSideTest::RunTest(const FString& Parameters)
{
	const FVector Forward = FVector::ForwardVector;   // victim faces +X
	const FVector Right = FVector::RightVector;       // its right is +Y
	// Push = attacker to victim
	TestEqual(TEXT("Attacker in front pushes back: Front"), FGSTN_DirectionalHitMath::SelectSide(-Forward, Forward, Right), EGSTN_HitSide::Front);
	TestEqual(TEXT("Attacker behind: Back"), FGSTN_DirectionalHitMath::SelectSide(Forward, Forward, Right), EGSTN_HitSide::Back);
	TestEqual(TEXT("Attacker on the right pushes left: Right"), FGSTN_DirectionalHitMath::SelectSide(-Right, Forward, Right), EGSTN_HitSide::Right);
	TestEqual(TEXT("Attacker on the left: Left"), FGSTN_DirectionalHitMath::SelectSide(Right, Forward, Right), EGSTN_HitSide::Left);
	TestEqual(TEXT("Exact diagonal goes to Front/Back"), FGSTN_DirectionalHitMath::SelectSide(FVector(-1, -1, 0), Forward, Right), EGSTN_HitSide::Front);
	TestEqual(TEXT("Mostly side: Right"), FGSTN_DirectionalHitMath::SelectSide(FVector(-0.3, -1, 0), Forward, Right), EGSTN_HitSide::Right);
	TestEqual(TEXT("Vertical part ignored"), FGSTN_DirectionalHitMath::SelectSide(FVector(1, 0, -5), Forward, Right), EGSTN_HitSide::Back);
	TestEqual(TEXT("Zero push: Front"), FGSTN_DirectionalHitMath::SelectSide(FVector::ZeroVector, Forward, Right), EGSTN_HitSide::Front);

	// Rotated victim: facing +Y, right -X
	TestEqual(TEXT("Rotated victim, attacker at +Y: Front"), FGSTN_DirectionalHitMath::SelectSide(FVector(0, -1, 0), FVector(0, 1, 0), FVector(-1, 0, 0)), EGSTN_HitSide::Front);

	// The push of a side is the cardinal direction it stands for, and selects that side back
	for (EGSTN_HitSide Side : { EGSTN_HitSide::Front, EGSTN_HitSide::Back, EGSTN_HitSide::Left, EGSTN_HitSide::Right })
	{
		TestEqual(TEXT("SidePush round trip"), FGSTN_DirectionalHitMath::SelectSide(FGSTN_DirectionalHitMath::SidePush(Side, Forward, Right), Forward, Right), Side);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DirectionalHitWeightTest, "GSTNCombat.GAS.ReactionAnim.DirectionalWeight", DirectionalTestFlags)
bool FGSTN_DirectionalHitWeightTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Unstaggered"), FGSTN_DirectionalHitMath::IntensityWeight(false, 30, 40, 0.5f, 0.3f), 0.3f);
	TestEqual(TEXT("No hitstun: min"), FGSTN_DirectionalHitMath::IntensityWeight(true, 0, 40, 0.5f, 0.3f), 0.5f);
	TestEqual(TEXT("Half"), FGSTN_DirectionalHitMath::IntensityWeight(true, 20, 40, 0.5f, 0.3f), 0.75f);
	TestEqual(TEXT("Capped at 1"), FGSTN_DirectionalHitMath::IntensityWeight(true, 400, 40, 0.5f, 0.3f), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DirectionalHitCurveTest, "GSTNCombat.GAS.ReactionAnim.DirectionalPlaceholderCurve", DirectionalTestFlags)
bool FGSTN_DirectionalHitCurveTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Starts at 0"), FGSTN_AnimNode_DirectionalHitAdditive::PlaceholderCurve(0.f, 0.07f, 0.4f), 0.f);
	TestTrue(TEXT("Peak is 1"), FMath::IsNearlyEqual(FGSTN_AnimNode_DirectionalHitAdditive::PlaceholderCurve(0.07f, 0.07f, 0.4f), 1.f, 1e-4f));
	TestTrue(TEXT("Rises fast"), FGSTN_AnimNode_DirectionalHitAdditive::PlaceholderCurve(0.035f, 0.07f, 0.4f) > 0.6f);
	TestTrue(TEXT("Returns"), FGSTN_AnimNode_DirectionalHitAdditive::PlaceholderCurve(0.39f, 0.07f, 0.4f) < 0.01f);
	TestEqual(TEXT("Ends at 0"), FGSTN_AnimNode_DirectionalHitAdditive::PlaceholderCurve(0.4f, 0.07f, 0.4f), 0.f);
	return true;
}

#endif
