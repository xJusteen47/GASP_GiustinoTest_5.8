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
	if (Run && Run->IsSkidInMainGraph())
	{
		// The main ABP's Skid state plays it: this layer stays off (and so does the placeholder lean)
		Run->bPlaceholderSkidLean = false;
		bActive = false;
		ClipTime = 0.f;
		return;
	}
	UAnimSequenceBase* SectionClip = Run ? Run->GetSkidSectionClip() : nullptr;
	if (!bActive)
	{
		ActiveClip = SectionClip ? SectionClip : SkidClip.Get();
	}
	if (Run && ActiveClip)
	{
		// The real clip replaces the placeholder lean (the additive would lean the clip a second time)
		Run->bPlaceholderSkidLean = false;
	}
	const bool bLive = ActiveClip && Slide.Source == EGSTN_FootSlideSource::RunSkid && Slide.SkidPhase != EGSTN_RunSkidPhase::None;
	if (bLive)
	{
		bActive = true;
		TailSeconds = -1.f;
		LastPhase = Slide.SkidPhase;
		if (SectionClip && ActiveClip == SectionClip)
		{
			// Three sections: fixed entry, hold on the speed left, fixed push (brief section 3)
			ClipTime = Run->GetSkidSectionSettings().ClipTime(Slide.SkidPhase, Slide.Frame, Slide.RestFrame, Run->GetSkidEnterFrames(),
				Run->GetSkidExitFrames(), Slide.Speed, Run->GetSkidEntrySpeed(), ActiveClip->GetPlayLength());
		}
		else
		{
			ClipTime = FGSTN_SkidClipTime::Compute(Slide.SkidPhase, Slide.Frame, Slide.RestFrame, ActiveClip->GetPlayLength(), StopTime, StopFraction,
				ExitPlayRate, GSTNRunLayers::SimFrameSeconds);
		}
		return;
	}
	// Tail: the sim's Exit is over but the clip (stop + push off in one motion) is not; play it out on the ground, nothing else sliding
	if (!Mover.IsValid())
	{
		AActor* Owner = GetOwningActor();
		Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	}
	const bool bOnGround = Mover.IsValid() && Mover->HasGameplayTag(Mover_IsOnGround, false);
	if (bActive && ActiveClip && LastPhase == EGSTN_RunSkidPhase::Exit && !Slide.IsSliding() && bOnGround && MaxTailSeconds > 0.f)
	{
		TailSeconds = FMath::Max(TailSeconds, 0.f) + DeltaSeconds;
		ClipTime += DeltaSeconds * FMath::Max(ExitPlayRate, 0.f);
		if (ClipTime < ActiveClip->GetPlayLength() && TailSeconds <= MaxTailSeconds)
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
	OutClip = ActiveClip ? ActiveClip.Get() : SkidClip.Get();
	OutTime = ClipTime;
	return OutClip != nullptr;
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
	// Scramble mode: the presentation component runs the state, this layer only shows the run cycle at its time
	const UGSTN_RunPresentationComponent* Run = Presentation.Get();
	ScrambleClip = Run ? Run->GetScrambleClip() : nullptr;
	bScramble = ScrambleClip != nullptr;
	if (bScramble)
	{
		const FGSTN_RunStartScrambleSettings& Settings = Run->GetScrambleSettings();
		ScrambleBlendIn = Settings.BlendInSeconds;
		ScrambleBlendOut = Settings.BlendOutSeconds;
		const float Length = ScrambleClip->GetPlayLength();
		if (Run->GetScramble().bActive && Length > 0.f)
		{
			ScrambleTime = FMath::Fmod(Run->GetScramble().ClipTime, Length);
		}
		return;
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

bool UGSTN_RunStartAnimInstance::IsOverrideLayerActive() const
{
	if (bScramble)
	{
		const UGSTN_RunPresentationComponent* Run = Presentation.Get();
		return Run && Run->GetScramble().bActive;
	}
	return State.bActive && RunStartClip != nullptr;
}

float UGSTN_RunStartAnimInstance::GetOverrideBlendTime() const
{
	// Scramble: a near cut in (brief: 2 frames), a softer hand back to GASP's run
	return bScramble ? (IsOverrideLayerActive() ? ScrambleBlendIn : ScrambleBlendOut) : OverrideBlendTime;
}

bool UGSTN_RunStartAnimInstance::GetClipAtTime(UAnimSequenceBase*& OutClip, float& OutTime) const
{
	if (bScramble)
	{
		OutClip = ScrambleClip;
		OutTime = ScrambleTime;
		return true;
	}
	OutClip = RunStartClip;
	OutTime = State.Elapsed * PlayRate;
	return RunStartClip != nullptr;
}
