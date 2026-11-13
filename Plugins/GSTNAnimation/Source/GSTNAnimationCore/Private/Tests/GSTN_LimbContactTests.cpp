// Tests of the limb contact rules (limb-contact-ik.md): GSTNAnimation.Core.LimbContact.*
//
// Pure math, no world: a wall in front of a character facing +X (surface at X = 100, normal -X). The probe needs a world and is
// covered by the editor check list instead.

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_LimbContactUtils.h"
#include "Misc/AutomationTest.h"

namespace GSTNLimbContactTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Arm length used by the tests: upper arm + forearm of the mannequin, roughly */
	constexpr float ChainLength = 60.f;

	static FGSTN_LimbSurface Wall()
	{
		FGSTN_LimbSurface Surface;
		Surface.bValid = true;
		Surface.Point = FVector(100.f, 0.f, 0.f);
		Surface.Normal = FVector(-1.f, 0.f, 0.f);
		return Surface;
	}

	static FGSTN_LimbContactSettings MakeSettings()
	{
		FGSTN_LimbContactSettings Settings;
		Settings.ContactOffset = 3.f;
		Settings.AttachDistance = 8.f;
		Settings.ReleaseDistance = 25.f;
		Settings.bRequireReach = true;
		Settings.MaxReachRatio = 1.f;
		Settings.BlendInSpeed = 10.f;
		Settings.BlendOutSpeed = 5.f;
		return Settings;
	}

	/** Shoulder 50 cm from the wall */
	const FVector Shoulder(50.f, 20.f, 140.f);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LimbContactDistanceTest, "GSTNAnimation.Core.LimbContact.Distance", GSTNLimbContactTest::TestFlags)
bool FGSTN_LimbContactDistanceTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLimbContactTest;
	const FGSTN_LimbContactSettings Settings = MakeSettings();

	// 5 cm from the wall: gap 2, fully attached, slid along the normal only (Y and Z kept from the clip)
	const FGSTN_LimbContactResult Near = UGSTN_LimbContactUtils::ComputeContact(FVector(95.f, 30.f, 150.f), Shoulder, ChainLength, Wall(), Settings);
	TestTrue(TEXT("Near hand fully attached"), FMath::IsNearlyEqual(Near.TargetAlpha, 1.f));
	TestTrue(TEXT("Target at ContactOffset from the wall"), Near.TargetLocation.Equals(FVector(97.f, 30.f, 150.f), 0.01f));

	// Inside the wall: pushed out, fully attached
	const FGSTN_LimbContactResult Inside = UGSTN_LimbContactUtils::ComputeContact(FVector(104.f, 30.f, 150.f), Shoulder, ChainLength, Wall(), Settings);
	TestTrue(TEXT("Hand inside the wall attached"), FMath::IsNearlyEqual(Inside.TargetAlpha, 1.f));
	TestTrue(TEXT("Pushed out onto the contact plane"), FMath::IsNearlyEqual(Inside.TargetLocation.X, 97.f, 0.01f));
	TestTrue(TEXT("Negative gap inside"), Inside.Gap < 0.f);

	// Halfway in the fade band (gap 16.5 between 8 and 25): smoothstep(0.5) = 0.5
	const FGSTN_LimbContactResult Mid = UGSTN_LimbContactUtils::ComputeContact(FVector(80.5f, 30.f, 150.f), Shoulder, ChainLength, Wall(), Settings);
	TestTrue(TEXT("Fade band halfway"), FMath::IsNearlyEqual(Mid.TargetAlpha, 0.5f, 0.01f));

	// Beyond ReleaseDistance: the clip keeps the hand
	const FGSTN_LimbContactResult Far = UGSTN_LimbContactUtils::ComputeContact(FVector(70.f, 30.f, 150.f), Shoulder, ChainLength, Wall(), Settings);
	TestTrue(TEXT("Far hand left alone"), FMath::IsNearlyZero(Far.TargetAlpha));

	// No surface: left alone, target = animated location
	FGSTN_LimbSurface None = Wall();
	None.bValid = false;
	const FGSTN_LimbContactResult NoSurface = UGSTN_LimbContactUtils::ComputeContact(FVector(95.f, 30.f, 150.f), Shoulder, ChainLength, None, Settings);
	TestTrue(TEXT("Invalid surface gives 0"), FMath::IsNearlyZero(NoSurface.TargetAlpha));
	TestTrue(TEXT("Invalid surface keeps the animated location"), NoSurface.TargetLocation.Equals(FVector(95.f, 30.f, 150.f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LimbContactReachTest, "GSTNAnimation.Core.LimbContact.Reach", GSTNLimbContactTest::TestFlags)
bool FGSTN_LimbContactReachTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLimbContactTest;
	FGSTN_LimbContactSettings Settings = MakeSettings();

	// Shoulder closer to the wall than the hand (elbow out, hand pulled back): not reaching, released
	const FVector ShoulderAtWall(96.f, 20.f, 140.f);
	const FGSTN_LimbContactResult Back = UGSTN_LimbContactUtils::ComputeContact(FVector(90.f, 30.f, 120.f), ShoulderAtWall, ChainLength, Wall(), Settings);
	TestTrue(TEXT("Limb not reaching the wall is released"), FMath::IsNearlyZero(Back.TargetAlpha));

	Settings.bRequireReach = false;
	const FGSTN_LimbContactResult BackAllowed = UGSTN_LimbContactUtils::ComputeContact(FVector(90.f, 30.f, 120.f), ShoulderAtWall, ChainLength, Wall(), Settings);
	TestTrue(TEXT("Without bRequireReach it attaches"), FMath::IsNearlyEqual(BackAllowed.TargetAlpha, 1.f));

	// Target farther than the chain from the shoulder: released (no straight-arm snap)
	Settings.bRequireReach = true;
	const FGSTN_LimbContactResult TooFar = UGSTN_LimbContactUtils::ComputeContact(FVector(95.f, 70.f, 150.f), FVector(50.f, 20.f, 140.f), ChainLength, Wall(), Settings);
	TestTrue(TEXT("Out of reach is released"), FMath::IsNearlyZero(TooFar.TargetAlpha));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LimbContactBlendTest, "GSTNAnimation.Core.LimbContact.Blend", GSTNLimbContactTest::TestFlags)
bool FGSTN_LimbContactBlendTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLimbContactTest;
	FGSTN_LimbContactSettings Settings = MakeSettings();

	TestTrue(TEXT("Rises at BlendInSpeed"), FMath::IsNearlyEqual(UGSTN_LimbContactUtils::StepContactAlpha(0.f, 1.f, Settings, 0.05f), 0.5f));
	TestTrue(TEXT("Falls at BlendOutSpeed"), FMath::IsNearlyEqual(UGSTN_LimbContactUtils::StepContactAlpha(1.f, 0.f, Settings, 0.1f), 0.5f));
	TestTrue(TEXT("Does not overshoot"), FMath::IsNearlyEqual(UGSTN_LimbContactUtils::StepContactAlpha(0.9f, 1.f, Settings, 1.f), 1.f));
	TestTrue(TEXT("Target clamped to [0, 1]"), FMath::IsNearlyEqual(UGSTN_LimbContactUtils::StepContactAlpha(0.f, 3.f, Settings, 10.f), 1.f));

	Settings.BlendInSpeed = 0.f;
	TestTrue(TEXT("Speed 0 snaps"), FMath::IsNearlyEqual(UGSTN_LimbContactUtils::StepContactAlpha(0.2f, 0.8f, Settings, 0.01f), 0.8f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LimbContactAlignTest, "GSTNAnimation.Core.LimbContact.Align", GSTNLimbContactTest::TestFlags)
bool FGSTN_LimbContactAlignTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLimbContactTest;
	const FVector PalmAxis(0.f, 1.f, 0.f);

	// Identity: the palm points +Y; full alignment turns it into the wall (+X)
	const FQuat Full = UGSTN_LimbContactUtils::AlignAxisToSurface(FQuat::Identity, PalmAxis, Wall().Normal, 1.f);
	TestTrue(TEXT("Palm into the wall"), Full.RotateVector(PalmAxis).Equals(FVector(1.f, 0.f, 0.f), 1.e-3f));

	const FQuat Zero = UGSTN_LimbContactUtils::AlignAxisToSurface(FQuat::Identity, PalmAxis, Wall().Normal, 0.f);
	TestTrue(TEXT("Alpha 0 keeps the rotation"), Zero.Equals(FQuat::Identity));

	const FQuat Half = UGSTN_LimbContactUtils::AlignAxisToSurface(FQuat::Identity, PalmAxis, Wall().Normal, 0.5f);
	const float HalfAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Half.RotateVector(PalmAxis), FVector(0.f, 1.f, 0.f))));
	TestTrue(TEXT("Alpha 0.5 turns halfway (45 deg)"), FMath::IsNearlyEqual(HalfAngle, 45.f, 0.1f));

	const FQuat Degenerate = UGSTN_LimbContactUtils::AlignAxisToSurface(FQuat::Identity, FVector::ZeroVector, Wall().Normal, 1.f);
	TestTrue(TEXT("Zero axis keeps the rotation"), Degenerate.Equals(FQuat::Identity));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LimbContactSpaceTest, "GSTNAnimation.Core.LimbContact.ComponentSpace", GSTNLimbContactTest::TestFlags)
bool FGSTN_LimbContactSpaceTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLimbContactTest;

	// Component at (1000, 0, 0) turned 90 deg around Z: the round trip back to world must give the same point and normal
	const FTransform Component(FRotator(0.f, 90.f, 0.f), FVector(1000.f, 0.f, 0.f));
	const FGSTN_LimbSurface Local = UGSTN_LimbContactUtils::InverseTransformSurface(Wall(), Component);
	TestTrue(TEXT("Validity kept"), Local.bValid);
	TestTrue(TEXT("Point to component space"), Component.TransformPosition(Local.Point).Equals(Wall().Point, 0.01f));
	TestTrue(TEXT("Normal to component space"), Component.TransformVectorNoScale(Local.Normal).Equals(Wall().Normal, 1.e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LimbContactPreviewWallTest, "GSTNAnimation.Core.LimbContact.SurfaceFromTransform", GSTNLimbContactTest::TestFlags)
bool FGSTN_LimbContactPreviewWallTest::RunTest(const FString& Parameters)
{
	// A preview wall control 45 cm in front of a mannequin (component forward +Y), turned so its X axis faces the character (-Y)
	const FTransform Control(FRotator(0.f, -90.f, 0.f), FVector(0.f, 45.f, 100.f));
	const FGSTN_LimbSurface Surface = UGSTN_LimbContactUtils::SurfaceFromTransform(Control);
	TestTrue(TEXT("Valid"), Surface.bValid);
	TestTrue(TEXT("Point at the control"), Surface.Point.Equals(FVector(0.f, 45.f, 100.f), 0.01f));
	TestTrue(TEXT("Normal is the control's X axis"), Surface.Normal.Equals(FVector(0.f, -1.f, 0.f), 1.e-3f));
	return true;
}

#endif
