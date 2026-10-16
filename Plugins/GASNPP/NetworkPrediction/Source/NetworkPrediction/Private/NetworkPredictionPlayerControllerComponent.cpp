// 2025 Yohoho Productions /  Sirkai


#include "NetworkPredictionPlayerControllerComponent.h"

#include "NetworkPredictionWorldManager.h"


// Sets default values for this component's properties
UNetworkPredictionPlayerControllerComponent::UNetworkPredictionPlayerControllerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	// ...
}


// Called when the game starts
void UNetworkPredictionPlayerControllerComponent::BeginPlay()
{
	Super::BeginPlay();
	// ...
	
}

void UNetworkPredictionPlayerControllerComponent::OnRegister()
{
	Super::OnRegister();

	UNetworkPredictionWorldManager* Manager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());
	if (OwningPlayerController && Manager)
	{
		Manager->RegisterRPCHandler(this);
	}
}

void UNetworkPredictionPlayerControllerComponent::OnUnregister()
{
	Super::OnUnregister();
	UNetworkPredictionWorldManager* Manager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());
	if (OwningPlayerController && Manager)
	{
		Manager->UnRegisterRPCHandler(this);
	}
}


// Called every frame
void UNetworkPredictionPlayerControllerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                                 FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// ...
}

void UNetworkPredictionPlayerControllerComponent::GetLifetimeReplicatedProps(
	TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNetworkPredictionPlayerControllerComponent, TimeDilation);
}

void UNetworkPredictionPlayerControllerComponent::SendServerRpc(const int32& Frame)
{
	Server_ReceivedInput(Frame,InterpolationTimeMS,InputsToSend);
	InputsToSend.Reset();
}

void UNetworkPredictionPlayerControllerComponent::SendAckedFrames(const FSerializedAckedFrames& AckedFrames)
{
	Server_ReceivedAckedFrames(AckedFrames);
}

void UNetworkPredictionPlayerControllerComponent::Server_ReceivedAckedFrames_Implementation(
	const FSerializedAckedFrames& AckedFrames)
{
	UNetworkPredictionWorldManager* Manager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	if (Manager)
	{
		Manager->OnReceivedAckedData(AckedFrames,this);
	}
}

void UNetworkPredictionPlayerControllerComponent::Server_ReceivedInput_Implementation(const int32& Frame,const float& InInterpolationTime,
                                                                                      const TArray<FSimulationReplicatedInput>& Inputs)
{
	UNetworkPredictionWorldManager* Manager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();

	/*for (const FSimulationReplicatedInput& Input : Inputs)
	{
		if (TFunction<void(
		const int32&, const float&, const FSimulationReplicatedInput&,
		UNetworkPredictionPlayerControllerComponent*, const FFixedTickState& TickState)>* Receiver = InputReceivers.BoundReceivers.Find(Input.ID))
		{
			(*Receiver)(Frame, InterpolationTime, Input,Manager->GetFixedTickState());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("No handler registered for Input ID %d"), Input.ID);
		}
	}*/
	if (Manager)
	{
		Manager->OnInputReceived(Frame,InInterpolationTime,Inputs,this);
	}
}


UNetConnection* UNetworkPredictionPlayerControllerComponent::GetNetConnection() const
{
	if (APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner()))
	{
		return OwningPlayerController->NetConnection;
	}
	return nullptr;
}

void UNetworkPredictionPlayerControllerComponent::UpdateTimeDilation(const float& InTimeDilation)
{
	if (GetOwner()->GetLocalRole() != ROLE_Authority)
	{
		return;
	}
	TimeDilation.UpdateTimeDilation(InTimeDilation);
}

void UNetworkPredictionPlayerControllerComponent::OnRep_TimeDilation()
{
	if (GetOwner()->GetLocalRole() == ROLE_Authority)
	{
		return;
	}
	UNetworkPredictionWorldManager* Manager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	if (Manager)
	{
		Manager->SetTimeDilation(TimeDilation);
	}
}


void UNetworkPredictionPlayerControllerComponent::AdvanceLastConsumedFrame(const int32& MaxBufferSize)
{
	if (LastReceivedFrame == INDEX_NONE)
	{
		return;
	}
	if (LastConsumedFrame >= LastReceivedFrame)
	{
		//ToDo Log input starvation.
		LastConsumedFrame = FMath::Max(LastReceivedFrame - 2,0);
		return;
	}
	if (LastReceivedFrame - LastConsumedFrame > FMath::Max(2,MaxBufferSize))
	{
		//ToDo Log Buffer Overflow
		LastConsumedFrame = LastReceivedFrame - 7;
		return;
	}
	LastConsumedFrame++;
}

void UNetworkPredictionPlayerControllerComponent::AddInputToSend(const int32& ID, const uint32& DataSize,
	const TArray<uint8>& Data)
{
	InputsToSend.Add(FSimulationReplicatedInput(ID, DataSize,Data));
}

