// Combat presentation (GAS-NPP fork).

#include "Presentation/GSTN_RunPresentationComponent.h"

#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimStateMachineTypes.h"
#include "Animation/AnimNode_StateMachine.h"
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

	static int32 LogRunStart = 0;
	static FAutoConsoleVariableRef CVarLogRunStart(TEXT("gstn.RunStart.Log"), LogRunStart,
		TEXT("1 = log every presentation tick while a run start is possible: stamp time, stamp flag, speed and the current state of every "
			"state machine of the main anim instance (diagnosis of which state shows the start)"));

	/** "Machine:State" for every baked state machine of the anim instance (diagnostics only) */
	static FString DescribeMachines(const UAnimInstance* Anim)
	{
		FString Out;
		const IAnimClassInterface* AnimClass = Anim ? IAnimClassInterface::GetFromClass(Anim->GetClass()) : nullptr;
		if (!AnimClass)
		{
			return Out;
		}
		const TArray<FBakedAnimationStateMachine>& Machines = AnimClass->GetBakedStateMachines();
		for (int32 Index = 0; Index < Machines.Num(); ++Index)
		{
			// Read through the class's baked description: the instance's own description pointer is null until the machine initializes
			const FAnimNode_StateMachine* Machine = Anim->GetStateMachineInstance(Index);
			const int32 State = Machine ? Machine->GetCurrentState() : INDEX_NONE;
			const FName StateName = Machines[Index].States.IsValidIndex(State) ? Machines[Index].States[State].StateName : NAME_None;
			Out += FString::Printf(TEXT(" [%s:%s #%d/%d %.2f]"), *Machines[Index].MachineName.ToString(), *StateName.ToString(), State,
				Machine ? 1 : 0, Machine ? Machine->GetCurrentStateElapsedTime() : -1.f);
		}
		return Out;
	}
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
	{
		// Frantic run start: frames of the stamp counter turned into seconds (frame 0 of the stamp = one step played)
		const FGSTN_RunStageState* RunState = FGSTN_RunStageMover::Find(Mover->GetSyncState());
		const FGSTN_LocomotionSimTuning* RunTuning = FGSTN_LocomotionAttach::Find(GetOwner());
		RunStartTime = RunState && RunState->StampFrames >= 0 ? (RunState->StampFrames + 1) * GSTNRunPresentation::StepSeconds : -1.f;
		bRunStartStamp = RunState && RunTuning && FGSTN_RunStageLogic::IsStartStamp(RunTuning->RunStages, *RunState);
		BoostDashTime = RunState && RunState->DashFrames >= 0 ? (RunState->DashFrames + 1) * GSTNRunPresentation::StepSeconds : -1.f;
		if (GSTNRunPresentation::LogRunStart && RunState && (RunState->StampFrames >= 0 || RunState->Stage == 0))
		{
			const USkeletalMeshComponent* Mesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
			const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
			UE_LOG(LogTemp, Log, TEXT("RunStartLog %s %s f=%llu stamp=%d t=%.3f flag=%d stage=%d speed=%.0f%s"), *GetNameSafe(GetOwner()),
				*GetNameSafe(Anim ? Anim->GetClass() : nullptr), GFrameCounter, RunState->StampFrames,
				RunStartTime, bRunStartStamp ? 1 : 0, RunState->Stage, Mover->GetVelocity().Size2D(), *GSTNRunPresentation::DescribeMachines(Anim));
		}
	}
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
	TickRunStop(DeltaTime);
	TickFastCrouch(DeltaTime);
	// Time since this skid began (the Skid state's re-entry guard, locomotion-finish-2): -1 when not skidding
	SkidElapsed = IsSkidding() ? (SkidElapsed < 0.f ? 0.f : SkidElapsed + DeltaTime) : -1.f;
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

float UGSTN_RunPresentationComponent::GetAnimOwnerSkidElapsed(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run ? Run->SkidElapsed : -1.f;
}

float UGSTN_RunPresentationComponent::GetAnimOwnerRunStartTime(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run ? Run->GetRunStartTime() : -1.f;
}

float UGSTN_RunPresentationComponent::GetAnimOwnerBoostDashTime(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run ? Run->GetBoostDashTime() : -1.f;
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
	const float Stick = ReadStick();
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
	// The run stop clip carries its own weight (torso still leaning FORWARD over the planted foot, brief section 2 trait 2). The braking
	// would make the solver lean the body BACK by up to MaxAngleDeg and cancel it (run-stop-2 capture), so the lean springs out instead
	if (RunStop.IsClip())
	{
		StateScale = 0.f;
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

namespace GSTNRunPresentation
{
	static TAutoConsoleVariable<int32> CVarRunStopLog(TEXT("gstn.RunStop.Log"), 0, TEXT("1 = log the run stop every frame it is armed or playing (LogTemp)"));
	static TAutoConsoleVariable<int32> CVarRunStopMode(TEXT("gstn.RunStop.Mode"), -1,
		TEXT("Run stop (stick let go during a run): -1 = the locomotion profile's mode, 0 = off (GASP's stop), 1 = stop clip, 2 = blend into idle"));

	bool RunStopGroundOnly(const UMoverComponent* Mover)
	{
		return Mover->HasGameplayTag(Mover_IsOnGround, true) && !Mover->HasGameplayTag(Mover_IsCrouching, true);
	}

	/** Root positions of a clip at 60 Hz (the root track, before any root motion setting) */
	void BuildRunStopTrack(const UAnimSequenceBase* Clip, FGSTN_RunStopTrack& Out)
	{
		Out = FGSTN_RunStopTrack();
		if (!Clip)
		{
			return;
		}
		constexpr float Rate = 60.f;
		const float Length = Clip->GetPlayLength();
		TArray<FVector> Positions;
		for (int32 i = 0; i <= FMath::FloorToInt(Length * Rate); ++i)
		{
			const FAnimExtractContext Context(static_cast<double>(i) / Rate);
			Positions.Add(Clip->ExtractRootTrackTransform(Context, nullptr).GetTranslation());
		}
		Out.Build(Positions, Rate);
	}
}

float UGSTN_RunPresentationComponent::ReadStick() const
{
	float Stick = 1.f;
	if (const FCharacterDefaultInputs* Inputs = Mover ? Mover->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>() : nullptr)
	{
		Stick = FMath::Clamp(static_cast<float>(Inputs->GetMoveInput().Size()), 0.f, 1.f);
	}
	return Stick;
}

const FGSTN_RunStopSettings& UGSTN_RunPresentationComponent::GetRunStopSettings() const
{
	const UGSTN_LocomotionAnimSet* Profile = LeanProfile.Get();
	return Profile ? Profile->RunStop : DefaultRunStopSettings;
}

bool UGSTN_RunPresentationComponent::IsLeftFootSwinging() const
{
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents(Meshes);
	for (const USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetAnimInstance())
		{
			const int32 Left = Mesh->GetBoneIndex(TEXT("foot_l"));
			const int32 Right = Mesh->GetBoneIndex(TEXT("foot_r"));
			if (Left == INDEX_NONE || Right == INDEX_NONE)
			{
				break;
			}
			// Component space: the higher foot is the one swinging forward
			return Mesh->GetBoneTransform(Left, FTransform::Identity).GetLocation().Z >= Mesh->GetBoneTransform(Right, FTransform::Identity).GetLocation().Z;
		}
	}
	// No mesh: alternate
	return !RunStop.bLeftFoot;
}

void UGSTN_RunPresentationComponent::TickRunStop(float DeltaTime)
{
	const FGSTN_RunStopSettings& Settings = GetRunStopSettings();
	// Clips: soft references, loaded once on the game thread, their tracks measured once
	const FSoftObjectPath Paths[3] = { Settings.ClipLeftFoot.ToSoftObjectPath(), Settings.ClipRightFoot.ToSoftObjectPath(), Settings.IdleClip.ToSoftObjectPath() };
	for (int32 i = 0; i < 3; ++i)
	{
		if (Paths[i] == ResolvedRunStopPaths[i])
		{
			continue;
		}
		ResolvedRunStopPaths[i] = Paths[i];
		if (i < 2)
		{
			const TSoftObjectPtr<UAnimSequenceBase>& Soft = i == 0 ? Settings.ClipLeftFoot : Settings.ClipRightFoot;
			RunStopClips[i] = Paths[i].IsValid() ? Soft.LoadSynchronous() : nullptr;
			GSTNRunPresentation::BuildRunStopTrack(RunStopClips[i], RunStopTracks[i]);
		}
		else
		{
			RunStopIdleClip = Paths[i].IsValid() ? Settings.IdleClip.LoadSynchronous() : nullptr;
		}
	}

	const int32 Forced = GSTNRunPresentation::CVarRunStopMode.GetValueOnGameThread();
	const EGSTN_RunStopMode Mode = Forced >= 0 ? static_cast<EGSTN_RunStopMode>(FMath::Clamp(Forced, 0, 2)) : Settings.Mode;
	const FVector Up = Mover->GetUpDirection();
	const FVector Velocity = Mover->GetVelocity();
	const float Speed = static_cast<float>((Velocity - Up * Velocity.Dot(Up)).Size());
	// Only a plain grounded run stops this way: the skid (reverse input) and every slide keep their own presentation
	const bool bBlocked = !GSTNRunPresentation::RunStopGroundOnly(Mover) || FootSlide.IsSliding() || IsSkidding();
	// The foot is read only when a stop may start (a bone read per frame otherwise)
	const bool bMayStart = RunStop.Active == EGSTN_RunStopMode::Off && !bBlocked && RunStop.FramesReleased >= 0;
	const bool bLeftSwinging = bMayStart ? IsLeftFootSwinging() : RunStop.bLeftFoot;
	RunStop.Step(Settings, Mode, DeltaTime, Speed, ReadStick(), bBlocked, bLeftSwinging, &RunStopTracks[0], &RunStopTracks[1]);
	// Review evidence (rule 11): one line per presented frame of a stop, gstn.RunStop.Log 1
	static IConsoleVariable* LogVar = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.RunStop.Log"));
	if (LogVar && LogVar->GetInt() != 0 && (RunStop.Active != EGSTN_RunStopMode::Off || RunStop.FramesReleased >= 0))
	{
		UE_LOG(LogTemp, Log, TEXT("RunStop mode=%d active=%d left=%d t=%.3f speed=%.0f entry=%.0f released=%d stop=%.3f"), int32(Mode), int32(RunStop.Active),
			RunStop.bLeftFoot ? 1 : 0, RunStop.ClipTime, Speed, RunStop.EntrySpeed, RunStop.FramesReleased, RunStopTracks[RunStop.bLeftFoot ? 0 : 1].StopTime());
	}
}

UAnimSequenceBase* UGSTN_RunPresentationComponent::GetRunStopClip() const
{
	if (RunStop.IsBlend())
	{
		return RunStopIdleClip;
	}
	return RunStopClips[RunStop.bLeftFoot ? 0 : 1];
}

bool UGSTN_RunPresentationComponent::IsAnimOwnerRunStopping(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run && Run->IsRunStopping();
}

bool UGSTN_RunPresentationComponent::IsAnimOwnerRunStopBlending(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run && Run->IsRunStopBlending();
}

UAnimSequenceBase* UGSTN_RunPresentationComponent::GetAnimOwnerRunStopClip(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run ? Run->GetRunStopClip() : nullptr;
}

float UGSTN_RunPresentationComponent::GetAnimOwnerRunStopClipTime(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run ? Run->GetRunStopClipTime() : 0.f;
}

bool UGSTN_RunPresentationComponent::IsAnimOwnerRunStopLeftFoot(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return !Run || Run->GetRunStop().bLeftFoot;
}

namespace GSTNRunPresentation
{
	static TAutoConsoleVariable<int32> CVarFastCrouchMode(TEXT("gstn.FastCrouch.Mode"), -1,
		TEXT("Standing ground crouch: -1 = the locomotion profile's bFastCrouch, 0 = GASP's crouch, 1 = fast crouch clips"));
	static TAutoConsoleVariable<int32> CVarFastCrouchLog(TEXT("gstn.FastCrouch.Log"), 0, TEXT("1 = log the fast crouch phase every frame it is not Off"));
}

void UGSTN_RunPresentationComponent::TickFastCrouch(float DeltaTime)
{
	const UGSTN_LocomotionAnimSet* Profile = LeanProfile.Get();
	const FGSTN_FastCrouchSettings& Settings = Profile ? Profile->Crouch : DefaultFastCrouchSettings;
	const TSoftObjectPtr<UAnimSequenceBase>* Soft[2] = { &Settings.EnterClip, &Settings.ExitClip };
	for (int32 i = 0; i < 2; ++i)
	{
		const FSoftObjectPath Path = Soft[i]->ToSoftObjectPath();
		if (Path != ResolvedFastCrouchPaths[i])
		{
			ResolvedFastCrouchPaths[i] = Path;
			FastCrouchClips[i] = Path.IsValid() ? Soft[i]->LoadSynchronous() : nullptr;
		}
	}
	const int32 Forced = GSTNRunPresentation::CVarFastCrouchMode.GetValueOnGameThread();
	// No clip = nothing to play: GASP's crouch
	const bool bEnabled = (Forced >= 0 ? Forced != 0 : Settings.bFastCrouch) && FastCrouchClips[0] && FastCrouchClips[1];
	const FVector Up = Mover->GetUpDirection();
	const FVector Velocity = Mover->GetVelocity();
	const float Speed = static_cast<float>((Velocity - Up * Velocity.Dot(Up)).Size());
	const bool bGrounded = Mover->HasGameplayTag(Mover_IsOnGround, true);
	const bool bCrouched = bGrounded && Mover->HasGameplayTag(Mover_IsCrouching, true);
	FastCrouch.Step(bEnabled, bCrouched, bGrounded, Speed, DeltaTime, FastCrouchClips[0] ? FastCrouchClips[0]->GetPlayLength() : 0.f,
		FastCrouchClips[1] ? FastCrouchClips[1]->GetPlayLength() : 0.f, Settings.MoveSpeed);
	if (GSTNRunPresentation::CVarFastCrouchLog.GetValueOnGameThread() != 0 && FastCrouch.Phase != EGSTN_FastCrouchPhase::Off)
	{
		// Review evidence: the main state machine's current state (is the ABP following the phase?)
		FName StateName = NAME_None;
		const USkeletalMeshComponent* Mesh = GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
		if (UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr)
		{
			const int32 Machine = Anim->GetStateMachineIndex(TEXT("State Controller"));
			StateName = Machine != INDEX_NONE ? Anim->GetCurrentStateName(Machine) : FName(TEXT("no-machine"));
			// The ABP's own mode/blend variables (which branch of the anim graph is live), once per phase change
			static EGSTN_FastCrouchPhase LastLogged = EGSTN_FastCrouchPhase::Off;
			if (LastLogged != FastCrouch.Phase)
			{
				LastLogged = FastCrouch.Phase;
				FString Vars;
				for (TFieldIterator<FProperty> It(Anim->GetClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
				{
					const FProperty* Prop = *It;
					if (!(Prop->IsA<FBoolProperty>() || Prop->IsA<FIntProperty>() || Prop->IsA<FFloatProperty>() || Prop->IsA<FDoubleProperty>()
						|| Prop->IsA<FEnumProperty>() || Prop->IsA<FByteProperty>()))
					{
						continue;
					}
					FString Value;
					Prop->ExportTextItem_Direct(Value, Prop->ContainerPtrToValuePtr<void>(Anim), nullptr, Anim, PPF_None);
					Vars += FString::Printf(TEXT("%s=%s "), *Prop->GetName(), *Value);
				}
				UE_LOG(LogTemp, Log, TEXT("FastCrouch ABP vars: %s"), *Vars);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("FastCrouch phase=%d t=%.3f speed=%.0f crouched=%d state=%s anim=%s"), int32(FastCrouch.Phase), FastCrouch.Time, Speed,
			bCrouched ? 1 : 0, *StateName.ToString(), Mesh && Mesh->GetAnimInstance() ? *Mesh->GetAnimInstance()->GetClass()->GetName() : TEXT("none"));
	}
}

bool UGSTN_RunPresentationComponent::IsAnimOwnerFastCrouchEntering(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run && Run->FastCrouch.Phase == EGSTN_FastCrouchPhase::Enter;
}

bool UGSTN_RunPresentationComponent::IsAnimOwnerFastCrouchIdle(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run && Run->FastCrouch.Phase == EGSTN_FastCrouchPhase::Idle;
}

bool UGSTN_RunPresentationComponent::IsAnimOwnerFastCrouchExiting(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run && Run->FastCrouch.Phase == EGSTN_FastCrouchPhase::Exit;
}

bool UGSTN_RunPresentationComponent::IsAnimOwnerFastCrouching(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run && Run->FastCrouch.Phase != EGSTN_FastCrouchPhase::Off;
}

float UGSTN_RunPresentationComponent::GetAnimOwnerFastCrouchTime(const UAnimInstance* AnimInstance)
{
	const UGSTN_RunPresentationComponent* Run = FindRunPresentation(AnimInstance);
	return Run ? Run->FastCrouch.Time : 0.f;
}
