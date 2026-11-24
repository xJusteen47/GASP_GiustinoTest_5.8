#include "Presentation/GSTN_GreyboxAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "MoverComponent.h"
#include "MoverTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_GreyboxAnimInstance)

void FGSTN_GreyboxSpring::Step(float Target, float DeltaSeconds, float FrequencyHz, float Damping)
{
	// Sub-steps keep the spring stable on a long frame (a hitch must not explode the capsule)
	const float Omega = 2.f * PI * FMath::Max(FrequencyHz, 0.01f);
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / (1.f / 120.f)), 1, 16);
	const float Dt = DeltaSeconds / Steps;
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		const float Accel = Omega * Omega * (Target - Value) - 2.f * Damping * Omega * Velocity;
		Velocity += Accel * Dt;
		Value += Velocity * Dt;
	}
}

float UGSTN_GreyboxAnimInstance::SquashTarget(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float VelocityZ, float ForwardAcceleration)
{
	if (!bOnGround)
	{
		return FMath::Min(1.f + Settings.AirStretchPer1000 * FMath::Abs(VelocityZ) / 1000.f, Settings.MaxStretch);
	}
	// Only a speed gain squashes (the body gathers to push); braking reads through the lean
	return FMath::Max(1.f - Settings.AccelSquashPer1000 * FMath::Max(ForwardAcceleration, 0.f) / 1000.f, Settings.MinSquash);
}

FRotator UGSTN_GreyboxAnimInstance::LeanTarget(const FGSTN_GreyboxShapeSettings& Settings, const FVector& LocalAcceleration)
{
	FVector2D Lean(LocalAcceleration.X, LocalAcceleration.Y);
	Lean *= Settings.LeanDegPer1000 / 1000.f;
	if (Lean.Size() > Settings.MaxLeanDeg)
	{
		Lean = Lean.GetSafeNormal() * Settings.MaxLeanDeg;
	}
	// Pitch = minus the forward lean (Unreal pitch up = top back), roll = the lean to the right (+Y)
	return FRotator(-Lean.X, 0.f, Lean.Y);
}

void UGSTN_GreyboxAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	const AActor* Owner = GetOwningActor();
	Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	SquashSpring.Value = 1.f;
	SquashSpring.Velocity = 0.f;
	bHasPrevious = false;
}

void UGSTN_GreyboxAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const AActor* Owner = GetOwningActor();
	if (!Owner || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	if (!Mover.IsValid())
	{
		Mover = Owner->FindComponentByClass<UMoverComponent>();
	}
	const UMoverComponent* MoverComponent = Mover.Get();
	const FVector Velocity = MoverComponent ? MoverComponent->GetVelocity() : Owner->GetVelocity();
	const bool bOnGround = MoverComponent ? MoverComponent->HasGameplayTag(Mover_IsOnGround, true) : true;

	FVector Acceleration = FVector::ZeroVector;
	if (bHasPrevious)
	{
		Acceleration = (Velocity - PreviousVelocity) / DeltaSeconds;
		Acceleration.Z = 0.f;
	}
	PreviousVelocity = Velocity;
	bHasPrevious = true;

	// Touchdown: kick the squash spring by the fall speed, so it squashes and bounces back
	if (bOnGround && !bWasOnGround && LastAirVelocityZ < 0.f)
	{
		const float Omega = 2.f * PI * ShapeSettings.SpringFrequency;
		SquashSpring.Velocity -= ShapeSettings.LandSquashPer1000 * (-LastAirVelocityZ / 1000.f) * Omega;
	}
	if (!bOnGround)
	{
		LastAirVelocityZ = Velocity.Z;
	}
	bWasOnGround = bOnGround;

	const FVector Local = Owner->GetActorTransform().InverseTransformVectorNoScale(Acceleration);
	const FRotator LeanGoal = bOnGround ? LeanTarget(ShapeSettings, Local) : FRotator::ZeroRotator;
	SquashSpring.Step(SquashTarget(ShapeSettings, bOnGround, Velocity.Z, Local.X), DeltaSeconds, ShapeSettings.SpringFrequency, ShapeSettings.SpringDamping);
	PitchSpring.Step(LeanGoal.Pitch, DeltaSeconds, ShapeSettings.SpringFrequency, ShapeSettings.SpringDamping);
	RollSpring.Step(LeanGoal.Roll, DeltaSeconds, ShapeSettings.SpringFrequency, ShapeSettings.SpringDamping);

	Shape.Squash = FMath::Clamp(SquashSpring.Value, ShapeSettings.MinSquash * 0.8f, ShapeSettings.MaxStretch * 1.1f);
	// The lean is in the component's frame; the mesh is rotated -90 yaw from the actor in GASP pawns, so turn the actor-frame lean into it
	const FQuat ActorToComponent = GetSkelMeshComponent() ? (GetSkelMeshComponent()->GetComponentQuat().Inverse() * Owner->GetActorQuat()) : FQuat::Identity;
	// Rotation axis scaled by degrees, actor frame: tipping the top toward +X is a turn around +Y by the forward lean (-Pitch), toward +Y a
	// turn around -X by the side lean (Roll)
	const FVector Axis = ActorToComponent.RotateVector(FVector(-RollSpring.Value, -PitchSpring.Value, 0.f));
	Shape.Lean = FQuat(Axis.GetSafeNormal(), FMath::DegreesToRadians(Axis.Size())).Rotator();
	if (Axis.IsNearlyZero())
	{
		Shape.Lean = FRotator::ZeroRotator;
	}
}
