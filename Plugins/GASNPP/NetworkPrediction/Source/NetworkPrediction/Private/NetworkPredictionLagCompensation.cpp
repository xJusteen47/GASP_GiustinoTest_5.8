// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkPredictionLagCompensation.h"

#include "NetworkPredictionWorldManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"


// Sets default values for this component's properties
UNetworkPredictionLagCompensation::UNetworkPredictionLagCompensation()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void UNetworkPredictionLagCompensation::OnRegister()
{
	Super::OnRegister();
	RegisterWithSubsystem();
}

void UNetworkPredictionLagCompensation::OnUnregister()
{
	UnregisterWithSubsystem();
	Super::OnUnregister();
}


void UNetworkPredictionLagCompensation::RegisterWithSubsystem()
{
	UNetworkPredictionWorldManager* NpManager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	if (NpManager)
	{
		NpManager->RegisterRewindableComponent(this);
	}
}


void UNetworkPredictionLagCompensation::UnregisterWithSubsystem()
{
	UNetworkPredictionWorldManager* NpManager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	if (NpManager)
	{
		NpManager->UnregisterRewindableComponent(this);
	}
}


bool UNetworkPredictionLagCompensation::HasSimulation() const
{
	return GetOwnerRole() != ROLE_SimulatedProxy;
}

void UNetworkPredictionLagCompensation::CaptureStateAndAddToHistory(const float& TimeStampMs)
{
	TSharedPtr<FNpLagCompensationData> State = GetLatestOrAddEntry(TimeStampMs);
	CaptureState(State);
	State->SimTimeMs = TimeStampMs;
	WriteToLatest(State);
}

void UNetworkPredictionLagCompensation::CaptureState(TSharedPtr<FNpLagCompensationData>& StateToFill)
{
	StateToFill->Location = GetOwner()->GetActorLocation();
	StateToFill->Rotation = GetOwner()->GetActorQuat();
	if (UPrimitiveComponent* Comp = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
	{
		StateToFill->CollisionExtent = Comp->GetCollisionShape().GetExtent();
	}
	else
	{
		StateToFill->CollisionExtent = GetOwner()->GetSimpleCollisionCylinderExtent();
	}
	StateToFill->CanRewindFurther = true;
}

// Called every frame
void UNetworkPredictionLagCompensation::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UNetworkPredictionLagCompensation::SetOwningActorState(const TSharedPtr<FNpLagCompensationData> TargetState)
{
	if (TargetState == nullptr)
	{
		return;
	}
	GetOwner()->SetActorLocation(TargetState->Location);
	GetOwner()->SetActorRotation(TargetState->Rotation);
	if (UPrimitiveComponent* OwnerCollision = GetOwner()->GetComponentByClass<UPrimitiveComponent>())
	{
		if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(OwnerCollision))
		{
			Capsule->SetCapsuleSize(TargetState->CollisionExtent.X, TargetState->CollisionExtent.Z);
			return;
		}
		if (UBoxComponent* BoxComp = Cast<UBoxComponent>(OwnerCollision))
		{
			BoxComp->SetBoxExtent(TargetState->CollisionExtent);
			return;
		}
		if (USphereComponent* SphereComp = Cast<USphereComponent>(OwnerCollision))
		{
			SphereComp->SetSphereRadius(TargetState->CollisionExtent.X);
		}
	}
	
}

void UNetworkPredictionLagCompensation::CapturePreRewindState()
{
	const int32 LastIndex = History.Num() - 1;
	History.PreRewindData = TSharedPtr<FNpLagCompensationData>(History.GetAt(LastIndex)->Clone());
}

void UNetworkPredictionLagCompensation::OnStartedRewind()
{
	History.bIsInRewind = true;
}

void UNetworkPredictionLagCompensation::OnEndedRewind()
{
	History.bIsInRewind = false;
}

TSharedPtr<FNpLagCompensationData> UNetworkPredictionLagCompensation::GetLatestOrAddEntry(const float& SimTimeMS)
{
	return History.GetLatestOrAddCopy(SimTimeMS);
}

void UNetworkPredictionLagCompensation::WriteToLatest(const TSharedPtr<FNpLagCompensationData>& StateToOverride)
{
	History.WriteToLatestState(StateToOverride);
}

void UNetworkPredictionLagCompensation::InitializeHistory(const int32& MaxSize)
{
	History = FNpLagCompensationHistory(RewindDataType, 128);
}

