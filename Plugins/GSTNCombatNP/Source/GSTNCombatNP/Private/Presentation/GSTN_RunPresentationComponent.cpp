// Combat presentation (GAS-NPP fork).

#include "Presentation/GSTN_RunPresentationComponent.h"

#include "GameFramework/Actor.h"
#include "Kits/GSTN_LocomotionProfile.h"
#include "Movement/GSTN_LayeredMove_MomentumSlide.h"
#include "Movement/GSTN_LayeredMove_RunSkid.h"
#include "MoverComponent.h"
#include "MoverSimulationTypes.h"

namespace GSTNRunPresentation
{
	/** The fixed simulation step (62.5 Hz, owner: untouchable): the presentation only needs it to turn frames into speeds */
	constexpr float StepSeconds = 0.016f;
}

UGSTN_RunPresentationComponent::UGSTN_RunPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);
}

UGSTN_RunPresentationComponent* UGSTN_RunPresentationComponent::FindOrAdd(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	if (UGSTN_RunPresentationComponent* Existing = Owner->FindComponentByClass<UGSTN_RunPresentationComponent>())
	{
		return Existing;
	}
	UGSTN_RunPresentationComponent* Added = NewObject<UGSTN_RunPresentationComponent>(Owner, TEXT("GSTN_RunPresentation"));
	Added->RegisterComponent();
	return Added;
}

FGSTN_FootSlideState UGSTN_RunPresentationComponent::ReadFootSlide(const FMoverSyncState& SyncState, float StepSeconds)
{
	FGSTN_FootSlideState State;
	if (const FGSTN_LayeredMove_RunSkid* Skid = FGSTN_LayeredMove_RunSkid::FindActive(SyncState))
	{
		const EGSTN_RunSkidPhase Phase = Skid->GetPhase();
		if (Phase != EGSTN_RunSkidPhase::None)
		{
			State.Source = EGSTN_FootSlideSource::RunSkid;
			State.SkidPhase = Phase;
			State.Speed = Skid->GetSlideSpeed(StepSeconds);
			State.Direction = Skid->Direction;
			State.Frame = FMath::Max(Skid->CurrentLiveFrame == INDEX_NONE ? Skid->LiveFrames - 1 : Skid->CurrentLiveFrame, 0);
			State.RestFrame = Skid->RestFrame;
			State.DistanceToStop = FGSTN_RunSkidMath::StopDistance(State.Speed, Skid->Brake);
			return State;
		}
	}
	for (const TSharedPtr<FLayeredMoveBase>& Move : SyncState.LayeredMoves.GetActiveMoves())
	{
		if (Move.IsValid() && Move->GetScriptStruct() == FGSTN_LayeredMove_MomentumSlide::StaticStruct())
		{
			const FGSTN_LayeredMove_MomentumSlide& Slide = static_cast<const FGSTN_LayeredMove_MomentumSlide&>(*Move);
			if (Slide.Phase == static_cast<uint8>(EGSTN_MomentumSlidePhase::Sliding))
			{
				const int32 Frame = FMath::Max(Slide.CurrentLiveFrame == INDEX_NONE ? Slide.LiveFrames - 1 : Slide.CurrentLiveFrame, 0);
				State.Source = EGSTN_FootSlideSource::AttackSlide;
				State.Speed = Slide.CurrentSpeed;
				State.Direction = Slide.Direction;
				State.Frame = Frame;
				return State;
			}
		}
	}
	return State;
}

void UGSTN_RunPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Mover)
	{
		Mover = GetOwner() ? GetOwner()->FindComponentByClass<UMoverComponent>() : nullptr;
		if (!Mover)
		{
			return;
		}
	}

	const bool bWas = FootSlide.IsSliding();
	FootSlide = ReadFootSlide(Mover->GetSyncState(), GSTNRunPresentation::StepSeconds);

	// The kit's placeholder lean and phase lengths (static kit data)
	const FGSTN_LocomotionSimTuning* Locomotion = FGSTN_LocomotionAttach::Find(GetOwner());
	LeanDeg = Locomotion && bPlaceholderSkidLean ? Locomotion->RunStages.Skid.PlaceholderLeanDeg : 0.f;
	EnterFrames = Locomotion ? Locomotion->RunStages.Skid.EnterFrames : 0;
	ExitFrames = Locomotion ? Locomotion->RunStages.Skid.ExitFrames : 0;

	if (bWas != FootSlide.IsSliding())
	{
		OnFootSlideChanged.Broadcast(FootSlide.IsSliding(), FootSlide);
	}
}

void UGSTN_RunPresentationComponent::GetAdditiveReaction(FGSTN_AdditiveReactionRequest& Out) const
{
	if (FootSlide.Source != EGSTN_FootSlideSource::RunSkid || LeanDeg <= 0.f)
	{
		return;
	}
	// PLACEHOLDER: the torso leans back against the slide, in over the enter frames, held, out over the exit
	float Weight = 1.f;
	switch (FootSlide.SkidPhase)
	{
	case EGSTN_RunSkidPhase::Enter:
		Weight = static_cast<float>(FootSlide.Frame + 1) / static_cast<float>(FMath::Max(EnterFrames, 1));
		break;
	case EGSTN_RunSkidPhase::Exit:
		Weight = 1.f - static_cast<float>(FootSlide.Frame - FootSlide.RestFrame + 1) / static_cast<float>(FMath::Max(ExitFrames, 1));
		break;
	default:
		break;
	}
	FGSTN_AdditiveReactionLayer& Layer = Out.Layers[0];
	Layer.Additive = nullptr;
	Layer.PushDirection = -FootSlide.Direction;
	Layer.PlaceholderAngleDeg = LeanDeg;
	// The placeholder curve held at its peak: the weight carries the envelope
	Layer.PlaceholderPeakSeconds = 0.1f;
	Layer.PlaceholderLengthSeconds = 1.f;
	Layer.Time = 0.1f;
	Layer.Weight = FMath::Clamp(Weight, 0.f, 1.f);
}
