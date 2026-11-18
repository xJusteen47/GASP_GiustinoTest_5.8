// Combat presentation (GAS-NPP fork).

#include "Presentation/GSTN_RunPresentationComponent.h"

#include "GameFramework/Actor.h"
#include "GSTN_AnimSetComponent.h"
#include "GSTN_CombatNPTags.h"
#include "HAL/IConsoleManager.h"
#include "Movement/GSTN_RunStageSyncState.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Kits/GSTN_LocomotionProfile.h"
#include "Movement/GSTN_LayeredMove_MomentumSlide.h"
#include "Movement/GSTN_LayeredMove_RunSkid.h"
#include "MoverComponent.h"
#include "MoverSimulationTypes.h"

namespace GSTNRunPresentation
{
	/** The fixed simulation step (62.5 Hz, owner: untouchable): the presentation only needs it to turn frames into speeds */
	constexpr float StepSeconds = 0.016f;

	static TAutoConsoleVariable<float> CVarLean(TEXT("a.GSTN.LocomotionLean"), 1.f,
		TEXT("Additive locomotion lean: 1 = as the profile says (default), 0 = off, other values scale it (2 = double, to judge it)."));
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
	TickLean(DeltaTime);
}

const FGSTN_LocomotionLeanSettings& UGSTN_RunPresentationComponent::GetLeanSettings() const
{
	const UGSTN_LocomotionAnimSet* Profile = LeanProfile.Get();
	return Profile ? Profile->Lean : DefaultLeanSettings;
}

void UGSTN_RunPresentationComponent::TickLean(float DeltaTime)
{
	// The applied profile can change (skin, kit or weapon collections): looked up each frame, a map find on the anim set component
	const UGSTN_AnimSetComponent* AnimSets = GetOwner() ? GetOwner()->FindComponentByClass<UGSTN_AnimSetComponent>() : nullptr;
	LeanProfile = AnimSets ? Cast<UGSTN_LocomotionAnimSet>(AnimSets->FindSet(GSTNCombatNPTags::AnimSet_Locomotion)) : nullptr;
	const FGSTN_LocomotionLeanSettings& Settings = GetLeanSettings();

	// State scale: run stage, air, attack timeline, skid or attack slide (the spring blends in and out of each)
	const FMoverSyncState& Sync = Mover->GetSyncState();
	const FGSTN_RunStageState* Run = FGSTN_RunStageMover::Find(Sync);
	float StateScale = Settings.GetStageScale(Run ? Run->Stage : 0) * FMath::Max(GSTNRunPresentation::CVarLean.GetValueOnGameThread(), 0.f);
	if (Mover->IsAirborne())
	{
		StateScale *= Settings.AirScale;
	}
	if (FootSlide.IsSliding())
	{
		StateScale *= Settings.SlideScale;
	}
	if (const UGSTN_CombatSimComponent* Sim = GetOwner()->FindComponentByClass<UGSTN_CombatSimComponent>())
	{
		if (Sim->IsInTimeline())
		{
			StateScale *= Settings.AttackScale;
		}
	}
	LeanSolver.Step(Settings, Mover->GetVelocity(), GetOwner()->GetActorForwardVector(), DeltaTime, StateScale);
}

void UGSTN_RunPresentationComponent::GetAdditiveReaction(FGSTN_AdditiveReactionRequest& Out) const
{
	// Layer 1: the locomotion lean, the placeholder bend held at its peak with the weight carrying the angle
	const float LocoLeanDeg = LeanSolver.Lean.Size();
	const float MaxDeg = GetLeanSettings().MaxAngleDeg * 1.2f;
	if (LocoLeanDeg > 0.05f && MaxDeg > 0.f)
	{
		FGSTN_AdditiveReactionLayer& Lean = Out.Layers[1];
		Lean.Additive = nullptr;
		Lean.PushDirection = LeanSolver.Lean / LocoLeanDeg;
		Lean.PlaceholderAngleDeg = MaxDeg;
		Lean.PlaceholderPeakSeconds = 0.1f;
		Lean.PlaceholderLengthSeconds = 1.f;
		Lean.Time = 0.1f;
		Lean.Weight = FMath::Clamp(LocoLeanDeg / MaxDeg, 0.f, 1.f);
	}

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
