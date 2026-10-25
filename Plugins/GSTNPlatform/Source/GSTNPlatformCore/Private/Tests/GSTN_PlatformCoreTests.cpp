// Automation tests of the pure platform data (many-enemies-replication.md 4.2, 4.4; plan T3 skeleton).

#include "GSTN_PlatformShape.h"
#include "GSTN_PlatformSpec.h"
#include "GSTN_PlatformTags.h"
#include "GSTN_PoseHistory.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPlatformCoreTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FGSTN_PlatformShape MakeBox(uint8 Id, const FVector& Extent = FVector(50.0, 50.0, 10.0))
	{
		FGSTN_PlatformShape Shape;
		Shape.ShapeId = Id;
		Shape.Type = EGSTN_PlatformShapeType::Box;
		Shape.Extent = Extent;
		return Shape;
	}
}

using namespace GSTNPlatformCoreTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformTagsDeclaredOnceTest, "GSTNPlatform.Core.TagsDeclaredOnce", Flags)
bool FGSTN_PlatformTagsDeclaredOnceTest::RunTest(const FString& Parameters)
{
	TArray<FGameplayTag> Tags;
	GSTNPlatformTags::GetDeclaredTags(Tags);
	TSet<FGameplayTag> Seen;
	for (const FGameplayTag& Tag : Tags)
	{
		TestTrue(FString::Printf(TEXT("%s is registered"), *Tag.ToString()), Tag.IsValid());
		TestTrue(FString::Printf(TEXT("%s is under Platform"), *Tag.ToString()), Tag.MatchesTag(GSTNPlatformTags::Root_Platform));
		bool bAlreadySeen = false;
		Seen.Add(Tag, &bAlreadySeen);
		TestFalse(FString::Printf(TEXT("%s is declared once"), *Tag.ToString()), bAlreadySeen);
	}
	TestEqual(TEXT("Every native platform tag is listed"), Tags.Num(), 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformShapeNamesTest, "GSTNPlatform.Core.ShapeComponentNames", Flags)
bool FGSTN_PlatformShapeNamesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Shape 0"), GSTNPlatform::GetShapeComponentName(0).ToString(), FString(TEXT("Surface0")));
	TestEqual(TEXT("Shape 7"), GSTNPlatform::GetShapeComponentName(7).ToString(), FString(TEXT("Surface7")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformSpecIdsTest, "GSTNPlatform.Core.SpecShapeIds", Flags)
bool FGSTN_PlatformSpecIdsTest::RunTest(const FString& Parameters)
{
	FGSTN_PlatformSpec Spec;
	Spec.Shapes = { MakeBox(0), MakeBox(3) };
	TArray<FText> Errors, Warnings;
	TestTrue(TEXT("Distinct ids in range are valid"), Spec.CollectFindings(Errors, Warnings));
	TestEqual(TEXT("Id mask"), int32(Spec.GetShapeIdMask()), 0b1001);
	TestEqual(TEXT("Find id 3"), Spec.FindShapeIndex(3), 1);
	TestEqual(TEXT("No id 2"), Spec.FindShapeIndex(2), INDEX_NONE);

	Spec.Shapes.Add(MakeBox(3));
	Errors.Reset();
	TestFalse(TEXT("A repeated id is an error"), Spec.CollectFindings(Errors, Warnings));

	Spec.Shapes = { MakeBox(8) };
	Errors.Reset();
	TestFalse(TEXT("Id 8 is out of range"), Spec.CollectFindings(Errors, Warnings));

	Spec.Shapes = { MakeBox(0, FVector(50.0, 0.0, 10.0)) };
	Errors.Reset();
	TestFalse(TEXT("A flat box is degenerate"), Spec.CollectFindings(Errors, Warnings));

	Spec.Shapes.Reset();
	for (uint8 Id = 0; Id < 9; ++Id)
	{
		Spec.Shapes.Add(MakeBox(Id % 8));
	}
	Errors.Reset();
	TestFalse(TEXT("Nine shapes are too many"), Spec.CollectFindings(Errors, Warnings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformShapeComposeTest, "GSTNPlatform.Core.ShapeComposeIgnoresScale", Flags)
bool FGSTN_PlatformShapeComposeTest::RunTest(const FString& Parameters)
{
	FGSTN_PlatformShape Shape = MakeBox(0);
	Shape.LocalLocation = FVector(100.0, 0.0, 50.0);
	const FTransform Owner(FRotator(0.0, 90.0, 0.0), FVector(1000.0, 0.0, 0.0), FVector(3.0));
	const FTransform World = Shape.ComposeWorld(Owner);
	TestTrue(TEXT("Local offset rotated by the owner's yaw, owner scale ignored"), World.GetLocation().Equals(FVector(1000.0, 100.0, 50.0), 1.e-6));
	TestTrue(TEXT("No scale"), World.GetScale3D().Equals(FVector::OneVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PoseHistorySampleTest, "GSTNPlatform.Core.PoseHistory.SampleExactFrame", Flags)
bool FGSTN_PoseHistorySampleTest::RunTest(const FString& Parameters)
{
	FGSTN_PoseHistory History;
	FGSTN_PoseSample Sample;
	TestFalse(TEXT("Empty"), History.Sample(0, Sample));
	History.Record(10, FTransform(FVector(1.0, 2.0, 3.0)), 0.016);
	TestTrue(TEXT("Frame 10 recorded"), History.Sample(10, Sample));
	TestTrue(TEXT("Location kept"), Sample.Location.Equals(FVector(1.0, 2.0, 3.0)));
	TestFalse(TEXT("Frame 9 missing"), History.Sample(9, Sample));
	TestEqual(TEXT("Latest frame"), History.GetLatestFrame(), 10);

	// A frame recorded again (re-simulation) overwrites; the latest frame does not go back
	History.Record(11, FTransform(FVector(5.0, 0.0, 0.0)), 0.016);
	History.Record(10, FTransform(FVector(7.0, 0.0, 0.0)), 0.016);
	TestTrue(TEXT("Frame 10 again"), History.Sample(10, Sample) && Sample.Location.Equals(FVector(7.0, 0.0, 0.0)));
	TestEqual(TEXT("Latest frame stays 11"), History.GetLatestFrame(), 11);

	// The ring keeps Capacity frames
	for (int32 Frame = 12; Frame < 12 + FGSTN_PoseHistory::Capacity; ++Frame)
	{
		History.Record(Frame, FTransform(FVector(double(Frame), 0.0, 0.0)), 0.016);
	}
	TestFalse(TEXT("Frame 11 dropped out of the ring"), History.Sample(11, Sample));
	TestTrue(TEXT("Oldest kept frame"), History.Sample(12, Sample) && Sample.Location.X == 12.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PoseHistoryVelocityTest, "GSTNPlatform.Core.PoseHistory.VelocityFromTwoFrames", Flags)
bool FGSTN_PoseHistoryVelocityTest::RunTest(const FString& Parameters)
{
	FGSTN_PoseHistory History;
	const double Dt = 0.016;
	FVector Velocity;
	History.Record(20, FTransform(FVector(0.0, 0.0, 0.0)), Dt);
	TestFalse(TEXT("One frame gives no velocity"), History.GetVelocityAtPoint(20, FVector::ZeroVector, Velocity));

	// 300 cm/s along +Y
	History.Record(21, FTransform(FVector(0.0, 300.0 * Dt, 0.0)), Dt);
	TestTrue(TEXT("Two frames give a velocity"), History.GetVelocityAtPoint(21, FVector(0.0, 300.0 * Dt, 0.0), Velocity));
	TestTrue(TEXT("Linear velocity"), Velocity.Equals(FVector(0.0, 300.0, 0.0), 1.e-6));

	// Turning about Z at 90 deg/s: a point 100 cm along +X of the origin moves at 100 * pi/2 cm/s along +Y
	const double Yaw = 90.0 * Dt;
	History.Record(30, FTransform(FQuat::Identity, FVector::ZeroVector), Dt);
	History.Record(31, FTransform(FRotator(0.0, Yaw, 0.0).Quaternion(), FVector::ZeroVector), Dt);
	TestTrue(TEXT("Rotation gives a velocity"), History.GetVelocityAtPoint(31, FVector(100.0, 0.0, 0.0), Velocity));
	TestTrue(TEXT("Point velocity of a turn"), Velocity.Equals(FVector(0.0, 100.0 * UE_DOUBLE_PI * 0.5, 0.0), 1.e-3));
	return true;
}

#endif
