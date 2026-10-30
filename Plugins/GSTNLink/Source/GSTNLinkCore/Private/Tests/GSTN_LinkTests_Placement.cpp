// Automation tests of the link plugin: exit placers (plan F5b).

#include "GSTN_LinkTestUtils.h"
#include "Misc/AutomationTest.h"
#include "Placement/GSTN_Placer_AtSocket.h"
#include "Placement/GSTN_Placer_Ring.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_ExitQuery MakeQuery()
	{
		FGSTN_ExitQuery Query;
		Query.PartnerTransform = FTransform(FRotator::ZeroRotator, FVector(100.0, 0.0, 0.0));
		Query.PreferredDirection = FVector(1.0, 0.0, 0.0);
		Query.Up = FVector::UpVector;
		return Query;
	}

	UGSTN_Placer_Ring* MakeRing()
	{
		UGSTN_Placer_Ring* Ring = NewObject<UGSTN_Placer_Ring>(GetTransientPackage());
		Ring->RadiusCm = 120.f;
		return Ring;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPlacerRingOrderTest, "GSTNLink.Core.Placer.RingOrder", GSTNLinkTest::Flags)
bool FGSTN_LinkPlacerRingOrderTest::RunTest(const FString& Parameters)
{
	const UGSTN_Placer_Ring* Ring = MakeRing();
	FGSTN_ExitQuery Query = MakeQuery();

	// No world to ask: the first candidate, the preferred direction, wins
	const FGSTN_ExitResult First = Ring->Place(Query);
	TestTrue(TEXT("A free first candidate is valid"), First.bValid);
	TestEqual(TEXT("It is candidate 0"), First.CandidateIndex, 0);
	TestTrue(TEXT("It is ahead of the partner at the ring radius"), First.Transform.GetLocation().Equals(FVector(220.0, 0.0, 0.0), 0.001));
	TestTrue(TEXT("The entity lands facing the partner"), First.Transform.GetUnitAxis(EAxis::X).Equals(FVector(-1.0, 0.0, 0.0), 0.001));
	TestTrue(TEXT("It leaves at rest by default"), First.Velocity.IsNearlyZero());

	// The eight ring directions turn counter-clockwise in 45 degree steps from the preferred one
	for (int32 Index = 0; Index < UGSTN_Placer_Ring::DirectionCount; ++Index)
	{
		const double Angle = FMath::DegreesToRadians(45.0 * Index);
		const FVector Expected(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
		TestTrue(FString::Printf(TEXT("Direction %d"), Index), UGSTN_Placer_Ring::RingDirection(Query, Index).Equals(Expected, 0.0001));
	}

	// Blocking the first k candidates makes candidate k win: the order is fixed
	GSTNLinkTest::FFakeExitQuery World;
	Query.Collision = &World;
	for (int32 Blocked = 0; Blocked < UGSTN_Placer_Ring::DirectionCount; ++Blocked)
	{
		World.IsBlocked = [&Query, Blocked](const FVector& Location)
		{
			for (int32 Index = 0; Index < Blocked; ++Index)
			{
				const FVector Candidate = Query.PartnerTransform.GetLocation() + UGSTN_Placer_Ring::RingDirection(Query, Index) * 120.0;
				if (Location.Equals(Candidate, 0.01))
				{
					return true;
				}
			}
			return false;
		};
		const FGSTN_ExitResult Result = Ring->Place(Query);
		TestEqual(FString::Printf(TEXT("With %d blocked, candidate %d wins"), Blocked, Blocked), Result.CandidateIndex, Blocked);
		TestTrue(FString::Printf(TEXT("Candidate %d is valid"), Blocked), Result.bValid);
	}

	// The preferred direction is honoured, projected on the plane of the up
	Query.PreferredDirection = FVector(0.0, 1.0, 5.0);
	World.IsBlocked = nullptr;
	TestTrue(TEXT("A tilted preferred direction is projected flat"), Ring->Place(Query).Transform.GetLocation().Equals(FVector(100.0, 120.0, 0.0), 0.001));

	// No preferred direction: the partner's forward
	Query.PreferredDirection = FVector::ZeroVector;
	Query.PartnerTransform = FTransform(FRotator(0.0, 90.0, 0.0), FVector(100.0, 0.0, 0.0));
	TestTrue(TEXT("A zero preference uses the partner's forward"), Ring->Place(Query).Transform.GetLocation().Equals(FVector(100.0, 120.0, 0.0), 0.001));

	// Any gravity: the ring turns about the local up
	Query.PartnerTransform = FTransform::Identity;
	Query.PreferredDirection = FVector(1.0, 0.0, 0.0);
	Query.Up = FVector(1.0, 0.0, 0.0);
	const FVector Along = UGSTN_Placer_Ring::RingDirection(Query, 3);
	TestTrue(TEXT("With the up along X every ring direction is perpendicular to it"), FMath::IsNearlyZero(Along | Query.Up, 0.0001) && Along.IsNormalized());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPlacerBlockedFallbackTest, "GSTNLink.Core.Placer.BlockedCandidatesFallback", GSTNLinkTest::Flags)
bool FGSTN_LinkPlacerBlockedFallbackTest::RunTest(const FString& Parameters)
{
	const UGSTN_Placer_Ring* Ring = MakeRing();
	FGSTN_ExitQuery Query = MakeQuery();
	GSTNLinkTest::FFakeExitQuery World;
	Query.Collision = &World;

	// The ring is blocked, the air above is free: the candidate above wins
	const double PartnerZ = Query.PartnerTransform.GetLocation().Z;
	World.IsBlocked = [PartnerZ](const FVector& Location) { return FMath::IsNearlyEqual(Location.Z, PartnerZ, 1.0); };
	const FGSTN_ExitResult Above = Ring->Place(Query);
	TestTrue(TEXT("Above is valid"), Above.bValid);
	TestEqual(TEXT("It is the candidate after the ring"), Above.CandidateIndex, UGSTN_Placer_Ring::AboveIndex);
	TestTrue(TEXT("A capsule height and its clearance over the partner"), FMath::IsNearlyEqual(Above.Transform.GetLocation().Z, 88.0 * 2.0 + 20.0, 0.001));
	TestTrue(TEXT("Straight above"), FMath::IsNearlyEqual(Above.Transform.GetLocation().X, 100.0, 0.001));

	// Everything is blocked: the fallback is next to the partner, lifted, and reported invalid
	World.IsBlocked = [](const FVector&) { return true; };
	World.QueryCount = 0;
	const FGSTN_ExitResult Fallback = Ring->Place(Query);
	TestFalse(TEXT("The fallback is invalid"), Fallback.bValid);
	TestEqual(TEXT("It names no candidate"), Fallback.CandidateIndex, static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("It is the partner's position plus the lift"), Fallback.Transform.GetLocation().Equals(FVector(100.0, 0.0, Ring->FallbackLiftCm), 0.001));
	TestEqual(TEXT("Every candidate was asked once: eight directions and above"), World.QueryCount, UGSTN_Placer_Ring::DirectionCount + 1);

	// A placer is never asked more than it needs
	World.IsBlocked = nullptr;
	World.QueryCount = 0;
	Ring->Place(Query);
	TestEqual(TEXT("A free first candidate costs one question"), World.QueryCount, 1);

	// The knockback of a throw does not reach a ring placer
	Query.KnockbackVelocity = FVector(0.0, 0.0, 900.0);
	TestTrue(TEXT("A ring exit ignores the throw's velocity"), Ring->Place(Query).Velocity.IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPlacerAtSocketVelocityTest, "GSTNLink.Core.Placer.AtSocketVelocity", GSTNLinkTest::Flags)
bool FGSTN_LinkPlacerAtSocketVelocityTest::RunTest(const FString& Parameters)
{
	UGSTN_Placer_AtSocket* AtSocket = NewObject<UGSTN_Placer_AtSocket>(GetTransientPackage());
	FGSTN_ExitQuery Query = MakeQuery();
	Query.SocketTransform = FTransform(FRotator(0.0, 30.0, 0.0), FVector(500.0, 50.0, 200.0));
	Query.KnockbackVelocity = FVector(300.0, 0.0, 400.0);

	const FGSTN_ExitResult Result = AtSocket->Place(Query);
	TestTrue(TEXT("The socket position is free"), Result.bValid);
	TestEqual(TEXT("It is the first candidate"), Result.CandidateIndex, 0);
	TestTrue(TEXT("The entity leaves from the socket"), Result.Transform.GetLocation().Equals(FVector(500.0, 50.0, 200.0), 0.001));
	TestTrue(TEXT("Facing as the socket"), Result.Transform.GetRotation().Equals(Query.SocketTransform.GetRotation(), 0.0001));
	TestTrue(TEXT("The velocity is the throw's knockback"), Result.Velocity.Equals(Query.KnockbackVelocity, 0.001));

	// The partner's own transform plays no part in the velocity
	Query.PartnerTransform = FTransform(FRotator(0.0, 180.0, 0.0), FVector(-900.0, 0.0, 0.0));
	TestTrue(TEXT("Still the knockback"), AtSocket->Place(Query).Velocity.Equals(Query.KnockbackVelocity, 0.001));

	// A blocked socket raises the capsule a nudge at a time
	GSTNLinkTest::FFakeExitQuery World;
	World.IsBlocked = [](const FVector& Location) { return Location.Z < 214.0; };
	Query.Collision = &World;
	const FGSTN_ExitResult Nudged = AtSocket->Place(Query);
	TestTrue(TEXT("A raised retry fits"), Nudged.bValid);
	TestEqual(TEXT("It is the first raised retry"), Nudged.CandidateIndex, 1);
	TestTrue(TEXT("Raised by one nudge along the up"), FMath::IsNearlyEqual(Nudged.Transform.GetLocation().Z, 215.0, 0.001));
	TestTrue(TEXT("The velocity is still the knockback"), Nudged.Velocity.Equals(Query.KnockbackVelocity, 0.001));

	// Nothing fits: the fallback beside the partner, invalid, but the throw still launches
	World.IsBlocked = [](const FVector&) { return true; };
	const FGSTN_ExitResult Fallback = AtSocket->Place(Query);
	TestFalse(TEXT("No room: invalid"), Fallback.bValid);
	TestTrue(TEXT("The fallback is next to the partner"), Fallback.Transform.GetLocation().Equals(FVector(-900.0, 0.0, AtSocket->FallbackLiftCm), 0.001));
	TestTrue(TEXT("The throw still launches"), Fallback.Velocity.Equals(Query.KnockbackVelocity, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPlacerDeterministicTest, "GSTNLink.Core.Placer.DeterministicForEqualInputs", GSTNLinkTest::Flags)
bool FGSTN_LinkPlacerDeterministicTest::RunTest(const FString& Parameters)
{
	FGSTN_ExitQuery Query = MakeQuery();
	Query.PreferredDirection = FVector(0.3, -0.8, 0.1);
	Query.SocketTransform = FTransform(FRotator(10.0, 77.0, 3.0), FVector(12.5, -7.25, 301.0));
	Query.KnockbackVelocity = FVector(111.0, -22.0, 333.0);

	// A world that blocks an arbitrary but fixed set of places
	GSTNLinkTest::FFakeExitQuery World;
	World.IsBlocked = [](const FVector& Location) { return FMath::Fmod(Location.X + Location.Y * 3.0, 7.0) < 3.0; };
	Query.Collision = &World;

	const UGSTN_Placer_Ring* RingA = MakeRing();
	const UGSTN_Placer_Ring* RingB = MakeRing();
	const FGSTN_ExitResult First = RingA->Place(Query);
	bool bSame = true;
	for (int32 Repeat = 0; Repeat < 5; ++Repeat)
	{
		const FGSTN_ExitResult Again = (Repeat % 2 == 0 ? RingA : RingB)->Place(Query);
		bSame &= GSTNLinkTest::ExactlyEqual(Again.Transform, First.Transform) && Again.Velocity == First.Velocity
			&& Again.bValid == First.bValid && Again.CandidateIndex == First.CandidateIndex;
	}
	TestTrue(TEXT("The ring gives bit-equal results for equal inputs, on one instance or two"), bSame);

	const UGSTN_Placer_AtSocket* SocketA = NewObject<UGSTN_Placer_AtSocket>(GetTransientPackage());
	const UGSTN_Placer_AtSocket* SocketB = NewObject<UGSTN_Placer_AtSocket>(GetTransientPackage());
	const FGSTN_ExitResult SocketFirst = SocketA->Place(Query);
	bool bSocketSame = true;
	for (int32 Repeat = 0; Repeat < 5; ++Repeat)
	{
		const FGSTN_ExitResult Again = (Repeat % 2 == 0 ? SocketA : SocketB)->Place(Query);
		bSocketSame &= GSTNLinkTest::ExactlyEqual(Again.Transform, SocketFirst.Transform) && Again.Velocity == SocketFirst.Velocity
			&& Again.bValid == SocketFirst.bValid && Again.CandidateIndex == SocketFirst.CandidateIndex;
	}
	TestTrue(TEXT("The socket placer gives bit-equal results for equal inputs"), bSocketSame);

	// The placers hold no state a call could change
	const int32 Before = World.QueryCount;
	RingA->Place(Query);
	const int32 Cost = World.QueryCount - Before;
	const int32 Before2 = World.QueryCount;
	RingA->Place(Query);
	TestEqual(TEXT("A call costs the same questions every time"), World.QueryCount - Before2, Cost);

	// A definition without a placer uses the ring; one with a placer uses it
	UGSTN_LinkTypeDefinition* Definition = GSTNLinkTest::MakeGrabDefinition();
	TestTrue(TEXT("The default exit is the ring placer"), Definition->GetExitPlacer() != nullptr && Definition->GetExitPlacer()->IsA<UGSTN_Placer_Ring>());
	Definition->ExitPlacer = NewObject<UGSTN_Placer_AtSocket>(Definition);
	TestTrue(TEXT("A definition's own placer wins"), Definition->GetExitPlacer()->IsA<UGSTN_Placer_AtSocket>());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
