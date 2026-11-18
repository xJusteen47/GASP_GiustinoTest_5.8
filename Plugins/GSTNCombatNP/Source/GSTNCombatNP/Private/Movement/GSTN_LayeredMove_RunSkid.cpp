#include "Movement/GSTN_LayeredMove_RunSkid.h"

#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "GameFramework/Actor.h"
#include "GSTN_CombatNPTags.h"
#include "GSTN_SurfaceTypes.h"
#include "Kits/GSTN_LocomotionProfile.h"
#include "Movement/GSTN_GroundProbe.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatVictim.h"

namespace GSTNRunSkid
{
	/** A safety net only: the move ends itself long before */
	constexpr float MaxLifeMs = 10000.f;

	bool IsFrozen(const FMoverTickStartData& StartState)
	{
		const FGameplayTag Frozen = GSTNCombatNPTags::Mover_Frozen.GetTag();
		const FLayeredMoveGroup& Moves = StartState.SyncState.LayeredMoves;
		for (const TArray<TSharedPtr<FLayeredMoveBase>>* List : { &Moves.GetActiveMoves(), &Moves.GetQueuedMoves() })
		{
			for (const TSharedPtr<FLayeredMoveBase>& Move : *List)
			{
				if (Move.IsValid() && Move->HasGameplayTag(Frozen, true))
				{
					return true;
				}
			}
		}
		return false;
	}
}

FGSTN_LayeredMove_RunSkid::FGSTN_LayeredMove_RunSkid()
{
	MixMode = EMoveMixMode::OverrideVelocity;
	Priority = 0;
	DurationMs = GSTNRunSkid::MaxLifeMs;
}

FGSTN_RunSkidTuning FGSTN_LayeredMove_RunSkid::MakeTuning() const
{
	FGSTN_RunSkidTuning Tuning;
	Tuning.Brake = Brake;
	Tuning.EnterFrames = EnterFrames;
	Tuning.ExitFrames = ExitFrames;
	Tuning.ExitSpeed = ExitSpeed;
	Tuning.JumpWindowStart = JumpWindowStart;
	Tuning.JumpWindowEnd = JumpWindowEnd;
	return Tuning;
}

EGSTN_RunSkidPhase FGSTN_LayeredMove_RunSkid::GetPhase() const
{
	if (Life != static_cast<uint8>(EGSTN_RunSkidLife::Running) || CurrentLiveFrame == INDEX_NONE)
	{
		// A frozen step keeps the phase of the last live frame
		return Life == static_cast<uint8>(EGSTN_RunSkidLife::Running) && LiveFrames > 0 ? FGSTN_RunSkidMath::PhaseAt(MakeTuning(), LiveFrames - 1, RestFrame) : EGSTN_RunSkidPhase::None;
	}
	return FGSTN_RunSkidMath::PhaseAt(MakeTuning(), CurrentLiveFrame, RestFrame);
}

float FGSTN_LayeredMove_RunSkid::GetSlideSpeed(float DeltaSeconds) const
{
	if (bClipTrack)
	{
		return 0.f;
	}
	const int32 Frame = CurrentLiveFrame == INDEX_NONE ? LiveFrames - 1 : CurrentLiveFrame;
	return Frame >= 0 && Frame < RestFrame ? FGSTN_RunSkidMath::SpeedAt(StartSpeed, Brake, Frame, DeltaSeconds) : 0.f;
}

const FGSTN_LayeredMove_RunSkid* FGSTN_LayeredMove_RunSkid::FindActive(const FMoverSyncState& SyncState)
{
	for (const TSharedPtr<FLayeredMoveBase>& Move : SyncState.LayeredMoves.GetActiveMoves())
	{
		if (Move.IsValid() && Move->GetScriptStruct() == FGSTN_LayeredMove_RunSkid::StaticStruct())
		{
			const FGSTN_LayeredMove_RunSkid& Skid = static_cast<const FGSTN_LayeredMove_RunSkid&>(*Move);
			if (Skid.Life != static_cast<uint8>(EGSTN_RunSkidLife::Done))
			{
				return &Skid;
			}
		}
	}
	return nullptr;
}

void FGSTN_LayeredMove_RunSkid::Finish(const FMoverTimeStep& TimeStep)
{
	Life = static_cast<uint8>(EGSTN_RunSkidLife::Done);
	// Finished at the next flush: start + duration lands on the start time of the next step
	if (StartSimTimeMs >= 0.0)
	{
		DurationMs = FMath::Max(static_cast<float>(TimeStep.BaseSimTimeMs + TimeStep.StepMs - StartSimTimeMs), 1.f);
	}
}

bool FGSTN_LayeredMove_RunSkid::Decide(const FMoverTickStartData& StartState, const UMoverComponent& MoverComp)
{
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const UCommonLegacyMovementSettings* Settings = MoverComp.FindSharedSettings<UCommonLegacyMovementSettings>();
	const AActor* Owner = MoverComp.GetOwner();
	const FGSTN_LocomotionSimTuning* Locomotion = FGSTN_LocomotionAttach::Find(Owner);
	const UGSTN_CombatSimComponent* Combat = Owner ? Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	if (!Sync || !Settings || !Locomotion || !Combat || StartState.SyncState.MovementMode != Settings->GroundMovementModeName)
	{
		return false;
	}
	const FGSTN_RunSkidTuning& Tuning = Locomotion->RunStages.Skid;

	const FVector Up = MoverComp.GetUpDirection();
	const FVector FlatVelocity = FVector::VectorPlaneProject(Sync->GetVelocity_WorldSpace(), Up);
	FVector2D Stick;
	float CameraYaw = 0.f;
	Combat->GetFrameStick(Stick, CameraYaw);
	const FVector StickDir = Stick.IsNearlyZero() ? FVector::ZeroVector : FGSTN_CombatVictim::StickToPlane(Stick, CameraYaw, Up).GetSafeNormal();

	Direction = FlatVelocity.GetSafeNormal();
	// The stick of the frame the combat tick decided on; without one (a forced test start) the skid turns all the way round
	TurnDirection = StickDir.IsNearlyZero() ? -Direction : StickDir;
	StartSpeed = static_cast<float>(FlatVelocity.Size());

	// The floor's surface: its ledge rule wins over the kit's, its friction scales the brake (ice)
	FGSTN_SurfaceView Floor;
	const bool bHasFloor = FGSTN_GroundProbe::ResolveFloorSurface(StartState, Floor);
	const float Friction = bHasFloor ? Floor.Locomotion.FrictionScale : 1.f;
	const EGSTN_SurfaceLedgeRule SurfaceLedge = bHasFloor ? Floor.Locomotion.SkidLedge : EGSTN_SurfaceLedgeRule::Inherit;
	bStopAtLedge = SurfaceLedge == EGSTN_SurfaceLedgeRule::Inherit ? Tuning.Ledge == EGSTN_SkidLedge::StopAtLedge : SurfaceLedge == EGSTN_SurfaceLedgeRule::StopAtLedge;

	Brake = FMath::Max(Tuning.Brake * Friction, 1.f);
	EnterFrames = Tuning.EnterFrames;
	ExitFrames = Tuning.ExitFrames;
	ExitSpeed = Tuning.ExitSpeed;
	JumpWindowStart = Tuning.JumpWindowStart;
	JumpWindowEnd = Tuning.JumpWindowEnd;
	LiveFrames = 0;
	CurrentLiveFrame = INDEX_NONE;

	// Clip track: the turn goes from this frame's facing to the new direction, the way the clip's root yaw goes
	bClipTrack = Tuning.RootTrack.IsUsable();
	StartFacing = FVector::VectorPlaneProject(Sync->GetOrientation_WorldSpace().Vector(), Up).GetSafeNormal();
	if (StartFacing.IsNearlyZero())
	{
		StartFacing = Direction;
	}
	TurnAngleDeg = 0.f;
	if (bClipTrack)
	{
		const double Sin = Up.Dot(StartFacing.Cross(TurnDirection));
		const double Cos = StartFacing.Dot(TurnDirection);
		double Angle = FMath::RadiansToDegrees(FMath::Atan2(Sin, Cos));
		// A reversal near 180: turn the side the clip turns, so the capsule and the mesh agree
		const double ClipSign = Tuning.RootTrack.TurnSign >= 0.f ? 1.0 : -1.0;
		if (FMath::Abs(Angle) > 170.0 && FMath::Sign(Angle) != ClipSign)
		{
			Angle -= FMath::Sign(Angle) * 360.0;
		}
		TurnAngleDeg = static_cast<float>(Angle);
	}
	return StartSpeed > KINDA_SMALL_NUMBER && !Direction.IsNearlyZero();
}

const FGSTN_RunSkidRootTrack* FGSTN_LayeredMove_RunSkid::FindRootTrack(const UMoverComponent& MoverComp)
{
	const FGSTN_LocomotionSimTuning* Locomotion = FGSTN_LocomotionAttach::Find(MoverComp.GetOwner());
	return Locomotion && Locomotion->RunStages.Skid.RootTrack.IsUsable() ? &Locomotion->RunStages.Skid.RootTrack : nullptr;
}

bool FGSTN_LayeredMove_RunSkid::GenerateClipTrackMove(const FMoverTickStartData& StartState, const UMoverComponent& MoverComp, const FGSTN_RunSkidRootTrack& Track,
	double StepMs, FProposedMove& OutProposedMove)
{
	const float DeltaSeconds = static_cast<float>(StepMs * 0.001);
	const FVector Up = MoverComp.GetUpDirection();

	// Signed along the old direction: braking while > 0, then the push-off along the new direction
	const float Ground = FGSTN_RunSkidMath::TrackGroundSpeed(Track, CurrentLiveFrame, DeltaSeconds, StartSpeed);
	FVector Velocity = Ground >= 0.f ? Direction * Ground : TurnDirection * -Ground;
	if (bStopAtLedge && !Velocity.IsNearlyZero() && !FGSTN_GroundProbe::HasFloorAhead(StartState, MoverComp, Velocity, StepMs))
	{
		// The edge: hold here, the clip goes on (the body brakes and turns in place)
		Velocity = FVector::ZeroVector;
	}

	// Facing: where the clip's root yaw is at the end of this frame, an absolute target so nothing drifts over the frames
	FVector Angular = FVector::ZeroVector;
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (Sync)
	{
		const FVector Facing = FVector::VectorPlaneProject(Sync->GetOrientation_WorldSpace().Vector(), Up).GetSafeNormal();
		const FVector Target = StartFacing.RotateAngleAxis(TurnAngleDeg * FGSTN_RunSkidMath::TrackTurnAt(Track, CurrentLiveFrame, DeltaSeconds), Up);
		if (!Facing.IsNearlyZero())
		{
			// Signed angle about Up (UE: X cross Y = Z, a positive yaw turns X to Y)
			const double AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Up.Dot(Facing.Cross(Target)), Facing.Dot(Target)));
			Angular = Up * (AngleDeg / FMath::Max<double>(DeltaSeconds, UE_SMALL_NUMBER));
		}
	}
	OutProposedMove.LinearVelocity = Velocity;
	OutProposedMove.AngularVelocityDegrees = Angular;
	return true;
}

bool FGSTN_LayeredMove_RunSkid::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	OutProposedMove.MixMode = EMoveMixMode::OverrideVelocity;
	if (Life == static_cast<uint8>(EGSTN_RunSkidLife::Done) || !MoverComp)
	{
		return false;
	}

	const double StepMs = FMath::Max<double>(TimeStep.StepMs, UE_SMALL_NUMBER);
	const float DeltaSeconds = static_cast<float>(StepMs * 0.001);

	if (Life == static_cast<uint8>(EGSTN_RunSkidLife::Undecided))
	{
		if (!Decide(StartState, *MoverComp))
		{
			Finish(TimeStep);
			return false;
		}
		const FGSTN_RunSkidRootTrack* DecidedTrack = bClipTrack ? FindRootTrack(*MoverComp) : nullptr;
		if (DecidedTrack)
		{
			// The clip decides the phases: braking until its root speed turns round, then the turn and push-off to its end
			const int32 Frames = FGSTN_RunSkidMath::TrackFrames(*DecidedTrack, DeltaSeconds);
			RestFrame = FGSTN_RunSkidMath::TrackRestFrame(*DecidedTrack, DeltaSeconds);
			ExitFrames = FMath::Max(Frames - RestFrame, 0);
			EnterFrames = FMath::Min(EnterFrames, RestFrame);
		}
		else
		{
			bClipTrack = false;
			RestFrame = FGSTN_RunSkidMath::FramesToRest(StartSpeed, Brake, DeltaSeconds);
		}
		Life = static_cast<uint8>(EGSTN_RunSkidLife::Running);
	}

	// Off the ground (a jump from the skid, a SlideOff ledge) or no longer free to move (an attack, a hit): whatever replaces it takes over
	const UCommonLegacyMovementSettings* Settings = MoverComp->FindSharedSettings<UCommonLegacyMovementSettings>();
	const AActor* Owner = MoverComp->GetOwner();
	const UGSTN_CombatSimComponent* Combat = Owner ? Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	if (!Settings || StartState.SyncState.MovementMode != Settings->GroundMovementModeName || !Combat || !Combat->IsFreeToMove())
	{
		Finish(TimeStep);
		return false;
	}

	// Count the step once per simulation time: a refunded substep generates again with the same time
	const bool bFrozen = GSTNRunSkid::IsFrozen(StartState);
	if (TimeStep.BaseSimTimeMs != LastStepTimeMs)
	{
		LastStepTimeMs = TimeStep.BaseSimTimeMs;
		CurrentLiveFrame = bFrozen ? INDEX_NONE : LiveFrames++;
	}
	if (CurrentLiveFrame == INDEX_NONE)
	{
		return false;
	}

	const FVector Up = MoverComp->GetUpDirection();
	const EGSTN_RunSkidPhase Phase = FGSTN_RunSkidMath::PhaseAt(MakeTuning(), CurrentLiveFrame, RestFrame);
	if (Phase == EGSTN_RunSkidPhase::None)
	{
		Finish(TimeStep);
		return false;
	}

	if (bClipTrack)
	{
		const FGSTN_RunSkidRootTrack* Track = FindRootTrack(*MoverComp);
		if (!Track)
		{
			// The kit lost its track mid-skid (loadout change): the walking mode takes over
			Finish(TimeStep);
			return false;
		}
		return GenerateClipTrackMove(StartState, *MoverComp, *Track, StepMs, OutProposedMove);
	}

	if (Phase != EGSTN_RunSkidPhase::Exit)
	{
		const FVector SlideVelocity = Direction * FGSTN_RunSkidMath::SpeedAt(StartSpeed, Brake, CurrentLiveFrame, DeltaSeconds);
		if (!bStopAtLedge || FGSTN_GroundProbe::HasFloorAhead(StartState, *MoverComp, SlideVelocity, StepMs))
		{
			// Facing held: the body slides along the old direction, the turn comes in the exit
			OutProposedMove.LinearVelocity = SlideVelocity;
			OutProposedMove.AngularVelocityDegrees = FVector::ZeroVector;
			return true;
		}
		// The edge: the skid comes to rest here and its exit starts now
		RestFrame = CurrentLiveFrame;
	}

	// Exit: turn to the new direction over the exit frames and push off, the speed growing to ExitSpeed on the last one
	const int32 ExitFrame = CurrentLiveFrame - RestFrame;
	const int32 ExitLength = FMath::Max(ExitFrames, 1);
	const float Push = ExitSpeed * static_cast<float>(ExitFrame + 1) / static_cast<float>(ExitLength);
	FVector PushVelocity = TurnDirection * Push;
	if (bStopAtLedge && Push > 0.f && !FGSTN_GroundProbe::HasFloorAhead(StartState, *MoverComp, PushVelocity, StepMs))
	{
		PushVelocity = FVector::ZeroVector;
	}

	FVector Angular = FVector::ZeroVector;
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (Sync)
	{
		const FVector Facing = FVector::VectorPlaneProject(Sync->GetOrientation_WorldSpace().Vector(), Up).GetSafeNormal();
		if (!Facing.IsNearlyZero())
		{
			// Signed angle about Up from the facing to the new direction (UE: X cross Y = Z, a positive yaw turns X to Y)
			const double Sin = Up.Dot(Facing.Cross(TurnDirection));
			const double Cos = Facing.Dot(TurnDirection);
			const double AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Sin, Cos));
			const int32 FramesLeft = FMath::Max(ExitLength - ExitFrame, 1);
			Angular = Up * (AngleDeg / (FramesLeft * StepMs * 0.001));
		}
	}
	OutProposedMove.LinearVelocity = PushVelocity;
	OutProposedMove.AngularVelocityDegrees = Angular;
	return true;
}

void FGSTN_LayeredMove_RunSkid::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	if (Life != static_cast<uint8>(EGSTN_RunSkidLife::Running))
	{
		return;
	}
	InOutTags.AddTag(GSTNCombatNPTags::Movement_State_Skid);
	const int32 Frame = CurrentLiveFrame == INDEX_NONE ? LiveFrames - 1 : CurrentLiveFrame;
	if (Frame >= 0 && FGSTN_RunSkidMath::InJumpWindow(MakeTuning(), Frame))
	{
		InOutTags.AddTag(GSTNCombatNPTags::Movement_State_Skid_JumpWindow);
	}
}

bool FGSTN_LayeredMove_RunSkid::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	FGameplayTagContainer Tags;
	GetGameplayTags(Tags);
	return bExactMatch ? Tags.HasTagExact(TagToFind) : Tags.HasTag(TagToFind);
}

FLayeredMoveBase* FGSTN_LayeredMove_RunSkid::Clone() const
{
	return new FGSTN_LayeredMove_RunSkid(*this);
}

void FGSTN_LayeredMove_RunSkid::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << Life;
	uint8 bLedgeBit = bStopAtLedge ? 1 : 0;
	Ar.SerializeBits(&bLedgeBit, 1);
	bStopAtLedge = bLedgeBit != 0;
	uint8 bTrackBit = bClipTrack ? 1 : 0;
	Ar.SerializeBits(&bTrackBit, 1);
	bClipTrack = bTrackBit != 0;
	Ar << StartFacing;
	Ar << TurnAngleDeg;
	// Full precision: the speed of every frame is recomputed from these
	Ar << Direction;
	Ar << TurnDirection;
	Ar << StartSpeed;
	Ar << Brake;
	Ar << EnterFrames;
	Ar << ExitFrames;
	Ar << ExitSpeed;
	Ar << JumpWindowStart;
	Ar << JumpWindowEnd;
	Ar << RestFrame;
	Ar << LiveFrames;
	Ar << CurrentLiveFrame;
	Ar << LastStepTimeMs;
}

UScriptStruct* FGSTN_LayeredMove_RunSkid::GetScriptStruct() const
{
	return FGSTN_LayeredMove_RunSkid::StaticStruct();
}

FString FGSTN_LayeredMove_RunSkid::ToSimpleString() const
{
	return FString::Printf(TEXT("RunSkid (phase %d, frame %d, rest %d)"), static_cast<int32>(GetPhase()), CurrentLiveFrame, RestFrame);
}

FEffectMoveContainer UGSTN_RunSkidMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	// Undecided: the move reads everything on its own first frame
	FGSTN_LayeredMove_RunSkid Move;
	return FEffectMoveContainer(&Move);
}

UGSTN_GE_RunSkid::UGSTN_GE_RunSkid()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	UGSTN_RunSkidMoveCreator* Creator = CreateDefaultSubobject<UGSTN_RunSkidMoveCreator>(TEXT("RunSkidMoveCreator"));
	Creator->LayeredMoveDefaultParams.CopyDurationFromEffect = false;
	ApplyMove->MoveProcessor = Creator;
	GEComponents.Add(ApplyMove);
}
