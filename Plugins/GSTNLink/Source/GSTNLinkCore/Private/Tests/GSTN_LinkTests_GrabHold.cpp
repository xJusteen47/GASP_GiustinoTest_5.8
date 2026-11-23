// Links, pure core tests of grab-link (GSTNLink.Core.GrabHold.*): the neck hold anchor and the escape hit defaults.

#include "Data/GSTN_LinkKitExtension.h"
#include "GSTN_LinkTestUtils.h"
#include "Misc/AutomationTest.h"
#include "State/GSTN_LinkMath.h"
#include "State/GSTN_LinkState.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkGrabHoldNeckTest, "GSTNLink.Core.GrabHold.NeckOnHoldPoint", GSTNLinkTest::Flags)

bool FGSTN_LinkGrabHoldNeckTest::RunTest(const FString& Parameters)
{
	FGSTN_GrabHoldPose Pose;
	Pose.HoldPointCm = FVector(60.0, 15.0, 55.0);
	const FVector Neck(5.0, 0.0, 60.0);
	// The test grab type's target role: on the root, 90 cm ahead, turned to face the grabber
	const FTransform Offset(FRotator(0.0, 180.0, 0.0), FVector(90.0, 0.0, 0.0));
	const FTransform Grabber(FRotator(0.0, 37.0, 0.0), FVector(1000.0, -250.0, 92.0));

	for (const FTransform& Socket : { FTransform::Identity, FTransform(FRotator(10.0, -20.0, 5.0), FVector(30.0, 20.0, 40.0)) })
	{
		const FVector Anchor = UGSTN_LinkMath::NeckHoldAnchor(Socket, Offset, Neck, Pose);
		FGSTN_LinkState Parked;
		Parked.SetAnchorOverride(Anchor);
		// The same composition the snapshot, the watchdog and the presentation use (TryGetParkedTransform with the parked state)
		const FTransform Root = Parked.ResolveParkedOffset(Offset) * Socket * Grabber;
		const FVector NeckWorld = Root.TransformPositionNoScale(Neck);
		const FVector HoldWorld = Grabber.TransformPositionNoScale(Pose.HoldPointCm);
		// The anchor is rounded to whole cm on the wire: within a cm and a half (three rounded axes)
		TestTrue(*FString::Printf(TEXT("neck on the hold point (%.1f cm off)"), FVector::Dist(NeckWorld, HoldWorld)), FVector::Dist(NeckWorld, HoldWorld) < 1.5);
		TestTrue(TEXT("the grabbed still faces the grabber (the offset's rotation stays)"),
			FVector::DotProduct(Root.GetRotation().GetForwardVector(), Grabber.GetRotation().GetForwardVector()) < -0.5 || !Socket.Equals(FTransform::Identity));
	}

	// The nudge moves the whole body
	FGSTN_GrabHoldPose Nudged = Pose;
	Nudged.ExtraOffsetCm = FVector(0.0, 0.0, 10.0);
	const FVector Base = UGSTN_LinkMath::NeckHoldAnchor(FTransform::Identity, Offset, Neck, Pose);
	const FVector Up = UGSTN_LinkMath::NeckHoldAnchor(FTransform::Identity, Offset, Neck, Nudged);
	TestTrue(TEXT("extra offset lifts by 10 cm"), (Up - Base).Equals(FVector(0.0, 0.0, 10.0), 0.01));
	// Identity socket, yaw 180: anchor = hold - (-5, 0, 60) = (65, 15, -5)
	TestTrue(TEXT("worked example"), Base.Equals(FVector(65.0, 15.0, -5.0), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkGrabHoldEscapeDefaultsTest, "GSTNLink.Core.GrabHold.EscapeHitOffByDefault", GSTNLinkTest::Flags)

bool FGSTN_LinkGrabHoldEscapeDefaultsTest::RunTest(const FString& Parameters)
{
	// Owner 2026-11-23: damage on escape is per target type and off for players, so a kit extension that says nothing deals none
	const FGSTN_LinkKitExtension Kit;
	TestFalse(TEXT("escape hit off by default"), Kit.EscapeHit.bDamageGrabberOnEscape);
	TestFalse(TEXT("the escape hit cannot be parried"), Kit.EscapeHit.Hit.bParryable);
	TestFalse(TEXT("the kit keeps the project's hold pose"), Kit.bOverrideHoldPose);
	return true;
}

#endif
