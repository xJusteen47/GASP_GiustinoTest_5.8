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
	TestTrue(TEXT("a looping clip wraps"), FMath::IsNearlyEqual(FGSTN_AnimNode_ChooserPlayer::AdvanceTime(0.9f, 0.2f, 1.f, 1.f, true), 0.1f, KINDA_SMALL_NUMBER));
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

	// Pelvis squash: the chosen local axis takes the squash, the others widen; lying flat fades it out
	const FVector Upright = FGSTN_AnimNode_GreyboxShape::AxisSquashScale(0, 0.5f, 1.f);
	TestTrue(TEXT("axis squash on X"), FMath::IsNearlyEqual(Upright.X, 0.5f) && Upright.Y > 1.f && FMath::IsNearlyEqual(Upright.Y, Upright.Z));
	TestTrue(TEXT("axis squash keeps the volume"), FMath::IsNearlyEqual(Upright.X * Upright.Y * Upright.Z, 1.f, 1.e-4f));
	TestTrue(TEXT("axis squash on Y"), FMath::IsNearlyEqual(FGSTN_AnimNode_GreyboxShape::AxisSquashScale(1, 0.5f, 1.f).Y, 0.5f));
	TestTrue(TEXT("lying flat = no squash"), FGSTN_AnimNode_GreyboxShape::AxisSquashScale(2, 0.5f, 0.f).Equals(FVector::OneVector));

	// Run 3 (crouch through the floor): the up axis of a mannequin-like pelvis (local X up) is X, and squashing the pelvis height and the
	// bone along that axis by the same amount keeps a capsule bottom hanging below the pelvis above the ground
	const FQuat XUp = FQuat::FindBetweenNormals(FVector::XAxisVector, FVector::UpVector);
	TestEqual(TEXT("up axis of an X-up bone"), FGSTN_AnimNode_GreyboxShape::UpAxisOf(XUp), 0);
	TestEqual(TEXT("up axis of an identity bone"), FGSTN_AnimNode_GreyboxShape::UpAxisOf(FQuat::Identity), 2);
	const float PelvisHeight = 95.f;
	const float BottomBelowPelvis = 81.f;  // capsule bottom 14 cm above the feet
	for (const float Squash : {0.35f, 0.6f, 1.f, 1.3f})
	{
		const float Height = FGSTN_AnimNode_GreyboxShape::PelvisHeightAfterSquash(PelvisHeight, Squash);
		const float Along = FGSTN_AnimNode_GreyboxShape::AxisSquashScale(0, Squash, 1.f).X;
		TestTrue(FString::Printf(TEXT("bottom stays above the ground at squash %.2f"), Squash), Height - BottomBelowPelvis * Along >= 0.f);
		TestTrue(FString::Printf(TEXT("bottom scales about the ground at squash %.2f"), Squash),
			FMath::IsNearlyEqual(Height - BottomBelowPelvis * Along, (PelvisHeight - BottomBelowPelvis) * Squash, 1.e-3f));
	}
	return true;
}

#endif
