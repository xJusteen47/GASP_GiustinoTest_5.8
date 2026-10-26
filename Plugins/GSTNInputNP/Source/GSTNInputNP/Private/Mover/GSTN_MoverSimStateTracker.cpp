// Mover state tracking for simulation code (GAS-NPP fork).

#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/LaunchMove.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "GameFramework/Actor.h"

namespace
{
	/** 1/4096 steps: the same gravity gives the same bits on every machine, and a tilt of a fraction of a degree is still kept */
	FVector QuantizeDirection(const FVector& Direction)
	{
		constexpr double Steps = 4096.0;
		const FVector Unit = Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		return FVector(FMath::RoundToDouble(Unit.X * Steps) / Steps, FMath::RoundToDouble(Unit.Y * Steps) / Steps, FMath::RoundToDouble(Unit.Z * Steps) / Steps);
	}
}

UGSTN_MoverSimStateTracker::UGSTN_MoverSimStateTracker()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

UGSTN_MoverSimStateTracker* UGSTN_MoverSimStateTracker::FindOrAdd(UMoverComponent* MoverComponent)
{
	AActor* Owner = MoverComponent ? MoverComponent->GetOwner() : nullptr;
	if (!Owner)
	{
		return nullptr;
	}

	UGSTN_MoverSimStateTracker* Tracker = Owner->FindComponentByClass<UGSTN_MoverSimStateTracker>();
	if (!Tracker)
	{
		Tracker = NewObject<UGSTN_MoverSimStateTracker>(Owner, TEXT("GSTN_MoverSimStateTracker"), RF_Transient);
		Tracker->RegisterComponent();
	}
	if (Tracker->Mover != MoverComponent)
	{
		Tracker->Bind(MoverComponent);
	}
	return Tracker;
}

void UGSTN_MoverSimStateTracker::Bind(UMoverComponent* MoverComponent)
{
	if (Mover)
	{
		Mover->OnPostMovement.RemoveDynamic(this, &UGSTN_MoverSimStateTracker::HandlePostMovement);
		Mover->OnPostSimulationRollback.RemoveDynamic(this, &UGSTN_MoverSimStateTracker::HandlePostSimulationRollback);
	}

	Mover = MoverComponent;
	SyncState = Mover->GetSyncState();
	UpDirection = QuantizeDirection(Mover->GetUpDirection());
	bExternallyLaunched = false;
	Mover->OnPostMovement.AddDynamic(this, &UGSTN_MoverSimStateTracker::HandlePostMovement);
	Mover->OnPostSimulationRollback.AddDynamic(this, &UGSTN_MoverSimStateTracker::HandlePostSimulationRollback);
}

void UGSTN_MoverSimStateTracker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Mover)
	{
		Mover->OnPostMovement.RemoveDynamic(this, &UGSTN_MoverSimStateTracker::HandlePostMovement);
		Mover->OnPostSimulationRollback.RemoveDynamic(this, &UGSTN_MoverSimStateTracker::HandlePostSimulationRollback);
		Mover = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool UGSTN_MoverSimStateTracker::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	return Mover && Mover->HasGameplayTagInState(SyncState, TagToFind, bExactMatch);
}

FVector UGSTN_MoverSimStateTracker::GetVelocity() const
{
	const FMoverDefaultSyncState* Default = SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	return Default ? Default->GetVelocity_WorldSpace() : FVector::ZeroVector;
}

void UGSTN_MoverSimStateTracker::CaptureFrameFacts(const FMoverSyncState& InSyncState, double StepBaseTimeMs)
{
	UpDirection = QuantizeDirection(Mover->GetUpDirection());

	// A finished one-tick move stays in the active list until the next step flushes it, so a launch that started in the step
	// that produced this state (and not one that started earlier and is still running) is still there to see
	bExternallyLaunched = false;
	for (const TSharedPtr<FLayeredMoveBase>& Move : InSyncState.LayeredMoves.GetActiveMoves())
	{
		if (Move.IsValid() && Move->StartSimTimeMs >= StepBaseTimeMs && Move->GetScriptStruct()->IsChildOf(FLayeredMove_Launch::StaticStruct()))
		{
			bExternallyLaunched = true;
			return;
		}
	}
}

void UGSTN_MoverSimStateTracker::HandlePostMovement(const FMoverTimeStep& TimeStep, FMoverSyncState& InSyncState, FMoverAuxStateContext& InAuxState)
{
	// End state of the step that was just simulated (re-simulations included)
	SyncState = InSyncState;
	CaptureFrameFacts(InSyncState, TimeStep.BaseSimTimeMs);
}

void UGSTN_MoverSimStateTracker::HandlePostSimulationRollback(const FMoverTimeStep& CurrentTimeStep, const FMoverTimeStep& ExpungedTimeStep)
{
	// The Mover component has just taken the restored state: the next step re-simulates from it. The step that produced it began
	// one step before the base time of the next one
	SyncState = Mover->GetSyncState();
	CaptureFrameFacts(SyncState, CurrentTimeStep.BaseSimTimeMs - CurrentTimeStep.StepMs);
}
