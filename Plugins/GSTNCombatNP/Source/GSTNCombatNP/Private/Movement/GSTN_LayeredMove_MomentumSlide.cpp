#include "Movement/GSTN_LayeredMove_MomentumSlide.h"

#include "Components/PrimitiveComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GSTN_CombatNPTags.h"
#include "MontageSimulator/NetMontageSimulatorData.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"
#include "Simulation/GSTN_CombatSimComponent.h"

namespace GSTNMomentumSlide
{
	/** A safety net only: the move ends itself long before (the end of its timeline at the latest) */
	constexpr float MaxLifeMs = 10000.f;
}

FGSTN_LayeredMove_MomentumSlide::FGSTN_LayeredMove_MomentumSlide()
{
	MixMode = EMoveMixMode::OverrideVelocity;
	Priority = 0;
	DurationMs = GSTNMomentumSlide::MaxLifeMs;
}

float FGSTN_LayeredMove_MomentumSlide::SpeedAtLiveFrame(int32 LiveFrame, float DeltaSeconds) const
{
	FGSTN_MomentumSlideBraking Braking;
	Braking.Deceleration = Deceleration;
	Braking.Friction = Friction;
	float Speed = StartSpeed;
	const int32 Steps = FMath::Min(LiveFrame, MaxSimulatedFrames);
	for (int32 Step = 0; Step < Steps && Speed > 0.f; ++Step)
	{
		Speed = FGSTN_MomentumSlideMath::StepSpeed(Speed, Braking, DeltaSeconds);
	}
	return Speed;
}

void FGSTN_LayeredMove_MomentumSlide::Finish(const FMoverTimeStep& TimeStep)
{
	Phase = static_cast<uint8>(EGSTN_MomentumSlidePhase::Done);
	// Finished at the next flush: start + duration lands on the start time of the next step
	if (StartSimTimeMs >= 0.0)
	{
		DurationMs = FMath::Max(static_cast<float>(TimeStep.BaseSimTimeMs + TimeStep.StepMs - StartSimTimeMs), 1.f);
	}
}

bool FGSTN_LayeredMove_MomentumSlide::Decide(const FMoverTickStartData& StartState, const UMoverComponent& MoverComp, const FGSTN_MomentumSlide& Data, int32 RunningStartFrame)
{
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const UCommonLegacyMovementSettings* Settings = MoverComp.FindSharedSettings<UCommonLegacyMovementSettings>();
	if (!Sync || !Settings || StartState.SyncState.MovementMode != Settings->GroundMovementModeName)
	{
		// Air moves and wall moves keep their behaviour
		return false;
	}

	const FVector Up = MoverComp.GetUpDirection();
	const FVector FlatVelocity = FVector::VectorPlaneProject(Sync->GetVelocity_WorldSpace(), Up);
	const FVector FlatForward = FVector::VectorPlaneProject(Sync->GetOrientation_WorldSpace().Vector(), Up).GetSafeNormal();
	bool bPickedAgainst = false;
	const FGSTN_MomentumSlideResponse* Response = FGSTN_MomentumSlideMath::PickResponse(Data, FlatVelocity, FlatForward, bPickedAgainst);
	bAgainst = bPickedAgainst;
	if (!Response)
	{
		return false;
	}

	// The braking of Mover's legacy walking: a constant deceleration plus a friction proportional to the speed
	const float CharacterFriction = (Settings->bUseSeparateBrakingFriction ? Settings->BrakingFriction : Settings->GroundFriction) * FMath::Max(Settings->BrakingFrictionFactor, 0.f);
	const FGSTN_MomentumSlideBraking Braking = FGSTN_MomentumSlideMath::ResolveBraking(*Response, Settings->Deceleration, CharacterFriction);

	Direction = FlatVelocity.GetSafeNormal();
	StartSpeed = FGSTN_MomentumSlideMath::StartSpeed(*Response, static_cast<float>(FlatVelocity.Size()));
	Deceleration = Braking.Deceleration;
	Friction = Braking.Friction;
	MaxFrames = Response->MaxFrames;
	bStopAtLedge = Data.bStopAtLedge;
	bAddRootMotion = Data.bAddRootMotion;
	TimelineStartFrame = RunningStartFrame;
	LiveFrames = 0;
	CurrentLiveFrame = INDEX_NONE;
	return StartSpeed > 0.f;
}

bool FGSTN_LayeredMove_MomentumSlide::IsFrozen(const FMoverTickStartData& StartState)
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

bool FGSTN_LayeredMove_MomentumSlide::FindRootMotion(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp,
	UMoverBlackboard* SimBlackboard, const FVector& Up, FVector& OutGroundVelocity, FVector& OutAngularVelocityDegrees)
{
	// The montage queues one root motion move per frame (duration 0). In the start state of the step it is still among the queued moves:
	// Mover copies the start state before it makes the queued moves active. An older one among the active moves is last frame's, so it is ignored
	for (const TSharedPtr<FLayeredMoveBase>& Move : StartState.SyncState.LayeredMoves.GetQueuedMoves())
	{
		if (!Move.IsValid() || !Move->GetScriptStruct()->IsChildOf(FLayeredMove_SyncMontageRootMotion::StaticStruct()))
		{
			continue;
		}
		// GenerateMove writes the copy's mix mode, never the queued move
		FLayeredMove_SyncMontageRootMotion RootMotion(*static_cast<const FLayeredMove_SyncMontageRootMotion*>(Move.Get()));
		FProposedMove Proposed;
		if (!RootMotion.GenerateMove(StartState, TimeStep, MoverComp, SimBlackboard, Proposed) || Proposed.MixMode != EMoveMixMode::OverrideVelocity)
		{
			// No root motion, or a montage that lets the movement mode move it (AdditiveRootMotion curve)
			return false;
		}
		OutGroundVelocity = FVector::VectorPlaneProject(Proposed.LinearVelocity, Up);
		OutAngularVelocityDegrees = Proposed.AngularVelocityDegrees;
		return true;
	}
	return false;
}

bool FGSTN_LayeredMove_MomentumSlide::HasFloorAhead(const FMoverTickStartData& StartState, const UMoverComponent& MoverComp, const FVector& Velocity, double StepMs)
{
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(MoverComp.GetUpdatedComponent());
	if (!Sync || !Primitive || !Primitive->GetWorld())
	{
		return true;
	}
	// The probe of the push back: the next step's feet position must have floor under it
	const FVector Up = MoverComp.GetUpDirection();
	const double HalfHeight = Primitive->Bounds.BoxExtent.GetMax();
	const FVector Next = Sync->GetLocation_WorldSpace() + Velocity * (StepMs * 0.001);
	const FVector Start = Next - Up * (HalfHeight - 10.0);
	const FVector End = Next - Up * (HalfHeight + LedgeProbeDepth);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNMomentumSlideLedge), false, Primitive->GetOwner());
	return Primitive->GetWorld()->LineTraceTestByChannel(Start, End, ECC_Visibility, Params);
}

bool FGSTN_LayeredMove_MomentumSlide::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	OutProposedMove.MixMode = EMoveMixMode::OverrideVelocity;
	if (Phase == static_cast<uint8>(EGSTN_MomentumSlidePhase::Done) || !MoverComp)
	{
		return false;
	}

	const AActor* Owner = MoverComp->GetOwner();
	const UGSTN_CombatSimComponent* Combat = Owner ? Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	int32 RunningStartFrame = INDEX_NONE;
	const FGSTN_MomentumSlide* Data = nullptr;
	const bool bRunning = Combat && Combat->ReadRunningSlide(RunningStartFrame, Data) && Data;

	if (Phase == static_cast<uint8>(EGSTN_MomentumSlidePhase::Undecided))
	{
		if (!bRunning || !Decide(StartState, *MoverComp, *Data, RunningStartFrame))
		{
			Finish(TimeStep);
			return false;
		}
		Phase = static_cast<uint8>(EGSTN_MomentumSlidePhase::Sliding);
	}
	else
	{
		// The move it belongs to was replaced, interrupted or ended, or the character left the ground
		const UCommonLegacyMovementSettings* Settings = MoverComp->FindSharedSettings<UCommonLegacyMovementSettings>();
		if (!bRunning || RunningStartFrame != TimelineStartFrame || !Settings || StartState.SyncState.MovementMode != Settings->GroundMovementModeName)
		{
			Finish(TimeStep);
			return false;
		}
	}

	// Count the step once per simulation time: a refunded substep generates again with the same time
	const bool bFrozen = IsFrozen(StartState);
	if (TimeStep.BaseSimTimeMs != LastStepTimeMs)
	{
		LastStepTimeMs = TimeStep.BaseSimTimeMs;
		CurrentLiveFrame = bFrozen ? INDEX_NONE : LiveFrames++;
	}
	if (CurrentLiveFrame == INDEX_NONE)
	{
		// Hitstop: the freeze holds the pawn and the slide waits
		return false;
	}

	const double StepMs = FMath::Max<double>(TimeStep.StepMs, UE_SMALL_NUMBER);
	const float Speed = SpeedAtLiveFrame(CurrentLiveFrame, static_cast<float>(StepMs * 0.001));
	if (Speed <= 0.f || (MaxFrames > 0 && CurrentLiveFrame >= MaxFrames))
	{
		Finish(TimeStep);
		return false;
	}

	const FVector SlideVelocity = Direction * Speed;
	if (bStopAtLedge && !HasFloorAhead(StartState, *MoverComp, SlideVelocity, StepMs))
	{
		Finish(TimeStep);
		return false;
	}

	FVector RootGround = FVector::ZeroVector;
	FVector RootAngular = FVector::ZeroVector;
	if (bAddRootMotion)
	{
		FindRootMotion(StartState, TimeStep, MoverComp, SimBlackboard, MoverComp->GetUpDirection(), RootGround, RootAngular);
	}
	OutProposedMove.LinearVelocity = SlideVelocity + RootGround;
	OutProposedMove.AngularVelocityDegrees = RootAngular;
	return true;
}

FLayeredMoveBase* FGSTN_LayeredMove_MomentumSlide::Clone() const
{
	return new FGSTN_LayeredMove_MomentumSlide(*this);
}

void FGSTN_LayeredMove_MomentumSlide::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << Phase;
	uint8 bAgainstBit = bAgainst ? 1 : 0;
	uint8 bLedgeBit = bStopAtLedge ? 1 : 0;
	uint8 bRootBit = bAddRootMotion ? 1 : 0;
	Ar.SerializeBits(&bAgainstBit, 1);
	Ar.SerializeBits(&bLedgeBit, 1);
	Ar.SerializeBits(&bRootBit, 1);
	bAgainst = bAgainstBit != 0;
	bStopAtLedge = bLedgeBit != 0;
	bAddRootMotion = bRootBit != 0;
	// Full precision: the speed of every frame is recomputed from these, so a quantised copy would drift from the authority's
	Ar << Direction;
	Ar << StartSpeed;
	Ar << Deceleration;
	Ar << Friction;
	Ar << MaxFrames;
	Ar << TimelineStartFrame;
	Ar << LiveFrames;
	Ar << CurrentLiveFrame;
	Ar << LastStepTimeMs;
}

UScriptStruct* FGSTN_LayeredMove_MomentumSlide::GetScriptStruct() const
{
	return FGSTN_LayeredMove_MomentumSlide::StaticStruct();
}

FString FGSTN_LayeredMove_MomentumSlide::ToSimpleString() const
{
	return FString::Printf(TEXT("MomentumSlide (%s, frame %d)"), bAgainst ? TEXT("against") : TEXT("along"), CurrentLiveFrame);
}

FEffectMoveContainer UGSTN_MomentumSlideMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	// Undecided: the move reads everything on its first frame
	FGSTN_LayeredMove_MomentumSlide Move;
	return FEffectMoveContainer(&Move);
}

UGSTN_GE_MomentumSlide::UGSTN_GE_MomentumSlide()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	UGSTN_MomentumSlideMoveCreator* Creator = CreateDefaultSubobject<UGSTN_MomentumSlideMoveCreator>(TEXT("MomentumSlideMoveCreator"));
	Creator->LayeredMoveDefaultParams.CopyDurationFromEffect = false;
	ApplyMove->MoveProcessor = Creator;
	GEComponents.Add(ApplyMove);
}
