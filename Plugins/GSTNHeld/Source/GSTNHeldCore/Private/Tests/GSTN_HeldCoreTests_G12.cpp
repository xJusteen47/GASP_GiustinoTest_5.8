// Held objects, pure core tests of the G12 presentation leftovers (GSTNHeld.Core.*): hand transform, release blend, arc preview math.

#include "GSTN_HeldTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldPresentationMathTest, "GSTNHeld.Core.PresentationMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldPresentationMathTest::RunTest(const FString& Parameters)
{
	// Hand: the offset is in the socket's space (socket turned 90 degrees in yaw: its X is the world's Y)
	const FTransform Socket(FRotator(0.f, 90.f, 0.f), FVector(100.0, 0.0, 50.0));
	const FTransform Hand = FGSTN_HeldRules::HandTransform(Socket, FVector(10.0, 0.0, 0.0), FRotator::ZeroRotator);
	TestTrue(TEXT("hand offset in socket space"), Hand.GetLocation().Equals(FVector(100.0, 10.0, 50.0), 0.01));
	TestTrue(TEXT("hand rotation follows the socket"), Hand.GetRotation().Equals(Socket.GetRotation(), 1.e-4f));

	// Blend: full at the start, zero at the end and beyond, half way at the middle (smoothstep), none without a duration
	const FVector Jump(0.0, 0.0, 40.0);
	TestTrue(TEXT("blend starts at the old place"), FGSTN_HeldRules::BlendOffset(Jump, 0.f, 0.1f).Equals(Jump));
	TestTrue(TEXT("blend half way"), FGSTN_HeldRules::BlendOffset(Jump, 0.05f, 0.1f).Equals(Jump * 0.5, 0.01));
	TestTrue(TEXT("blend done"), FGSTN_HeldRules::BlendOffset(Jump, 0.1f, 0.1f).IsZero());
	TestTrue(TEXT("no duration, no blend"), FGSTN_HeldRules::BlendOffset(Jump, 0.f, 0.f).IsZero());

	// Arc: closed form, first point at the origin, apex where the vertical speed is spent
	TArray<FVector> Points;
	FGSTN_HeldRules::ThrowArcPoints(FVector::ZeroVector, FVector(1000.0, 0.0, 490.0), FVector(0.0, 0.0, -980.0), 1.f, 10, Points);
	TestEqual(TEXT("steps + 1 points"), Points.Num(), 11);
	TestTrue(TEXT("starts at the origin"), Points[0].IsZero());
	TestTrue(TEXT("apex at t = 0.5 s"), Points[5].Equals(FVector(500.0, 0.0, 122.5), 0.01));
	TestTrue(TEXT("back at launch height at t = 1 s"), Points[10].Equals(FVector(1000.0, 0.0, 0.0), 0.01));
	FGSTN_HeldRules::ThrowArcPoints(FVector::ZeroVector, FVector::ForwardVector, FVector::ZeroVector, 1.f, 0, Points);
	TestEqual(TEXT("steps clamped to at least 1"), Points.Num(), 2);

	// Launch velocity: yaw then pitch, at speed
	const FVector Velocity = FGSTN_HeldRules::ThrowLaunchVelocity(90.f, 0.f, 1500.f);
	TestTrue(TEXT("yaw 90 = +Y"), Velocity.Equals(FVector(0.0, 1500.0, 0.0), 0.1));
	TestTrue(TEXT("pitch raises it"), FGSTN_HeldRules::ThrowLaunchVelocity(0.f, 30.f, 1000.f).Z > 499.f);

	// Shipped tuning: the hand socket is the mannequin's right hand
	TestEqual(TEXT("default hand socket"), FGSTN_HeldObjectTuning().HandSocket, FName(TEXT("hand_r")));
	return true;
}

#endif
