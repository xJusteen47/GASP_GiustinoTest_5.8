// Tests of the greybox anim nodes (greybox-capsula, 2026-11-24): GSTNAnimation.Core.Greybox.*
//
// Pure: the chooser player's clip timing and the shape node's squash scale, no mesh.

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_AnimNode_ChooserPlayer.h"
#include "GSTN_AnimNode_GreyboxShape.h"
#include "Misc/AutomationTest.h"

namespace GSTNGreyboxNodeTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ChooserPlayerTimeTest, "GSTNAnimation.Core.Greybox.ChooserPlayerTime", GSTNGreyboxNodeTest::TestFlags)
bool FGSTN_ChooserPlayerTimeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("advances by rate"), FMath::IsNearlyEqual(FGSTN_AnimNode_ChooserPlayer::AdvanceTime(0.f, 0.1f, 2.f, 1.f, false), 0.2f));
	TestTrue(TEXT("a looping clip wraps"), FMath::IsNearlyEqual(FGSTN_AnimNode_ChooserPlayer::AdvanceTime(0.9f, 0.2f, 1.f, 1.f, true), 0.1f));
	TestTrue(TEXT("a one-shot holds the last frame"), FMath::IsNearlyEqual(FGSTN_AnimNode_ChooserPlayer::AdvanceTime(0.9f, 0.2f, 1.f, 1.f, false), 1.f));
	TestTrue(TEXT("no clip length = 0"), FMath::IsNearlyZero(FGSTN_AnimNode_ChooserPlayer::AdvanceTime(0.5f, 0.2f, 1.f, 0.f, true)));
	TestTrue(TEXT("Asset follows the asset"), FGSTN_AnimNode_ChooserPlayer::ResolveLooping(EGSTN_ChooserPlayerLoop::Asset, true));
	TestFalse(TEXT("Once never loops"), FGSTN_AnimNode_ChooserPlayer::ResolveLooping(EGSTN_ChooserPlayerLoop::Once, true));
	TestTrue(TEXT("Loop always loops"), FGSTN_AnimNode_ChooserPlayer::ResolveLooping(EGSTN_ChooserPlayerLoop::Loop, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GreyboxSquashScaleTest, "GSTNAnimation.Core.Greybox.SquashScale", GSTNGreyboxNodeTest::TestFlags)
bool FGSTN_GreyboxSquashScaleTest::RunTest(const FString& Parameters)
{
	const FVector Rest = FGSTN_AnimNode_GreyboxShape::SquashToScale(1.f);
	TestTrue(TEXT("1 = no scale"), Rest.Equals(FVector::OneVector));
	const FVector Squashed = FGSTN_AnimNode_GreyboxShape::SquashToScale(0.8f);
	TestTrue(TEXT("squash keeps the volume"), FMath::IsNearlyEqual(Squashed.X * Squashed.Y * Squashed.Z, 1.f, 1.e-4f));
	TestTrue(TEXT("squash is lower and wider"), Squashed.Z < 1.f && Squashed.X > 1.f);
	TestTrue(TEXT("clamped"), FGSTN_AnimNode_GreyboxShape::SquashToScale(0.f).Z >= 0.2f);
	return true;
}

#endif
