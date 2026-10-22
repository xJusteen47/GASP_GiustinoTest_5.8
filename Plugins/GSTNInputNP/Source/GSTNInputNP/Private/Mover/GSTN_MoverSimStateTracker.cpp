// Mover state tracking for simulation code (GAS-NPP fork).

#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverComponent.h"
#include "GameFramework/Actor.h"

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

void UGSTN_MoverSimStateTracker::HandlePostMovement(const FMoverTimeStep& TimeStep, FMoverSyncState& InSyncState, FMoverAuxStateContext& InAuxState)
{
	// End state of the step that was just simulated (re-simulations included)
	SyncState = InSyncState;
}

void UGSTN_MoverSimStateTracker::HandlePostSimulationRollback(const FMoverTimeStep& CurrentTimeStep, const FMoverTimeStep& ExpungedTimeStep)
{
	// The Mover component has just taken the restored state: the next step re-simulates from it
	SyncState = Mover->GetSyncState();
}
