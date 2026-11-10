// Held objects, pure core tests (GSTNHeld.Core.*).

#include "GSTN_HeldTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldPickupRaceTest, "GSTNHeld.Core.PickupRace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldPickupRaceTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PickupAssignment> Out;
	const FGSTN_PickupCandidate Crate{ 0, FVector(0.0, 0.0, 0.0), 200.f, true };

	// Same frame: the lower holder id wins, whatever the arrival order
	FGSTN_HeldRules::ResolvePickups({ { 7, 100, FVector(50.0, 0.0, 0.0) }, { 3, 100, FVector(150.0, 0.0, 0.0) } }, { Crate }, Out);
	TestEqual(TEXT("same frame: one assignment"), Out.Num(), 1);
	TestTrue(TEXT("same frame: lower holder id wins"), Out.Num() == 1 && Out[0] == FGSTN_PickupAssignment{ 3, 0 });

	// Earlier frame wins over a lower id
	FGSTN_HeldRules::ResolvePickups({ { 3, 101, FVector(50.0, 0.0, 0.0) }, { 7, 100, FVector(150.0, 0.0, 0.0) } }, { Crate }, Out);
	TestTrue(TEXT("earlier frame wins"), Out.Num() == 1 && Out[0] == FGSTN_PickupAssignment{ 7, 0 });

	// Out of the object's radius: nothing
	FGSTN_HeldRules::ResolvePickups({ { 1, 100, FVector(250.0, 0.0, 0.0) } }, { Crate }, Out);
	TestEqual(TEXT("out of radius: none"), Out.Num(), 0);

	// Unavailable (held or flying): nothing
	FGSTN_PickupCandidate Held = Crate;
	Held.bAvailable = false;
	FGSTN_HeldRules::ResolvePickups({ { 1, 100, FVector::ZeroVector } }, { Held }, Out);
	TestEqual(TEXT("unavailable: none"), Out.Num(), 0);

	// Two objects: the winner takes the nearest, the loser the other one if it is in range
	const FGSTN_PickupCandidate Barrel{ 1, FVector(100.0, 0.0, 0.0), 200.f, true };
	FGSTN_HeldRules::ResolvePickups({ { 2, 100, FVector(90.0, 0.0, 0.0) }, { 5, 100, FVector(95.0, 0.0, 0.0) } }, { Crate, Barrel }, Out);
	TestTrue(TEXT("two objects: two assignments"), Out.Num() == 2 && Out[0] == FGSTN_PickupAssignment{ 2, 1 } && Out[1] == FGSTN_PickupAssignment{ 5, 0 });

	// Equal distance: the lower object id
	const FGSTN_PickupCandidate Left{ 4, FVector(0.0, -50.0, 0.0), 200.f, true };
	const FGSTN_PickupCandidate Right{ 2, FVector(0.0, 50.0, 0.0), 200.f, true };
	FGSTN_HeldRules::ResolvePickups({ { 1, 100, FVector::ZeroVector } }, { Left, Right }, Out);
	TestTrue(TEXT("equal distance: lower object id"), Out.Num() == 1 && Out[0] == FGSTN_PickupAssignment{ 1, 2 });

	// A holder gets one object, even with two requests
	FGSTN_HeldRules::ResolvePickups({ { 1, 100, FVector::ZeroVector }, { 1, 101, FVector::ZeroVector } }, { Crate, Barrel }, Out);
	TestEqual(TEXT("one object per holder"), Out.Num(), 1);

	// Slice 5: a focused target wins over the nearest; a target taken by an earlier request sends the later one to the nearest other object
	FGSTN_PickupRequest Focused{ 2, 100, FVector(90.0, 0.0, 0.0) };
	Focused.TargetObjectId = 0;
	FGSTN_HeldRules::ResolvePickups({ Focused }, { Crate, Barrel }, Out);
	TestTrue(TEXT("target: the focused object, not the nearest"), Out.Num() == 1 && Out[0] == FGSTN_PickupAssignment{ 2, 0 });
	FGSTN_PickupRequest Late{ 5, 101, FVector(95.0, 0.0, 0.0) };
	Late.TargetObjectId = 0;
	FGSTN_HeldRules::ResolvePickups({ Focused, Late }, { Crate, Barrel }, Out);
	TestTrue(TEXT("target taken: the later request gets the other object"), Out.Num() == 2 && Out[0] == FGSTN_PickupAssignment{ 2, 0 }
		&& Out[1] == FGSTN_PickupAssignment{ 5, 1 });
	FGSTN_HeldRules::ResolvePickups({ Focused, Late }, { Crate }, Out);
	TestTrue(TEXT("target taken, nothing else in reach: a fumble"), Out.Num() == 1 && Out[0] == FGSTN_PickupAssignment{ 2, 0 });
	FGSTN_PickupRequest Missing{ 2, 100, FVector::ZeroVector };
	Missing.TargetObjectId = 9;
	FGSTN_HeldRules::ResolvePickups({ Missing }, { Crate, Barrel }, Out);
	TestTrue(TEXT("unknown target: the nearest"), Out.Num() == 1 && Out[0] == FGSTN_PickupAssignment{ 2, 0 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldHoldTransformTest, "GSTNHeld.Core.HoldTransformAndDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldHoldTransformTest::RunTest(const FString& Parameters)
{
	const FTransform Holder(FRotator(0.0, 90.0, 0.0), FVector(100.0, 200.0, 50.0));
	const FTransform Hold = FGSTN_HeldRules::HoldTransform(Holder, FVector(45.0, 0.0, 70.0));
	TestTrue(TEXT("offset turns with the holder (forward = +Y at yaw 90)"), Hold.GetLocation().Equals(FVector(100.0, 245.0, 120.0), 0.001));
	TestTrue(TEXT("rotation = the holder's"), Hold.GetRotation().Equals(Holder.GetRotation(), 1e-6));

	const FGSTN_HeldObjectTuning Tuning;
	TestTrue(TEXT("default throw has gravity"), Tuning.Throw.GravityAccel.Z < 0.0);
	TestTrue(TEXT("default throw hits"), Tuning.Throw.Hit.Damage > 0.f && Tuning.Throw.MaxHits == 1);
	return true;
}

#endif
