// Launch readability (GSTN_LaunchReadability.h, owner 2026-11-24 3A): the view math of the launch zoom and the off-screen arrow. Pure: no world.

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/GSTN_LaunchReadability.h"
#include "Camera/CameraTypes.h"
#include "Misc/AutomationTest.h"

namespace GSTNLaunchReadabilityTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	static FMinimalViewInfo SquareView()
	{
		FMinimalViewInfo View;
		View.Location = FVector::ZeroVector;
		View.Rotation = FRotator::ZeroRotator;
		View.FOV = 90.f;      // tan(45) = 1: the frustum edge is at |y| = x
		View.AspectRatio = 1.f;
		return View;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LaunchZoomPushBackTest, "GSTN.Camera.Launch.PushBackToFit", GSTNLaunchReadabilityTest::TestFlags)
bool FGSTN_LaunchZoomPushBackTest::RunTest(const FString& Parameters)
{
	const FMinimalViewInfo View = GSTNLaunchReadabilityTest::SquareView();
	TestEqual(TEXT("in frame: no push back"), FGSTN_LaunchViewMath::PushBackToFit(View, FVector(1000, 300, 0), 1.f), 0.f);
	// 1000 ahead, 1500 to the side: the edge needs x = 1500, so 500 back
	TestEqual(TEXT("to the side: backs off until it fits"), FGSTN_LaunchViewMath::PushBackToFit(View, FVector(1000, 1500, 0), 1.f), 500.f, 0.01f);
	// With a 0.5 margin the side must be within half the frustum: x = 3000
	TestEqual(TEXT("margin narrows the frustum"), FGSTN_LaunchViewMath::PushBackToFit(View, FVector(1000, 1500, 0), 0.5f), 2000.f, 0.01f);
	// Above: same rule on the vertical half angle (aspect 1)
	TestEqual(TEXT("above"), FGSTN_LaunchViewMath::PushBackToFit(View, FVector(1000, 0, 1200), 1.f), 200.f, 0.01f);
	// Wide screen: the vertical half angle is narrower (tan = 1 / aspect)
	FMinimalViewInfo Wide = View;
	Wide.AspectRatio = 2.f;
	TestEqual(TEXT("wide screen vertical"), FGSTN_LaunchViewMath::PushBackToFit(Wide, FVector(1000, 0, 600), 1.f), 200.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LaunchArrowTest, "GSTN.Camera.Launch.OffscreenArrow", GSTNLaunchReadabilityTest::TestFlags)
bool FGSTN_LaunchArrowTest::RunTest(const FString& Parameters)
{
	const FMinimalViewInfo View = GSTNLaunchReadabilityTest::SquareView();
	const FVector2D Screen(1000.0, 1000.0);
	FVector2D Position;
	FVector2D Direction;
	TestFalse(TEXT("on screen: no arrow"), FGSTN_LaunchViewMath::OffscreenArrow(View, FVector(1000, 200, 100), Screen, 50.f, Position, Direction));

	TestTrue(TEXT("off to the right: arrow"), FGSTN_LaunchViewMath::OffscreenArrow(View, FVector(1000, 3000, 0), Screen, 50.f, Position, Direction));
	TestEqual(TEXT("right edge, inset"), Position.X, 950.0, 0.01);
	TestEqual(TEXT("vertically centred"), Position.Y, 500.0, 0.01);
	TestEqual(TEXT("points right"), Direction.X, 1.0, 0.001);

	TestTrue(TEXT("above: arrow"), FGSTN_LaunchViewMath::OffscreenArrow(View, FVector(1000, 0, 3000), Screen, 50.f, Position, Direction));
	TestEqual(TEXT("top edge (y down)"), Position.Y, 50.0, 0.01);
	TestEqual(TEXT("points up"), Direction.Y, -1.0, 0.001);

	TestTrue(TEXT("behind on the left: arrow"), FGSTN_LaunchViewMath::OffscreenArrow(View, FVector(-500, -400, 0), Screen, 50.f, Position, Direction));
	TestEqual(TEXT("left edge"), Position.X, 50.0, 0.01);

	TestTrue(TEXT("P1 red"), FGSTN_LaunchViewMath::PlayerColor(1).R > 0.9f && FGSTN_LaunchViewMath::PlayerColor(1).B < 0.2f);
	TestTrue(TEXT("P2 blue"), FGSTN_LaunchViewMath::PlayerColor(2).B > 0.9f);
	return true;
}

#endif
