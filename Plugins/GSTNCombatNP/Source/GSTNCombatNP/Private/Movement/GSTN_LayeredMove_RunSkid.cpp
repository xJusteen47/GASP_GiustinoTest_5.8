#include "Movement/GSTN_LayeredMove_RunSkid.h"

#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
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
	// Test override of the kit's turn side (captures compare both sides); -1 = the kit's data. Same value on every machine or the sides diverge
	TAutoConsoleVariable<int32> CVarTurnSide(TEXT("gstn.Skid.TurnSide"), -1, TEXT("Skid exit turn side for a near-reverse: -1 kit data, 0 shortest, 1 right (+yaw), 2 left"));

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

	// Turn side (anim-setup-1): a near-reverse turns on the kit's side (the skid clip's), anything else the shorter way. Latched here so
	// every exit frame turns the same way: an exact 180 used to pick its side from float noise each frame
	TurnSign = 0;
	{
		const FVector Facing = FVector::VectorPlaneProject(Sync->GetOrientation_WorldSpace().Vector(), Up).GetSafeNormal();
		const double AngleDeg = Facing.IsNearlyZero() ? 0.0
			: FMath::RadiansToDegrees(FMath::Atan2(Up.Dot(Facing.Cross(TurnDirection)), Facing.Dot(TurnDirection)));
		int32 Side = static_cast<int32>(Tuning.TurnSide);
		const int32 SideOverride = GSTNRunSkid::CVarTurnSide.GetValueOnAnyThread();
		if (SideOverride >= 0 && SideOverride <= static_cast<int32>(EGSTN_SkidTurnSide::Left))
		{
			Side = SideOverride;
		}
		if (Side != static_cast<int32>(EGSTN_SkidTurnSide::Shortest) && FMath::Abs(AngleDeg) >= 180.0 - Tuning.ReverseConeDeg)
		{
			TurnSign = Side == static_cast<int32>(EGSTN_SkidTurnSide::Right) ? 1 : -1;
		}
		else if (FMath::Abs(AngleDeg) > 1.0)
		{
			TurnSign = AngleDeg > 0.0 ? 1 : -1;
		}
		StartFacing = Facing;
		double Total = TurnSign == 0 ? 0.0 : AngleDeg;
		if (TurnSign > 0 && Total < 0.0)
		{
			Total += 360.0;
		}
		else if (TurnSign < 0 && Total > 0.0)
		{
			Total -= 360.0;
		}
		TurnTotalDeg = static_cast<float>(Total);
	}

	// The floor's surface: its ledge rule wins over the kit's, its friction scales the brake (ice)
	FGSTN_SurfaceView Floor;
	const bool bHasFloor = FGSTN_GroundProbe::ResolveFloorSurface(StartState, Floor);
	const float Friction = bHasFloor ? Floor.Locomotion.FrictionScale : 1.f;
	const EGSTN_SurfaceLedgeRule SurfaceLedge = bHasFloor ? Floor.Locomotion.SkidLedge : EGSTN_SurfaceLedgeRule::Inherit;
	bStopAtLedge = SurfaceLedge == EGSTN_SurfaceLedgeRule::Inherit ? Tuning.Ledge == EGSTN_SkidLedge::StopAtLedge : SurfaceLedge == EGSTN_SurfaceLedgeRule::StopAtLedge;

	// skid-fix-1: the clip's lateral drift goes to its own side; a turn latched to the other side mirrors it
	ClipRootSign = 0;
	if (Tuning.FollowsClipRoot())
	{
		const int32 ClipSide = Tuning.TurnSide == EGSTN_SkidTurnSide::Left ? -1 : 1;
		ClipRootSign = static_cast<int8>(TurnSign != 0 && TurnSign != ClipSide ? -1 : 1);
	}

	Brake = FMath::Max(Tuning.Brake * Friction, 1.f);
	EnterFrames = Tuning.EnterFrames;
	ExitFrames = Tuning.ExitFrames;
	ExitSpeed = Tuning.ExitSpeed;
	JumpWindowStart = Tuning.JumpWindowStart;
	JumpWindowEnd = Tuning.JumpWindowEnd;
	LiveFrames = 0;
	CurrentLiveFrame = INDEX_NONE;
	return StartSpeed > KINDA_SMALL_NUMBER && !Direction.IsNearlyZero();
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
		RestFrame = FGSTN_RunSkidMath::FramesToRest(StartSpeed, Brake, DeltaSeconds);
		if (ClipRootSign != 0)
		{
			// skid-fix-1: the skid lasts the clip. The brake's rest frame only splits the phases for the presentation (VFX, lean); the
			// exit runs on to the clip's last frame instead of ExitFrames, so the walking mode (and the stick) never moves the capsule
			// while the clip still has a planted foot
			const FGSTN_LocomotionSimTuning* ClipLocomotion = FGSTN_LocomotionAttach::Find(MoverComp->GetOwner());
			const float ClipSeconds = ClipLocomotion ? ClipLocomotion->RunStages.Skid.RootTrackSeconds() : 0.f;
			const int32 ClipFrames = FMath::Max(FMath::CeilToInt32(ClipSeconds / DeltaSeconds), 2);
			RestFrame = FMath::Clamp(RestFrame, 1, ClipFrames - 1);
			ExitFrames = ClipFrames - RestFrame;
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

	// Clip-following turn (anim-setup-2): the kit's TurnCurve, read from the owner's tuning each step (asset data, the same on every
	// machine) so the move carries no array. The angular velocity puts the facing at StartFacing + TurnTotalDeg * share by the step's end
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const FGSTN_LocomotionSimTuning* Locomotion = FGSTN_LocomotionAttach::Find(Owner);
	const FGSTN_RunSkidTuning* SkidTuning = Locomotion ? &Locomotion->RunStages.Skid : nullptr;
	const float CurveShare = SkidTuning ? SkidTuning->SampleTurnCurve(static_cast<float>(CurrentLiveFrame + 1) * DeltaSeconds) : -1.f;
	const auto AngularToShare = [&](float Share) -> FVector
	{
		const FVector Facing = Sync ? FVector::VectorPlaneProject(Sync->GetOrientation_WorldSpace().Vector(), Up).GetSafeNormal() : FVector::ZeroVector;
		if (Facing.IsNearlyZero() || StartFacing.IsNearlyZero())
		{
			return FVector::ZeroVector;
		}
		const double CurrentDeg = FMath::RadiansToDegrees(FMath::Atan2(Up.Dot(StartFacing.Cross(Facing)), StartFacing.Dot(Facing)));
		const double DeltaDeg = FMath::UnwindDegrees(static_cast<double>(TurnTotalDeg) * Share - CurrentDeg);
		return Up * (DeltaDeg / (StepMs * 0.001));
	};

	if (ClipRootSign != 0 && SkidTuning && SkidTuning->FollowsClipRoot())
	{
		// skid-fix-1 (owner 2026-11-20, back to stock GASP): from the first frame to the clip's end the capsule moves on the clip's root path
		// (along the old direction, its own small drift to the side) and turns on the clip's yaw. No brake formula, no push toward the
		// stick, no early finish of the turn: the planted foot of the clip stays planted. The stick takes over when the move ends
		const float T0 = static_cast<float>(CurrentLiveFrame) * DeltaSeconds;
		const FVector2D Delta = SkidTuning->SampleRootTrack(T0 + DeltaSeconds) - SkidTuning->SampleRootTrack(T0);
		const FVector Side = Up.Cross(Direction);
		FVector ClipVelocity = (Direction * Delta.X + Side * (Delta.Y * static_cast<double>(ClipRootSign))) / static_cast<double>(DeltaSeconds);
		if (bStopAtLedge && !ClipVelocity.IsNearlyZero() && !FGSTN_GroundProbe::HasFloorAhead(StartState, *MoverComp, ClipVelocity, StepMs))
		{
			ClipVelocity = FVector::ZeroVector;
		}
		OutProposedMove.LinearVelocity = ClipVelocity;
		OutProposedMove.AngularVelocityDegrees = AngularToShare(CurveShare);
		return true;
	}

	if (Phase != EGSTN_RunSkidPhase::Exit)
	{
		const FVector SlideVelocity = Direction * FGSTN_RunSkidMath::SpeedAt(StartSpeed, Brake, CurrentLiveFrame, DeltaSeconds);
		if (!bStopAtLedge || FGSTN_GroundProbe::HasFloorAhead(StartState, *MoverComp, SlideVelocity, StepMs))
		{
			// The body slides along the old direction; it turns with the clip when the kit has a TurnCurve, else the turn comes in the exit
			OutProposedMove.LinearVelocity = SlideVelocity;
			OutProposedMove.AngularVelocityDegrees = CurveShare >= 0.f ? AngularToShare(CurveShare) : FVector::ZeroVector;
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
	if (CurveShare >= 0.f)
	{
		// The clip's turn, and whatever it has left when the skid came to rest spread evenly over the exit (all done by its last frame)
		const float RestShare = SkidTuning->SampleTurnCurve(static_cast<float>(RestFrame) * DeltaSeconds);
		const float ExitShare = FMath::Lerp(RestShare, 1.f, static_cast<float>(ExitFrame + 1) / static_cast<float>(ExitLength));
		Angular = AngularToShare(FMath::Max(CurveShare, ExitShare));
	}
	else if (Sync)
	{
		const FVector Facing = FVector::VectorPlaneProject(Sync->GetOrientation_WorldSpace().Vector(), Up).GetSafeNormal();
		if (!Facing.IsNearlyZero())
		{
			// Signed angle about Up from the facing to the new direction (UE: X cross Y = Z, a positive yaw turns X to Y)
			const double Sin = Up.Dot(Facing.Cross(TurnDirection));
			const double Cos = Facing.Dot(TurnDirection);
			double AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Sin, Cos));
			// Keep the latched side: the remaining angle shrinks on that side; only the far side's wrap (past 90) is unwrapped, so a
			// facing already on target never spins round
			if (TurnSign > 0 && AngleDeg < -90.0)
			{
				AngleDeg += 360.0;
			}
			else if (TurnSign < 0 && AngleDeg > 90.0)
			{
				AngleDeg -= 360.0;
			}
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
	// Full precision: the speed of every frame is recomputed from these
	Ar << Direction;
	Ar << TurnDirection;
	Ar << TurnSign;
	Ar << StartFacing;
	Ar << TurnTotalDeg;
	Ar << ClipRootSign;
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
