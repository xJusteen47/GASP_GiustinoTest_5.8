// Combat presentation (GAS-NPP fork).

#include "Presentation/GSTN_RunPresentationComponent.h"

#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
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
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"

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
	const bool bWasSkid = FootSlide.Source == EGSTN_FootSlideSource::RunSkid;
	FootSlide = ReadFootSlide(Mover->GetSyncState(), GSTNRunPresentation::StepSeconds);
	// The skid's entry speed (highest over the enter frames): the hold of the three-section clip runs on the share of it left
	if (FootSlide.Source == EGSTN_FootSlideSource::RunSkid)
	{
		if (!bWasSkid)
		{
			SkidEntrySpeed = FootSlide.Speed;
		}
		else if (FootSlide.SkidPhase == EGSTN_RunSkidPhase::Enter)
		{
			SkidEntrySpeed = FMath::Max(SkidEntrySpeed, FootSlide.Speed);
		}
	}
	else
	{
		SkidEntrySpeed = 0.f;
	}

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
	ResolveClips();
	TickSkidPlayTime(DeltaTime);
	ApplyMainAnimClass();
	TickScramble(DeltaTime);
}

namespace GSTNRunPresentation
{
	bool SkidAtClipSpeed()
	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.Skid.PlayAtClipSpeed"));
		return CVar && CVar->GetInt() != 0;
	}
}

void UGSTN_RunPresentationComponent::TickSkidPlayTime(float DeltaTime)
{
	if (!SkidSectionClip || !GSTNRunPresentation::SkidAtClipSpeed())
	{
		SkidPlayTime = -1.f;
		return;
	}
	const float Length = SkidSectionClip->GetPlayLength();
	// With the sim on the clip's root track, the clip plays at the track's rate, the same clock as the capsule (owner choice A part 2)
	const FGSTN_LocomotionSimTuning* Locomotion = FGSTN_LocomotionAttach::Find(GetOwner());
	if (Locomotion && Locomotion->RunStages.Skid.RootTrack.IsUsable())
	{
		DeltaTime *= Locomotion->RunStages.Skid.RootTrack.PlayRate;
	}
	if (FootSlide.Source == EGSTN_FootSlideSource::RunSkid && FootSlide.SkidPhase != EGSTN_RunSkidPhase::None)
	{
		SkidPlayTime = SkidPlayTime < 0.f ? 0.f : FMath::Min(SkidPlayTime + DeltaTime, Length);
		return;
	}
	// Tail: the stop and the push off are one clip, it plays out after the sim's skid
	if (SkidPlayTime >= 0.f && !FootSlide.IsSliding() && Mover->HasGameplayTag(Mover_IsOnGround, false) && SkidPlayTime + DeltaTime < Length)
	{
		SkidPlayTime += DeltaTime;
		return;
	}
	SkidPlayTime = -1.f;
}

float UGSTN_RunPresentationComponent::GetSkidClipTime() const
{
	if (SkidSectionClip && SkidPlayTime >= 0.f)
	{
		return SkidPlayTime;
	}
	if (!SkidSectionClip || FootSlide.Source != EGSTN_FootSlideSource::RunSkid)
	{
		return 0.f;
	}
	return GetSkidSectionSettings().ClipTime(FootSlide.SkidPhase, FootSlide.Frame, FootSlide.RestFrame, EnterFrames, ExitFrames, FootSlide.Speed,
		SkidEntrySpeed, SkidSectionClip->GetPlayLength());
}

namespace
{
	const UGSTN_RunPresentationComponent* FindRunPresentation(const UAnimInstance* AnimInstance)
	{
		const AActor* Owner = AnimInstance ? AnimInstance->GetOwningActor() : nullptr;
		return Owner ? Owner->FindComponentByClass<UGSTN_RunPresentationComponent>() : nullptr;
	}
}

bool UGSTN_RunPresentationComponent::IsAnimOwnerSkidding(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run && Run->IsSkidding();
}

float UGSTN_RunPresentationComponent::GetAnimOwnerSkidClipTime(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run ? Run->GetSkidClipTime() : 0.f;
}

bool UGSTN_RunPresentationComponent::IsSkidInMainGraph() const
{
	const UGSTN_LocomotionAnimSet* Profile = LeanProfile.Get();
	return Profile && Profile->bSkidInMainGraph && !Profile->MainAnimClass.IsNull();
}

void UGSTN_RunPresentationComponent::ApplyMainAnimClass()
{
	const UGSTN_LocomotionAnimSet* Profile = LeanProfile.Get();
	const FSoftObjectPath Path = Profile ? Profile->MainAnimClass.ToSoftObjectPath() : FSoftObjectPath();
	if (Path == ResolvedMainAnimClass)
	{
		return;
	}
	ResolvedMainAnimClass = Path;
	UClass* AnimClass = Path.IsValid() ? Profile->MainAnimClass.LoadSynchronous() : nullptr;
	if (!AnimClass)
	{
		return;
	}
	// The character's animated mesh: the first one running an anim instance
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetAnimInstance())
		{
			if (Mesh->GetAnimInstance()->GetClass() != AnimClass)
			{
				Mesh->SetAnimInstanceClass(AnimClass);
			}
			return;
		}
	}
}

const FGSTN_RunStartScrambleSettings& UGSTN_RunPresentationComponent::GetScrambleSettings() const
{
	const UGSTN_LocomotionAnimSet* Profile = LeanProfile.Get();
	return Profile ? Profile->RunStart : DefaultScrambleSettings;
}

const FGSTN_SkidSectionSettings& UGSTN_RunPresentationComponent::GetSkidSectionSettings() const
{
	const UGSTN_LocomotionAnimSet* Profile = LeanProfile.Get();
	return Profile ? Profile->Skid : DefaultSkidSettings;
}

void UGSTN_RunPresentationComponent::ResolveClips()
{
	// Soft references in the profile: loaded once on the game thread, again only when the profile points somewhere else
	const FGSTN_RunStartScrambleSettings& Run = GetScrambleSettings();
	const FSoftObjectPath RunPath = Run.bEnabled ? Run.RunCycle.ToSoftObjectPath() : FSoftObjectPath();
	if (RunPath != ResolvedScramblePath)
	{
		ResolvedScramblePath = RunPath;
		ScrambleClip = RunPath.IsValid() ? Run.RunCycle.LoadSynchronous() : nullptr;
	}
	const FGSTN_SkidSectionSettings& Skid = GetSkidSectionSettings();
	const FSoftObjectPath SkidPath = Skid.bEnabled ? Skid.Clip.ToSoftObjectPath() : FSoftObjectPath();
	if (SkidPath != ResolvedSkidPath)
	{
		ResolvedSkidPath = SkidPath;
		SkidSectionClip = SkidPath.IsValid() ? Skid.Clip.LoadSynchronous() : nullptr;
	}
}

void UGSTN_RunPresentationComponent::TickScramble(float DeltaTime)
{
	const FGSTN_LocomotionSimTuning* Locomotion = FGSTN_LocomotionAttach::Find(GetOwner());
	const float TargetSpeed = Locomotion && Locomotion->RunStages.Stages.Num() > 0 ? Locomotion->RunStages.Stages[0].MaxSpeed : 0.f;
	const FVector Up = Mover->GetUpDirection();
	const FVector Velocity = Mover->GetVelocity();
	const FVector Ground = Velocity - Up * Velocity.Dot(Up);
	// The stick (a sim proxy without inputs counts as a full push: the speed alone then drives the scramble)
	float Stick = 1.f;
	if (const FCharacterDefaultInputs* Inputs = Mover->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>())
	{
		Stick = FMath::Clamp(static_cast<float>(Inputs->GetMoveInput().Size()), 0.f, 1.f);
	}
	if (!Ground.IsNearlyZero(1.f))
	{
		ScrambleDirection = Ground.GetSafeNormal();
	}
	// Crouched counts as off the ground: the scramble's end speed is a share of the STANDING run, which a crouch walk never reaches, so it would
	// otherwise overlay the standing run cycle for the whole crouch walk (owner playtest 2026-11-18)
	const bool bScrambleGround = Mover->HasGameplayTag(Mover_IsOnGround, true) && !Mover->HasGameplayTag(Mover_IsCrouching, true);
	Scramble.Step(GetScrambleSettings(), DeltaTime, static_cast<float>(Ground.Size()), TargetSpeed, Stick, bScrambleGround,
		FootSlide.Source == EGSTN_FootSlideSource::RunSkid, ScrambleClip != nullptr);
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
	if (Mover->HasGameplayTag(Mover_IsInAir, true))
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

	// Layer 0 out of a skid: the run start scramble's extra lean along the run (negative = the rebound, leaning back)
	const float ScrambleAngle = GetScrambleSettings().ExtraLeanDeg * 1.2f;
	if (FootSlide.Source != EGSTN_FootSlideSource::RunSkid && FMath::Abs(Scramble.LeanDeg) > 0.05f && ScrambleAngle > 0.f)
	{
		FGSTN_AdditiveReactionLayer& Layer = Out.Layers[0];
		Layer.Additive = nullptr;
		Layer.PushDirection = Scramble.LeanDeg > 0.f ? ScrambleDirection : -ScrambleDirection;
		Layer.PlaceholderAngleDeg = ScrambleAngle;
		Layer.PlaceholderPeakSeconds = 0.1f;
		Layer.PlaceholderLengthSeconds = 1.f;
		Layer.Time = 0.1f;
		Layer.Weight = FMath::Clamp(FMath::Abs(Scramble.LeanDeg) / ScrambleAngle, 0.f, 1.f);
		return;
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
