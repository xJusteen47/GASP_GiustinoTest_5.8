#include "Presentation/GSTN_SimpleLocoAnimInstance.h"

#include "MoverComponent.h"
#include "MoverTypes.h"
#include "Presentation/GSTN_RunPresentationComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_SimpleLocoAnimInstance)

EGSTN_SimpleLocoState FGSTN_SimpleLocoLogic::NextState(const FGSTN_SimpleLocoInputs& In, EGSTN_SimpleLocoState Previous, float TimeInPrevious,
	const FGSTN_SimpleLocoSettings& Settings)
{
	using EState = EGSTN_SimpleLocoState;
	if (!In.bOnGround)
	{
		return EState::Air;
	}
	if (In.bCrouching)
	{
		return EState::Crouch;
	}
	if (In.bSkidding)
	{
		return EState::Skid;
	}
	const bool bMoving = In.Speed2D > Settings.MoveSpeed;
	if (!bMoving)
	{
		return EState::Idle;
	}
	// Stopping: the run presentation's own stop, or the stick let go while still sliding to rest (keeps going until still)
	const bool bWasRunning = Previous == EState::Run || Previous == EState::RunStart || Previous == EState::RunStop;
	if (bWasRunning && (In.bRunStopping || In.StickAlong <= Settings.StopStickAlong))
	{
		return EState::RunStop;
	}
	// A start from rest (or from a landing / crouch / stop that reached rest) plays RunStart for its window, then Run
	if (Previous == EState::RunStart)
	{
		return TimeInPrevious < Settings.RunStartSeconds ? EState::RunStart : EState::Run;
	}
	if (Previous == EState::Idle || Previous == EState::Crouch)
	{
		return EState::RunStart;
	}
	return EState::Run;
}

void UGSTN_SimpleLocoAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	const AActor* Owner = GetOwningActor();
	Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	RunPresentation = Owner ? Owner->FindComponentByClass<UGSTN_RunPresentationComponent>() : nullptr;
	bHasPreviousVelocity = false;
}

void UGSTN_SimpleLocoAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const AActor* Owner = GetOwningActor();
	if (!Owner)
	{
		return;
	}
	if (!Mover.IsValid())
	{
		Mover = Owner->FindComponentByClass<UMoverComponent>();
	}
	if (!RunPresentation.IsValid())
	{
		// Created by the combat sim component at BeginPlay, possibly after this instance initialized
		RunPresentation = Owner->FindComponentByClass<UGSTN_RunPresentationComponent>();
	}

	const UMoverComponent* MoverComponent = Mover.Get();
	const FVector Velocity = MoverComponent ? MoverComponent->GetVelocity() : Owner->GetVelocity();
	const FVector Horizontal(Velocity.X, Velocity.Y, 0.f);

	FGSTN_SimpleLocoInputs In;
	In.Speed2D = Horizontal.Size();
	In.bOnGround = MoverComponent ? MoverComponent->HasGameplayTag(Mover_IsOnGround, true) : true;
	In.bCrouching = MoverComponent && MoverComponent->HasGameplayTag(Mover_IsCrouching, true);
	In.StickAlong = UGSTN_RunPresentationComponent::GetAnimOwnerStickAlong(this);
	if (const UGSTN_RunPresentationComponent* Run = RunPresentation.Get())
	{
		In.bSkidding = Run->IsSkidding();
		In.bRunStopping = Run->IsRunStopping();
		RunStage = Run->GetRunStage();
	}

	const EGSTN_SimpleLocoState Next = FGSTN_SimpleLocoLogic::NextState(In, LocoState, TimeInState, Settings);
	if (Next != LocoState)
	{
		PreviousLocoState = LocoState;
		LocoState = Next;
		TimeInState = 0.f;
		// Warping is on only where the new state's chosen row asks for it (the chooser player writes the targets on state entry)
		OrientationWarpTarget = 0.f;
		StrideWarpTarget = 0.f;
	}
	else
	{
		TimeInState += DeltaSeconds;
	}

	Speed2D = In.Speed2D;
	VelocityZ = Velocity.Z;
	if (bHasPreviousVelocity && DeltaSeconds > KINDA_SMALL_NUMBER && In.Speed2D > KINDA_SMALL_NUMBER)
	{
		const FVector PreviousHorizontal(PreviousVelocity.X, PreviousVelocity.Y, 0.f);
		const float Along = FVector::DotProduct((Horizontal - PreviousHorizontal) / DeltaSeconds, Horizontal / In.Speed2D);
		Acceleration2D = FMath::FInterpTo(Acceleration2D, Along, DeltaSeconds, 12.f);
	}
	else
	{
		Acceleration2D = FMath::FInterpTo(Acceleration2D, 0.f, DeltaSeconds, 12.f);
	}
	PreviousVelocity = Velocity;
	bHasPreviousVelocity = true;

	const FVector LocalVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(Horizontal);
	MoveAngle = In.Speed2D > Settings.MoveSpeed ? FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X)) : 0.f;

	LocomotionAngle = MoveAngle;
	LocomotionSpeed = In.Speed2D;
	OrientationWarpAlpha = FMath::FInterpConstantTo(OrientationWarpAlpha, FMath::Clamp(OrientationWarpTarget, 0.f, 1.f), DeltaSeconds,
		Settings.WarpBlendSpeed);
	StrideWarpAlpha = FMath::FInterpConstantTo(StrideWarpAlpha, FMath::Clamp(StrideWarpTarget, 0.f, 1.f), DeltaSeconds, Settings.WarpBlendSpeed);
}
