// Held objects, world side.

#include "GSTN_HeldObjectComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GSTN_HeldSettings.h"
#include "GSTN_HeldSubsystem.h"
#include "GSTN_HeldTags.h"
#include "GSTN_ProjectileActor.h"
#include "Net/UnrealNetwork.h"

UGSTN_HeldObjectComponent::UGSTN_HeldObjectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the holder moved this frame, so a held object does not trail by one frame
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(true);
}

const FGSTN_HeldObjectTuning& UGSTN_HeldObjectComponent::GetTuning() const
{
	static const FGSTN_HeldObjectTuning TestTuning;
	return Definition ? Definition->Tuning : TestTuning;
}

const FGSTN_HeldBreakRules& UGSTN_HeldObjectComponent::GetBreakRules() const
{
	const FGSTN_HeldObjectTuning& Tuning = GetTuning();
	return Tuning.bOverrideBreakRules ? Tuning.BreakRules : UGSTN_HeldSettings::Get().DefaultBreakRules;
}

TSubclassOf<UGameplayAbility> UGSTN_HeldObjectComponent::FindCommandAction(const FGameplayTag& Command) const
{
	const TSubclassOf<UGameplayAbility>* Found = Definition ? Definition->CommandActions.Find(Command) : nullptr;
	return Found ? *Found : nullptr;
}

void UGSTN_HeldObjectComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGSTN_HeldObjectComponent, State);
	DOREPLIFETIME(UGSTN_HeldObjectComponent, Holder);
	DOREPLIFETIME(UGSTN_HeldObjectComponent, Flight);
	DOREPLIFETIME(UGSTN_HeldObjectComponent, RestLocation);
}

void UGSTN_HeldObjectComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			RestLocation = Owner->GetActorLocation();
		}
		// Placement is computed on every machine; the engine's movement replication would fight it
		Owner->SetReplicatingMovement(false);
	}
	if (UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld()))
	{
		Held->RegisterObject(*this);
	}
}

void UGSTN_HeldObjectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld()))
	{
		Held->UnregisterObject(*this);
	}
	Super::EndPlay(EndPlayReason);
}

void UGSTN_HeldObjectComponent::SetHeld(AActor* InHolder)
{
	State = EGSTN_HeldObjectState::Held;
	Holder = InHolder;
	++HoldSerial;
	Flight = nullptr;
	FlightEndFrame = INDEX_NONE;
	GetOwner()->ForceNetUpdate();
	UpdatePlacement();
}

void UGSTN_HeldObjectComponent::SetFlying(AGSTN_Projectile& InFlight)
{
	State = EGSTN_HeldObjectState::Flying;
	Holder = nullptr;
	Flight = &InFlight;
	FlightSegment = InFlight.GetSegment();
	FlightEndFrame = FlightSegment.StartFrame + InFlight.GetLifetimeFrames();
	GetOwner()->ForceNetUpdate();
	UpdatePlacement();
}

void UGSTN_HeldObjectComponent::SetIdleAt(const FVector& Location)
{
	State = EGSTN_HeldObjectState::Idle;
	Holder = nullptr;
	Flight = nullptr;
	FlightEndFrame = INDEX_NONE;
	RestLocation = Location;
	GetOwner()->ForceNetUpdate();
	UpdatePlacement();
}

void UGSTN_HeldObjectComponent::OnRep_RestLocation()
{
	UpdatePlacement();
}

void UGSTN_HeldObjectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (State != EGSTN_HeldObjectState::Idle)
	{
		UpdatePlacement();
	}
	UpdatePrompt();
}

void UGSTN_HeldObjectComponent::UpdatePrompt()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || Owner->IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	bool bShown = false;
	const APlayerController* Player = World->GetFirstPlayerController();
	const APawn* Pawn = Player && Player->IsLocalController() ? Player->GetPawn() : nullptr;
	const UAbilitySystemComponent* Abilities = Pawn ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr;
	if (State == EGSTN_HeldObjectState::Idle && Abilities && Abilities->HasMatchingGameplayTag(GSTNHeldTags::Interact_Target_Pickup))
	{
		const UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(World);
		bShown = Held && Held->FindPickupTarget(Pawn->GetActorLocation()) == this;
	}
	if (bShown && !Prompt)
	{
		Prompt = NewObject<UTextRenderComponent>(Owner, TEXT("HeldPrompt"), RF_Transient);
		Prompt->SetupAttachment(Owner->GetRootComponent());
		Prompt->SetText(FText::FromString(TEXT("B")));
		Prompt->SetHorizontalAlignment(EHTA_Center);
		Prompt->SetWorldSize(40.f);
		Prompt->SetTextRenderColor(FColor::White);
		Prompt->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
		Prompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Prompt->RegisterComponent();
	}
	if (Prompt)
	{
		Prompt->SetVisibility(bShown);
		if (bShown)
		{
			// Faces the camera of the local player
			if (const APlayerCameraManager* Camera = Player->PlayerCameraManager)
			{
				const FVector ToCamera = Camera->GetCameraLocation() - Prompt->GetComponentLocation();
				Prompt->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
			}
		}
	}
}

void UGSTN_HeldObjectComponent::UpdatePlacement()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	switch (State)
	{
	case EGSTN_HeldObjectState::Held:
		if (IsValid(Holder))
		{
			const FTransform Hold = FGSTN_HeldRules::HoldTransform(Holder->GetActorTransform(), GetTuning().HoldOffset);
			Owner->SetActorLocationAndRotation(Hold.GetLocation(), Hold.GetRotation());
		}
		break;
	case EGSTN_HeldObjectState::Flying:
		// The projectile places itself at its frame; until a client has it, the object stays where it was (at the thrower)
		if (IsValid(Flight))
		{
			Owner->SetActorLocation(Flight->GetActorLocation());
		}
		break;
	default:
		Owner->SetActorLocation(RestLocation);
		break;
	}
}
