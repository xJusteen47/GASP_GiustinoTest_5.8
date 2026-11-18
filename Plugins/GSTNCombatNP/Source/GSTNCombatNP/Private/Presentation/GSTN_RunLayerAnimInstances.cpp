#include "Presentation/GSTN_RunLayerAnimInstances.h"

#include "Animation/AnimSequenceBase.h"
#include "MoverComponent.h"
#include "MoverTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_RunLayerAnimInstances)

namespace GSTNRunLayers
{
	/** One simulation frame (62.5 Hz): the skid counts live frames of the sim */
	constexpr float SimFrameSeconds = 0.016f;
}

float FGSTN_SkidClipTime::Compute(EGSTN_RunSkidPhase Phase, int32 Frame, int32 RestFrame, float Length, float StopTime, float StopFraction, float ExitPlayRate,
	float FrameSeconds)
{
	if (Length <= 0.f || Phase == EGSTN_RunSkidPhase::None)
	{
		return 0.f;
	}
	const float Stop = FMath::Clamp(StopTime >= 0.f ? StopTime : Length * FMath::Clamp(StopFraction, 0.f, 1.f), 0.f, Length);
	if (Phase == EGSTN_RunSkidPhase::Exit)
	{
		const int32 AfterRest = FMath::Max(Frame - RestFrame, 0);
		return FMath::Clamp(Stop + static_cast<float>(AfterRest) * FrameSeconds * FMath::Max(ExitPlayRate, 0.f), 0.f, Length);
	}
	// Enter and Active: the clip reaches its stop on the rest frame (a skid with no rest frame yet sits on the stop)
	const float Progress = RestFrame > 0 ? FMath::Clamp(static_cast<float>(Frame) / static_cast<float>(RestFrame), 0.f, 1.f) : 1.f;
	return Stop * Progress;
}

void FGSTN_RunStartClipState::Step(float DeltaSeconds, float Speed, bool bOnGround, bool bSkidding, float StartFromSpeed, float ClipLength, float PlayRate)
{
	const float Duration = ClipLength > 0.f ? ClipLength / FMath::Max(PlayRate, 0.01f) : 0.f;
	if (bActive)
	{
		Elapsed += FMath::Max(DeltaSeconds, 0.f);
		if (Elapsed >= Duration || !bOnGround || bSkidding || Speed < StartFromSpeed)
		{
			bActive = false;
			Elapsed = 0.f;
		}
	}
	else if (Duration > 0.f && bOnGround && !bSkidding && PreviousSpeed < StartFromSpeed && Speed >= StartFromSpeed)
	{
		bActive = true;
		Elapsed = 0.f;
	}
	PreviousSpeed = Speed;
}

void UGSTN_SkidAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (!Presentation.IsValid())
	{
		AActor* Owner = GetOwningActor();
		Presentation = Owner ? Owner->FindComponentByClass<UGSTN_RunPresentationComponent>() : nullptr;
	}
	UGSTN_RunPresentationComponent* Run = Presentation.Get();
	Slide = Run ? Run->GetFootSlide() : FGSTN_FootSlideState();
	if (Run && SkidClip)
	{
		// The real clip replaces the placeholder lean (the additive would lean the clip a second time)
		Run->bPlaceholderSkidLean = false;
	}
	const bool bLive = SkidClip && Slide.Source == EGSTN_FootSlideSource::RunSkid && Slide.SkidPhase != EGSTN_RunSkidPhase::None;
	if (bLive)
	{
		bActive = true;
		TailSeconds = -1.f;
		LastPhase = Slide.SkidPhase;
		ClipTime = FGSTN_SkidClipTime::Compute(Slide.SkidPhase, Slide.Frame, Slide.RestFrame, SkidClip->GetPlayLength(), StopTime, StopFraction,
			ExitPlayRate, GSTNRunLayers::SimFrameSeconds);
		return;
	}
	// Tail: the sim's Exit is over but the clip (stop + push off in one motion) is not; play it out on the ground, nothing else sliding
	if (!Mover.IsValid())
	{
		AActor* Owner = GetOwningActor();
		Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	}
	const bool bOnGround = Mover.IsValid() && Mover->HasGameplayTag(Mover_IsOnGround, false);
	if (bActive && SkidClip && LastPhase == EGSTN_RunSkidPhase::Exit && !Slide.IsSliding() && bOnGround && MaxTailSeconds > 0.f)
	{
		TailSeconds = FMath::Max(TailSeconds, 0.f) + DeltaSeconds;
		ClipTime += DeltaSeconds * FMath::Max(ExitPlayRate, 0.f);
		if (ClipTime < SkidClip->GetPlayLength() && TailSeconds <= MaxTailSeconds)
		{
			return;
		}
	}
	bActive = false;
	TailSeconds = -1.f;
	LastPhase = EGSTN_RunSkidPhase::None;
	ClipTime = 0.f;
}

bool UGSTN_SkidAnimInstance::GetClipAtTime(UAnimSequenceBase*& OutClip, float& OutTime) const
{
	OutClip = SkidClip;
	OutTime = ClipTime;
	return SkidClip != nullptr;
}

void UGSTN_RunStartAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const AActor* Owner = GetOwningActor();
	if (!Mover.IsValid() && Owner)
	{
		Mover = Owner->FindComponentByClass<UMoverComponent>();
	}
	if (!Presentation.IsValid() && Owner)
	{
		Presentation = Owner->FindComponentByClass<UGSTN_RunPresentationComponent>();
	}
	const UMoverComponent* Movement = Mover.Get();
	if (!Movement)
	{
		return;
	}
	const FVector Up = Movement->GetUpDirection();
	const FVector Velocity = Movement->GetVelocity();
	const float GroundSpeed = static_cast<float>((Velocity - Up * Velocity.Dot(Up)).Size());
	const bool bOnGround = Movement->HasGameplayTag(Mover_IsOnGround, false);
	const bool bSkidding = Presentation.IsValid() && Presentation->GetFootSlide().Source == EGSTN_FootSlideSource::RunSkid;
	State.Step(DeltaSeconds, GroundSpeed, bOnGround, bSkidding, StartFromSpeed, RunStartClip ? RunStartClip->GetPlayLength() : 0.f, PlayRate);
}

bool UGSTN_RunStartAnimInstance::GetClipAtTime(UAnimSequenceBase*& OutClip, float& OutTime) const
{
	OutClip = RunStartClip;
	OutTime = State.Elapsed * PlayRate;
	return RunStartClip != nullptr;
}
