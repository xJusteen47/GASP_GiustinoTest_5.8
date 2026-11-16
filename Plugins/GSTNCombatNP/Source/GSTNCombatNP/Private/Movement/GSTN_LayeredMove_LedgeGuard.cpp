// The Stop action of the Ledge Check track (owner 2026-11-16, FGSTN_TimelineTrack_LedgeCheck).

#include "Movement/GSTN_LayeredMove_LedgeGuard.h"

#include "GameFramework/Actor.h"
#include "GSTN_CombatNPTags.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"
#include "Simulation/GSTN_CombatSimComponent.h"

namespace GSTNLedgeGuard
{
	/** A safety net only: the move ends with its timeline long before */
	constexpr float MaxLifeMs = 10000.f;

	/** A hitstop freeze holds the pawn (a move with Combat.Mover.Frozen, active or queued this frame) */
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

FGSTN_LayeredMove_LedgeGuard::FGSTN_LayeredMove_LedgeGuard()
{
	MixMode = EMoveMixMode::OverrideVelocity;
	Priority = 1;
	DurationMs = GSTNLedgeGuard::MaxLifeMs;
}

void FGSTN_LayeredMove_LedgeGuard::Finish(const FMoverTimeStep& TimeStep)
{
	bDone = true;
	// Finished at the next flush: start + duration lands on the start time of the next step
	if (StartSimTimeMs >= 0.0)
	{
		DurationMs = FMath::Max(static_cast<float>(TimeStep.BaseSimTimeMs + TimeStep.StepMs - StartSimTimeMs), 1.f);
	}
}

bool FGSTN_LayeredMove_LedgeGuard::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	OutProposedMove.MixMode = EMoveMixMode::OverrideVelocity;
	if (bDone || !MoverComp)
	{
		return false;
	}
	const AActor* Owner = MoverComp->GetOwner();
	const UGSTN_CombatSimComponent* Combat = Owner ? Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	int32 RunningStartFrame = INDEX_NONE;
	bool bStop = false;
	if (!Combat || !Combat->ReadLedgeStop(RunningStartFrame, bStop) || (TimelineStartFrame != INDEX_NONE && RunningStartFrame != TimelineStartFrame))
	{
		// The move it belongs to ended, was replaced or recoiled
		Finish(TimeStep);
		return false;
	}
	TimelineStartFrame = RunningStartFrame;
	if (!bStop || GSTNLedgeGuard::IsFrozen(StartState))
	{
		return false;
	}
	// Held in the ground plane; the vertical part of the current velocity is kept (the walking mode snaps it to the floor)
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const FVector Up = MoverComp->GetUpDirection();
	OutProposedMove.LinearVelocity = Sync ? Up * FVector::DotProduct(Sync->GetVelocity_WorldSpace(), Up) : FVector::ZeroVector;
	OutProposedMove.AngularVelocityDegrees = FVector::ZeroVector;
	return true;
}

FLayeredMoveBase* FGSTN_LayeredMove_LedgeGuard::Clone() const
{
	return new FGSTN_LayeredMove_LedgeGuard(*this);
}

void FGSTN_LayeredMove_LedgeGuard::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << TimelineStartFrame;
	uint8 DoneBit = bDone ? 1 : 0;
	Ar.SerializeBits(&DoneBit, 1);
	bDone = DoneBit != 0;
}

UScriptStruct* FGSTN_LayeredMove_LedgeGuard::GetScriptStruct() const
{
	return FGSTN_LayeredMove_LedgeGuard::StaticStruct();
}

FString FGSTN_LayeredMove_LedgeGuard::ToSimpleString() const
{
	return FString::Printf(TEXT("LedgeGuard (timeline start %d%s)"), TimelineStartFrame, bDone ? TEXT(", done") : TEXT(""));
}

FEffectMoveContainer UGSTN_LedgeGuardMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	// Empty: the move latches its timeline on its first frame
	FGSTN_LayeredMove_LedgeGuard Move;
	return FEffectMoveContainer(&Move);
}

UGSTN_GE_LedgeGuard::UGSTN_GE_LedgeGuard()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	UGSTN_LedgeGuardMoveCreator* Creator = CreateDefaultSubobject<UGSTN_LedgeGuardMoveCreator>(TEXT("LedgeGuardMoveCreator"));
	Creator->LayeredMoveDefaultParams.CopyDurationFromEffect = false;
	ApplyMove->MoveProcessor = Creator;
	GEComponents.Add(ApplyMove);
}
