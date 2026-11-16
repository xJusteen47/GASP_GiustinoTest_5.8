// Automation tests of the minion brain's pure parts (M8 G8): steering math, the native chase and swing, the per-tier brain budget and
// the stale intent. No world: the context is filled by hand.

#include "GSTN_MinionTestUtils.h"
#include "Brain/GSTN_MinionBrain.h"
#include "Brain/GSTN_MinionSteering.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using BrainTier = EGSTN_MinionLODTier;

	/** A context for a minion at the origin and a target Distance cm away along +X */
	FGSTN_MinionBrainContext MakeBrainTestContext(double Distance, BrainTier Tier = BrainTier::Combat)
	{
		FGSTN_MinionBrainContext Context;
		Context.Position = FVector::ZeroVector;
		Context.Tier = Tier;
		Context.bHasTarget = true;
		Context.TargetId = 7;
		Context.TargetPosition = FVector(Distance, 0.0, 0.0);
		Context.MovePoint = Context.TargetPosition;
		Context.TargetDistance = static_cast<float>(Distance);
		Context.DefaultCommand = FGameplayTag(); // an invalid command is fine here: only the plumbing is judged
		return Context;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBrainSteeringMathTest, "GSTNMinion.Core.Brain.SteeringMath", GSTNMinionTest::Flags)
bool FGSTN_MinionBrainSteeringMathTest::RunTest(const FString& Parameters)
{
	// Seek: unit direction, zero when on top
	const FVector2D Seek = UGSTN_MinionSteeringUtils::Seek(FVector2D(0.0, 0.0), FVector2D(300.0, 400.0));
	TestTrue(TEXT("Seek is a unit vector toward the target"), FMath::IsNearlyEqual(Seek.X, 0.6, 1.0e-6) && FMath::IsNearlyEqual(Seek.Y, 0.8, 1.0e-6));
	TestTrue(TEXT("Seek on top of the target is zero"), UGSTN_MinionSteeringUtils::Seek(FVector2D(5.0, 5.0), FVector2D(5.0, 5.0)).IsNearlyZero());

	// Arrive: stop radius 100, slow ring 200, speed 300
	FVector2D Direction;
	float Speed = -1.f;
	UGSTN_MinionSteeringUtils::Arrive(FVector2D::ZeroVector, FVector2D(100.0, 0.0), 300.f, 100.f, 200.f, Direction, Speed);
	TestTrue(TEXT("At the stop radius the minion stands still"), Speed == 0.f && Direction.IsNearlyZero());
	UGSTN_MinionSteeringUtils::Arrive(FVector2D::ZeroVector, FVector2D(40.0, 0.0), 300.f, 100.f, 200.f, Direction, Speed);
	TestTrue(TEXT("Inside the stop radius the minion stands still"), Speed == 0.f && Direction.IsNearlyZero());
	UGSTN_MinionSteeringUtils::Arrive(FVector2D::ZeroVector, FVector2D(200.0, 0.0), 300.f, 100.f, 200.f, Direction, Speed);
	TestTrue(TEXT("Halfway through the slow ring the speed is half"), FMath::IsNearlyEqual(Speed, 150.f, 0.01f) && FMath::IsNearlyEqual(Direction.X, 1.0, 1.0e-6));
	UGSTN_MinionSteeringUtils::Arrive(FVector2D::ZeroVector, FVector2D(900.0, 0.0), 300.f, 100.f, 200.f, Direction, Speed);
	TestEqual(TEXT("Beyond the slow ring the speed is full"), Speed, 300.f);
	UGSTN_MinionSteeringUtils::Arrive(FVector2D::ZeroVector, FVector2D(110.0, 0.0), 300.f, 100.f, 0.f, Direction, Speed);
	TestEqual(TEXT("With no slow ring the speed is full right outside the stop radius"), Speed, 300.f);

	// Separation: pushed away from a close neighbour, ignoring far ones and the coincident one
	const FVector2D Neighbours[] = { FVector2D(50.0, 0.0) };
	const FVector2D Push = UGSTN_MinionSteeringUtils::Separation(FVector2D::ZeroVector, Neighbours, 100.f);
	TestTrue(TEXT("A neighbour at +X pushes toward -X, half strength at half the radius"), FMath::IsNearlyEqual(Push.X, -0.5, 1.0e-6) && FMath::IsNearlyZero(Push.Y, 1.0e-6));
	const FVector2D Far[] = { FVector2D(150.0, 0.0), FVector2D(0.0, 0.0) };
	TestTrue(TEXT("A neighbour outside the radius, and one on the same spot, push nothing"), UGSTN_MinionSteeringUtils::Separation(FVector2D::ZeroVector, Far, 100.f).IsNearlyZero());
	const FVector2D Crowd[] = { FVector2D(10.0, 0.0), FVector2D(-10.0, 0.0), FVector2D(0.0, 10.0), FVector2D(0.0, 20.0), FVector2D(5.0, 5.0) };
	TestTrue(TEXT("The push never exceeds unit length"), UGSTN_MinionSteeringUtils::Separation(FVector2D::ZeroVector, Crowd, 100.f).Size() <= 1.0 + 1.0e-9);

	// Blend: unit result; weight 0 keeps the seek direction
	const FVector2D Blend = UGSTN_MinionSteeringUtils::BlendSeparation(FVector2D(1.0, 0.0), FVector2D(0.0, 1.0), 1.f);
	TestTrue(TEXT("Seek plus an equal perpendicular push turns 45 degrees"), FMath::IsNearlyEqual(Blend.X, FMath::Sqrt(0.5), 1.0e-6) && FMath::IsNearlyEqual(Blend.Y, FMath::Sqrt(0.5), 1.0e-6));
	TestTrue(TEXT("Weight 0 ignores the push"), UGSTN_MinionSteeringUtils::BlendSeparation(FVector2D(1.0, 0.0), FVector2D(0.0, 1.0), 0.f).Equals(FVector2D(1.0, 0.0), 1.0e-6));
	TestTrue(TEXT("A zero seek with a zero push is zero"), UGSTN_MinionSteeringUtils::BlendSeparation(FVector2D::ZeroVector, FVector2D::ZeroVector, 1.f).IsNearlyZero());
	TestTrue(TEXT("Yaw of +Y is 90"), FMath::IsNearlyEqual(UGSTN_MinionSteeringUtils::YawOfDirection(FVector2D(0.0, 1.0)), 90.f, 1.0e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBrainActionsTest, "GSTNMinion.Core.Brain.Actions", GSTNMinionTest::Flags)
bool FGSTN_MinionBrainActionsTest::RunTest(const FString& Parameters)
{
	const FGSTN_MinionChaseSwingParams Params;
	// Far target: moves toward it at the chase speed and faces it
	{
		FGSTN_MinionBrainContext Context = MakeBrainTestContext(1000.0);
		FGSTN_MinionBrainActions::MoveToTarget(Context, Params.Speed, Params.StopRadius, Params.SlowRadius, Params.SeparationRadius, Params.SeparationWeight);
		FGSTN_MinionBrainActions::FaceTarget(Context);
		TestEqual(TEXT("Move speed"), Context.PendingIntent.DesiredSpeed, Params.Speed);
		TestTrue(TEXT("Move direction is +X"), FMath::IsNearlyEqual(Context.PendingIntent.MoveDir.X, 1.0, 1.0e-6));
		TestTrue(TEXT("Facing is requested toward +X"), Context.PendingIntent.bFaceRequested && FMath::IsNearlyZero(Context.PendingIntent.DesiredYaw, 1.0e-3f));
		TestEqual(TEXT("The intent names the target"), Context.PendingIntent.TargetEntityId, 7);
	}
	// Inside the stop radius: no movement, the target is still named
	{
		FGSTN_MinionBrainContext Context = MakeBrainTestContext(Params.StopRadius - 10.0);
		FGSTN_MinionBrainActions::MoveToTarget(Context, Params.Speed, Params.StopRadius, Params.SlowRadius, Params.SeparationRadius, Params.SeparationWeight);
		TestEqual(TEXT("No speed inside the stop radius"), Context.PendingIntent.DesiredSpeed, 0.f);
		TestTrue(TEXT("No move direction inside the stop radius"), Context.PendingIntent.MoveDir.IsNearlyZero());
	}
	// Neighbour on the path bends the direction
	{
		FGSTN_MinionBrainContext Context = MakeBrainTestContext(1000.0);
		const FVector2D Neighbours[] = { FVector2D(60.0, 20.0) };
		Context.Neighbours = Neighbours;
		FGSTN_MinionBrainActions::MoveToTarget(Context, Params.Speed, Params.StopRadius, Params.SlowRadius, Params.SeparationRadius, Params.SeparationWeight);
		TestTrue(TEXT("A neighbour ahead and to the left bends the move to the right"), Context.PendingIntent.MoveDir.Y < 0.0);
		TestTrue(TEXT("The bent direction is still unit length"), FMath::IsNearlyEqual(Context.PendingIntent.MoveDir.Size(), 1.0, 1.0e-6));
	}
	// No target: nothing is written
	{
		FGSTN_MinionBrainContext Context;
		FGSTN_MinionBrainActions::MoveToTarget(Context, 200.f, 150.f, 0.f, 120.f, 0.6f);
		FGSTN_MinionBrainActions::FaceTarget(Context);
		FGSTN_MinionBrainActions::RequestAttack(Context, FGameplayTag());
		TestTrue(TEXT("Without a target the intent stays empty"), Context.PendingIntent.DesiredSpeed == 0.f && !Context.PendingIntent.bFaceRequested && !Context.PendingIntent.HasCommand());
		TestFalse(TEXT("No target is never in range"), FGSTN_MinionBrainActions::IsTargetInRange(Context, 0.f, 1.0e9f));
	}
	// Range and tier conditions
	{
		FGSTN_MinionBrainContext Context = MakeBrainTestContext(250.0, BrainTier::Far);
		TestTrue(TEXT("250 is inside [200, 300]"), FGSTN_MinionBrainActions::IsTargetInRange(Context, 200.f, 300.f));
		TestFalse(TEXT("250 is outside [0, 200]"), FGSTN_MinionBrainActions::IsTargetInRange(Context, 0.f, 200.f));
		TestTrue(TEXT("Far is at least Far"), FGSTN_MinionBrainActions::IsLODAtLeast(Context, BrainTier::Far));
		TestTrue(TEXT("Far is at least Dormant"), FGSTN_MinionBrainActions::IsLODAtLeast(Context, BrainTier::Dormant));
		TestFalse(TEXT("Far is not at least Near"), FGSTN_MinionBrainActions::IsLODAtLeast(Context, BrainTier::Near));
		Context.Tier = BrainTier::Combat;
		TestTrue(TEXT("Combat is at least Near"), FGSTN_MinionBrainActions::IsLODAtLeast(Context, BrainTier::Near));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBrainNativeChaseSwingTest, "GSTNMinion.Core.Brain.NativeChaseSwing", GSTNMinionTest::Flags)
bool FGSTN_MinionBrainNativeChaseSwingTest::RunTest(const FString& Parameters)
{
	const FGSTN_MinionChaseSwingParams Params;
	FGSTN_MinionNativeBrain Brain;
	FGSTN_MinionBrainContext Context = MakeBrainTestContext(1000.0);
	// A command to ask for, so RequestAttack has something to write
	Context.DefaultCommand = FGameplayTag::EmptyTag;

	Brain.Tick(Context);
	TestTrue(TEXT("The first tick enters Chase and moves"), Brain.Phase == FGSTN_MinionNativeBrain::EPhase::Chase && Context.PendingIntent.DesiredSpeed > 0.f);
	Brain.Tick(Context);
	TestTrue(TEXT("Far away it keeps chasing"), Brain.Phase == FGSTN_MinionNativeBrain::EPhase::Chase && Context.PendingIntent.DesiredSpeed > 0.f);

	// Close in: into range of the swing
	Context = MakeBrainTestContext(Params.EnterRange - 5.0);
	Brain.Tick(Context);
	TestTrue(TEXT("In range it turns into Swing"), Brain.Phase == FGSTN_MinionNativeBrain::EPhase::Swing);
	Context = MakeBrainTestContext(Params.EnterRange - 5.0);
	Brain.Tick(Context);
	TestTrue(TEXT("Swing faces and does not walk"), Brain.Phase == FGSTN_MinionNativeBrain::EPhase::Swing && Context.PendingIntent.bFaceRequested && Context.PendingIntent.DesiredSpeed == 0.f);

	// Between the enter and the exit range it keeps swinging (the hysteresis against flicker)
	Context = MakeBrainTestContext((Params.EnterRange + Params.ExitRange) * 0.5);
	Brain.Tick(Context);
	TestTrue(TEXT("Inside the exit range it stays in Swing"), Brain.Phase == FGSTN_MinionNativeBrain::EPhase::Swing);

	// Out of the exit range: back to chasing, and the new phase's actions run at once
	Context = MakeBrainTestContext(Params.ExitRange + 100.0);
	Brain.Tick(Context);
	TestTrue(TEXT("Out of range it goes back to Chase"), Brain.Phase == FGSTN_MinionNativeBrain::EPhase::Chase);
	TestTrue(TEXT("The Chase actions ran on the transition tick"), Context.PendingIntent.DesiredSpeed > 0.f);

	// No target: still alive, writes nothing
	FGSTN_MinionBrainContext Empty;
	Brain.Tick(Empty);
	TestTrue(TEXT("Without a target the brain idles"), Empty.PendingIntent.DesiredSpeed == 0.f && !Empty.PendingIntent.HasCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBrainBudgetTest, "GSTNMinion.Core.Brain.Budget", GSTNMinionTest::Flags)
bool FGSTN_MinionBrainBudgetTest::RunTest(const FString& Parameters)
{
	const FGSTN_MinionBrainBudget Budget;
	TestEqual(TEXT("T0 interval"), Budget.GetInterval(BrainTier::Combat), 1);
	TestEqual(TEXT("T1 interval"), Budget.GetInterval(BrainTier::Near), 4);
	TestEqual(TEXT("T2 interval"), Budget.GetInterval(BrainTier::Far), 16);
	TestEqual(TEXT("T3 never runs"), Budget.GetInterval(BrainTier::Dormant), 0);

	constexpr int32 NumSlots = 64;
	constexpr int32 NumFrames = 64;
	int32 TicksPerTier[4] = { 0, 0, 0, 0 };
	int32 MinPerFrame[4] = { INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX };
	int32 MaxPerFrame[4] = { 0, 0, 0, 0 };
	for (int32 Counter = 0; Counter < NumFrames; ++Counter)
	{
		int32 PerFrame[4] = { 0, 0, 0, 0 };
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			for (int32 Tier = 0; Tier < 4; ++Tier)
			{
				PerFrame[Tier] += Budget.ShouldTick(static_cast<BrainTier>(Tier), Slot, Counter, false) ? 1 : 0;
			}
		}
		for (int32 Tier = 0; Tier < 4; ++Tier)
		{
			TicksPerTier[Tier] += PerFrame[Tier];
			MinPerFrame[Tier] = FMath::Min(MinPerFrame[Tier], PerFrame[Tier]);
			MaxPerFrame[Tier] = FMath::Max(MaxPerFrame[Tier], PerFrame[Tier]);
		}
	}
	// 64 slots of one tier over 64 frames
	TestEqual(TEXT("T0: every slot every frame"), TicksPerTier[0], NumSlots * NumFrames);
	TestEqual(TEXT("T1: one frame in four"), TicksPerTier[1], NumSlots * NumFrames / 4);
	TestEqual(TEXT("T2: one frame in sixteen"), TicksPerTier[2], NumSlots * NumFrames / 16);
	TestEqual(TEXT("T3: never"), TicksPerTier[3], 0);
	// Flat: the slots spread over the frames, no frame carries more than its share
	TestEqual(TEXT("T1 load is flat (16 of 64 slots every frame)"), MinPerFrame[1], MaxPerFrame[1]);
	TestEqual(TEXT("T1 per frame"), MaxPerFrame[1], NumSlots / 4);
	TestEqual(TEXT("T2 load is flat (4 of 64 slots every frame)"), MinPerFrame[2], MaxPerFrame[2]);
	TestEqual(TEXT("T2 per frame"), MaxPerFrame[2], NumSlots / 16);

	// A wake forces a tick whatever the tier, even Dormant
	TestTrue(TEXT("A woken dormant slot ticks"), Budget.ShouldTick(BrainTier::Dormant, 3, 5, true));
	TestFalse(TEXT("A dormant slot does not tick without a wake"), Budget.ShouldTick(BrainTier::Dormant, 3, 5, false));

	// Tunable rates
	FGSTN_MinionBrainBudget Slow;
	Slow.NearInterval = 8;
	TestEqual(TEXT("T1 interval follows the setting"), Slow.GetInterval(BrainTier::Near), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBrainStaleIntentTest, "GSTNMinion.Core.Brain.IntentStaleWhenBrainStops", GSTNMinionTest::Flags)
bool FGSTN_MinionBrainStaleIntentTest::RunTest(const FString& Parameters)
{
	// What the batch sees when a brain stops writing: the stamp ages and the intent is ignored after IntentStaleFrames
	FGSTN_MinionIntent Intent;
	TestTrue(TEXT("An intent never written is stale"), Intent.IsStale(0));
	Intent.StampFrame = 100;
	TestFalse(TEXT("Fresh on the frame it was written"), Intent.IsStale(100));
	TestFalse(TEXT("Still valid at the limit"), Intent.IsStale(100 + GSTNMinion::IntentStaleFrames));
	TestTrue(TEXT("Stale one frame after the limit"), Intent.IsStale(100 + GSTNMinion::IntentStaleFrames + 1));
	// The slowest brain rate (T2, every 16 engine frames) must refresh well inside the limit
	TestTrue(TEXT("The T2 brain rate is inside the stale limit"), FGSTN_MinionBrainBudget().FarInterval < GSTNMinion::IntentStaleFrames);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
