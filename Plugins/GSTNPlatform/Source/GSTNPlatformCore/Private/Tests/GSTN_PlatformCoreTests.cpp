// Automation tests of the pure platform data (many-enemies-replication.md 4.2, 4.4; plan T3 skeleton).

#include "GSTN_PlatformShape.h"
#include "GSTN_PlatformSpec.h"
#include "GSTN_PlatformTags.h"
#include "GSTN_SurfaceTags.h"
#include "GSTN_PoseHistory.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPlatformCoreTest
{
	constexpr EAutomationTestFlags PlatformCoreTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FGSTN_PlatformShape MakeBox(uint8 Id, const FVector& Extent = FVector(50.0, 50.0, 10.0))
	{
		FGSTN_PlatformShape Shape;
		Shape.ShapeId = Id;
		Shape.Type = EGSTN_PlatformShapeType::Box;
		Shape.Extent = Extent;
		return Shape;
	}

	/** A box that offers the given behaviours, so the validation has something to judge */
	FGSTN_PlatformShape MakeBehaviourBox(uint8 Id, const FGameplayTag& Behaviour)
	{
		FGSTN_PlatformShape Shape = MakeBox(Id);
		Shape.Behaviour.Behaviours.AddTag(Behaviour);
		return Shape;
	}

	bool AnyContains(const TArray<FText>& Texts, const TCHAR* Needle)
	{
		return Texts.ContainsByPredicate([Needle](const FText& Text) { return Text.ToString().Contains(Needle); });
	}
}

using namespace GSTNPlatformCoreTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformTagsDeclaredOnceTest, "GSTNPlatform.Core.TagsDeclaredOnce", PlatformCoreTestFlags)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformShapeNamesTest, "GSTNPlatform.Core.ShapeComponentNames", PlatformCoreTestFlags)
bool FGSTN_PlatformShapeNamesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Shape 0"), GSTNPlatform::GetShapeComponentName(0).ToString(), FString(TEXT("Surface0")));
	TestEqual(TEXT("Shape 7"), GSTNPlatform::GetShapeComponentName(7).ToString(), FString(TEXT("Surface7")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformSpecIdsTest, "GSTNPlatform.Core.SpecShapeIds", PlatformCoreTestFlags)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformShapeComposeTest, "GSTNPlatform.Core.ShapeComposeIgnoresScale", PlatformCoreTestFlags)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PoseHistorySampleTest, "GSTNPlatform.Core.PoseHistory.SampleExactFrame", PlatformCoreTestFlags)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PoseHistoryVelocityTest, "GSTNPlatform.Core.PoseHistory.VelocityFromTwoFrames", PlatformCoreTestFlags)
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

// Pure validation of the spec (traversal-surfaces plan T9): no world, so it runs headless. The wall angle range is a warning, never an error:
// a platform may be authored for a pose the enemy only reaches later.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformValidationWallAngleTest, "GSTNPlatform.Core.Validation.WallAngleRange", PlatformCoreTestFlags)
bool FGSTN_PlatformValidationWallAngleTest::RunTest(const FString& Parameters)
{
	FGSTN_PlatformSpec Spec;
	Spec.Shapes = { MakeBehaviourBox(0, GSTNSurfaceTags::Behaviour_WallSlide) };

	// An unrotated box has faces at 0, 90 and 180 degrees from up: the default range (45..120) holds the 90
	TArray<FText> Errors, Warnings;
	TestTrue(TEXT("A wall slide box passes"), Spec.CollectFindings(Errors, Warnings));
	TestFalse(TEXT("and has a face in the range"), AnyContains(Warnings, TEXT("degrees")));

	// A range that holds none of 0, 90, 180: the shape is never grabbed, which is a warning that names the range
	FGSTN_PlatformValidationContext Narrow;
	Narrow.MinWallSurfaceAngle = 100.0;
	Narrow.MaxWallSurfaceAngle = 110.0;
	Errors.Reset();
	Warnings.Reset();
	TestTrue(TEXT("Still not an error"), Spec.CollectFindings(Errors, Warnings, Narrow));
	TestTrue(TEXT("A warning says no face is in the range"), AnyContains(Warnings, TEXT("no face is within")));

	// Stand looks at the walkable angle instead: the same box has a top at 0
	Spec.Shapes = { MakeBehaviourBox(0, GSTNSurfaceTags::Behaviour_Stand) };
	Errors.Reset();
	Warnings.Reset();
	Spec.CollectFindings(Errors, Warnings);
	TestFalse(TEXT("A Stand box has a walkable top"), AnyContains(Warnings, TEXT("no face is walkable")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformValidationShapeDataTest, "GSTNPlatform.Core.Validation.ShapeData", PlatformCoreTestFlags)
bool FGSTN_PlatformValidationShapeDataTest::RunTest(const FString& Parameters)
{
	FGSTN_PlatformSpec Spec;
	FGSTN_PlatformShape Flat = MakeBehaviourBox(0, GSTNSurfaceTags::Behaviour_Stand);
	Flat.Extent = FVector(50.0, 0.0, 10.0);
	FGSTN_PlatformShape Twin = MakeBehaviourBox(0, GSTNSurfaceTags::Behaviour_Stand);
	FGSTN_PlatformShape Nothing = MakeBox(1);
	Spec.Shapes = { Flat, Twin, Nothing };
	Spec.DropGraceFrames = -1;

	TArray<FText> Errors, Warnings;
	TestFalse(TEXT("A bad spec is not valid"), Spec.CollectFindings(Errors, Warnings));
	TestTrue(TEXT("a degenerate extent is an error"), AnyContains(Errors, TEXT("degenerate extent")));
	TestTrue(TEXT("a repeated ShapeId is an error"), AnyContains(Errors, TEXT("used twice")));
	TestTrue(TEXT("a negative grace is an error"), AnyContains(Errors, TEXT("DropGraceFrames must not be negative")));
	TestTrue(TEXT("a shape that offers nothing is a warning"), AnyContains(Warnings, TEXT("offers no traversal behaviour")));

	FGSTN_PlatformSpec TooMany;
	for (int32 Index = 0; Index <= GSTNPlatform::MaxShapes; ++Index)
	{
		TooMany.Shapes.Add(MakeBehaviourBox(uint8(Index), GSTNSurfaceTags::Behaviour_Stand));
	}
	Errors.Reset();
	Warnings.Reset();
	TestFalse(TEXT("More shapes than the mask holds is invalid"), TooMany.CollectFindings(Errors, Warnings));
	TestTrue(TEXT("with the limit in the message"), AnyContains(Errors, TEXT("at most")));
	return true;
}

#endif
