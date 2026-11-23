// Gravity frame (gravity audit 2026-11-16): facing yaw about the local up, upright frames and earth-authored gravity carried into the
// entity's own gravity, pure. World-Z gravity must give the old world-yaw maths exactly; tilted, sideways and inverted gravity must keep
// every direction on the gravity floor.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Movement/GSTN_GravityFrame.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNGravityFrameTests
{
	const FVector Ups[] = {
		FVector(1.0, 0.0, 0.0),                   // sideways (a wall)
		FVector(0.0, 0.0, -1.0),                  // inverted (a ceiling)
		FVector(0.0, 1.0, 1.0).GetSafeNormal(),   // 45 degrees
		FVector(0.3, -0.5, 0.81).GetSafeNormal(), // arbitrary (a planet's side)
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GravityFrameWorldUpTest, "GSTNCombat.Core.GravityFrame.WorldUpUnchanged", GSTN_CORE_TEST_FLAGS)
bool FGSTN_GravityFrameWorldUpTest::RunTest(const FString& Parameters)
{
	const FVector Up = FVector::UpVector;
	TestTrue(TEXT("world up"), FGSTN_GravityFrame::IsWorldUp(Up));
	TestTrue(TEXT("zero up counts as world up"), FGSTN_GravityFrame::IsWorldUp(FVector::ZeroVector));
	const FVector Dir(3.0, -4.0, 7.0);
	TestEqual(TEXT("yaw of a direction = Rotation().Yaw"), FGSTN_GravityFrame::YawOfDirection(Dir, Up), static_cast<float>(Dir.Rotation().Yaw));
	const FRotator Rot(5.0, 10.0, -3.0);
	TestEqual(TEXT("yaw of a rotator = its Yaw"), FGSTN_GravityFrame::YawOfRotator(Rot, Up), 10.f);
	const FRotator Turned = FGSTN_GravityFrame::WithYaw(Rot, 200.f, Up);
	TestEqual(TEXT("pitch kept"), Turned.Pitch, 5.0);
	TestEqual(TEXT("roll kept"), Turned.Roll, -3.0);
	TestEqual(TEXT("yaw normalized"), Turned.Yaw, -160.0);
	TestTrue(TEXT("direction of yaw = FRotator(0, yaw, 0).Vector()"), FGSTN_GravityFrame::DirectionOfYaw(30.f, Up) == FRotator(0.0, 30.0, 0.0).Vector());
	TestTrue(TEXT("flatten drops Z"), FGSTN_GravityFrame::Flatten(Dir, Up).Equals(FVector(3.0, -4.0, 0.0)));
	const FVector Earth(0.0, 0.0, -980.0);
	TestTrue(TEXT("earth gravity keeps an authored throw exactly"), FGSTN_GravityFrame::FollowGravity(Earth, Earth) == Earth);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GravityFrameTiltedTest, "GSTNCombat.Core.GravityFrame.Tilted", GSTN_CORE_TEST_FLAGS)
bool FGSTN_GravityFrameTiltedTest::RunTest(const FString& Parameters)
{
	using namespace GSTNGravityFrameTests;
	for (const FVector& Up : Ups)
	{
		const FString U = Up.ToCompactString();
		TestFalse(*(U + TEXT(": not world up")), FGSTN_GravityFrame::IsWorldUp(Up));
		TestTrue(*(U + TEXT(": Mover's gravity frame")), FGSTN_GravityFrame::WorldToGravity(Up).Equals(FQuat::FindBetweenNormals(FVector::UpVector, Up), 1e-9));
		for (const float Yaw : {0.f, 37.f, 90.f, -135.f, 179.f})
		{
			const FString Y = FString::Printf(TEXT("%s yaw %.0f: "), *U, Yaw);
			const FVector Dir = FGSTN_GravityFrame::DirectionOfYaw(Yaw, Up);
			TestEqual(*(Y + TEXT("unit")), Dir.Size(), 1.0, 1e-6);
			TestEqual(*(Y + TEXT("on the gravity floor")), FVector::DotProduct(Dir, Up), 0.0, 1e-6);
			TestEqual(*(Y + TEXT("yaw round trip")), FMath::FindDeltaAngleDegrees(FGSTN_GravityFrame::YawOfDirection(Dir, Up), Yaw), 0.f, 1e-3f);
			// A climb along up does not change the yaw (the part along up is ignored)
			TestEqual(*(Y + TEXT("up part ignored")), FMath::FindDeltaAngleDegrees(FGSTN_GravityFrame::YawOfDirection(Dir * 3.0 + Up * 5.0, Up), Yaw), 0.f, 1e-3f);

			// Turning an upright capsule keeps it upright on that gravity and faces the yaw (what FGSTN_SetFacingEffect writes)
			const FRotator Upright = FGSTN_GravityFrame::UprightRotation(FQuat::Identity, Up).Rotator();
			const FQuat Turned = FGSTN_GravityFrame::WithYaw(Upright, Yaw, Up).Quaternion();
			TestTrue(*(Y + TEXT("capsule stays upright")), Turned.GetUpVector().Equals(Up, 1e-4));
			TestTrue(*(Y + TEXT("faces the yaw")), Turned.GetForwardVector().Equals(Dir, 1e-4));
			TestEqual(*(Y + TEXT("yaw read back")), FMath::FindDeltaAngleDegrees(FGSTN_GravityFrame::YawOfRotation(Turned, Up), Yaw), 0.f, 1e-3f);
			TestTrue(*(Y + TEXT("upright of the turned = the turned")), FGSTN_GravityFrame::UprightRotation(Turned, Up).GetForwardVector().Equals(Dir, 1e-4));
		}
		const FVector V(100.0, -40.0, 25.0);
		TestEqual(*(U + TEXT(": flatten on the floor")), FVector::DotProduct(FGSTN_GravityFrame::Flatten(V, Up), Up), 0.0, 1e-6);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GravityFrameFollowTest, "GSTNCombat.Core.GravityFrame.FollowGravity", GSTN_CORE_TEST_FLAGS)
bool FGSTN_GravityFrameFollowTest::RunTest(const FString& Parameters)
{
	const FVector Authored(0.0, 0.0, -980.0);
	TestTrue(TEXT("sideways: falls along -X"), FGSTN_GravityFrame::FollowGravity(Authored, FVector(-980.0, 0.0, 0.0)).Equals(FVector(-980.0, 0.0, 0.0), 1e-6));
	TestTrue(TEXT("inverted: falls along +Z"), FGSTN_GravityFrame::FollowGravity(Authored, FVector(0.0, 0.0, 980.0)).Equals(FVector(0.0, 0.0, 980.0), 1e-6));
	TestTrue(TEXT("low gravity: weaker"), FGSTN_GravityFrame::FollowGravity(Authored, FVector(0.0, 0.0, -245.0)).Equals(FVector(0.0, 0.0, -245.0), 1e-6));
	TestTrue(TEXT("zero gravity: straight"), FGSTN_GravityFrame::FollowGravity(Authored, FVector::ZeroVector).IsZero());
	// A lighter authored arc keeps its ratio to earth
	TestTrue(TEXT("authored half earth on a double-strength wall"), FGSTN_GravityFrame::FollowGravity(FVector(0.0, 0.0, -490.0), FVector(0.0, -1960.0, 0.0)).Equals(FVector(0.0, -980.0, 0.0), 1e-6));
	return true;
}

#endif
