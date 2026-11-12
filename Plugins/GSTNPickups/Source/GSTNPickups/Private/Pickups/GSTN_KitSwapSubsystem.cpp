// Equipment pickups (M11 G2).

#include "Pickups/GSTN_KitSwapSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_InputTags.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Pickups/GSTN_EquipmentPickup.h"
#include "Pickups/GSTN_KitPickup.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNKitSwap, Log, All);

UGSTN_KitSwapSubsystem* UGSTN_KitSwapSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UGSTN_KitSwapSubsystem>() : nullptr;
}

bool UGSTN_KitSwapSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_KitSwapSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// The event stream is empty on a client (UGSTN_SimEventSubsystem): listening there would only cost a listener
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(&InWorld);
	if (!Events)
	{
		return;
	}
	FGameplayTagContainer Required;
	Required.AddTag(GSTNInputTags::Event_CommandExecuted.GetTag());
	Required.AddTag(GSTNCommandTags::Interact_PickUp.GetTag());
	TWeakObjectPtr<UGSTN_KitSwapSubsystem> WeakThis(this);
	ListenerHandle = Events->ListenNative(FGameplayTagQuery::MakeQuery_MatchAllTags(Required), [WeakThis](const FGSTN_SimEvent& Event)
	{
		if (UGSTN_KitSwapSubsystem* Self = WeakThis.Get())
		{
			Self->HandleCommandExecuted(Event);
		}
	});
	EventSource = Events;
}

void UGSTN_KitSwapSubsystem::Deinitialize()
{
	if (UGSTN_SimEventSubsystem* Events = EventSource.Get())
	{
		Events->StopListening(ListenerHandle);
	}
	EventSource.Reset();
	ListenerHandle = FGSTN_SimEventListenerHandle();
	Super::Deinitialize();
}

void UGSTN_KitSwapSubsystem::HandleCommandExecuted(const FGSTN_SimEvent& Event)
{
	AActor* Pawn = Event.Instigator.Get();
	if (!bSwapOnInteractHoldEvent || !Pawn || !FindPickupFor(Pawn))
	{
		// A hold of interact away from any kit: the kit's own pick-up move (if any) is all that happens
		return;
	}
	TArray<FString> Errors;
	if (!TrySwap(Pawn, Errors) && Errors.Num() > 0)
	{
		if (UGSTN_LoadoutComponent* Component = FindLoadoutComponent(Pawn))
		{
			Component->ClientLoadoutRejected(Errors);
		}
	}
}

AGSTN_EquipmentPickup* UGSTN_KitSwapSubsystem::FindPickupFor(const AActor* Pawn) const
{
	const UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return nullptr;
	}
	// A level holds a handful of kits and this runs on a swap, not per tick: a scan is cheaper than keeping a registry right
	const FVector From = Pawn->GetActorLocation();
	AGSTN_EquipmentPickup* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (TActorIterator<AGSTN_EquipmentPickup> It(World); It; ++It)
	{
		AGSTN_EquipmentPickup* Pickup = *It;
		if (!IsValid(Pickup) || Pickup->IsEmpty())
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(From, Pickup->GetActorLocation());
		if (DistSq <= FMath::Square(static_cast<double>(Pickup->InteractRadius)) && DistSq < BestDistSq)
		{
			Best = Pickup;
			BestDistSq = DistSq;
		}
	}
	return Best;
}

bool UGSTN_KitSwapSubsystem::TrySwap(AActor* Pawn, TArray<FString>& OutErrors)
{
	OutErrors.Reset();
	if (!Pawn || !Pawn->HasAuthority())
	{
		OutErrors.Add(TEXT("Kit swap: only the server swaps kits"));
		return false;
	}
	AGSTN_EquipmentPickup* Pickup = FindPickupFor(Pawn);
	if (!Pickup)
	{
		OutErrors.Add(TEXT("Kit swap: no kit in reach"));
		return false;
	}
	UGSTN_LoadoutComponent* Component = FindLoadoutComponent(Pawn);
	if (!Component)
	{
		OutErrors.Add(FString::Printf(TEXT("Kit swap: %s has no loadout component"), *Pawn->GetName()));
		return false;
	}
	const FPrimaryAssetId OfferedKit = Pickup->GetDisplayId();
	const bool bSwapped = SwapWithPickup(*Component, *Pickup, OutErrors);
	UE_LOG(LogGSTNKitSwap, Log, TEXT("%s %s kit %s at %s"), *Pawn->GetName(), bSwapped ? TEXT("took") : TEXT("was refused"),
		*OfferedKit.ToString(), *Pickup->GetName());
	return bSwapped;
}

bool UGSTN_KitSwapSubsystem::SwapWithPickup(UGSTN_LoadoutComponent& Component, AGSTN_EquipmentPickup& Pickup, TArray<FString>& OutErrors)
{
	return Pickup.Take(Component, OutErrors);
}

int32 UGSTN_KitSwapSubsystem::GetRacePlayerId(const AActor* Pawn)
{
	const APawn* AsPawn = Cast<APawn>(Pawn);
	const APlayerState* PlayerState = AsPawn ? AsPawn->GetPlayerState() : nullptr;
	return PlayerState ? PlayerState->GetPlayerId() : INDEX_NONE;
}

AGSTN_KitPickup* UGSTN_KitSwapSubsystem::DropKit(UGSTN_LoadoutComponent* From, const FTransform& Where, TSubclassOf<AGSTN_KitPickup> PickupClass)
{
	UWorld* World = GetWorld();
	if (!From || !From->HasLoadout() || !World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* Class = PickupClass ? PickupClass.Get() : AGSTN_KitPickup::StaticClass();
	AGSTN_KitPickup* Pickup = World->SpawnActor<AGSTN_KitPickup>(Class, Where, Params);
	if (Pickup)
	{
		Pickup->SetLoadout(From->GetApplied());
	}
	return Pickup;
}

UGSTN_LoadoutComponent* UGSTN_KitSwapSubsystem::FindLoadoutComponent(const AActor* Pawn)
{
	if (!Pawn)
	{
		return nullptr;
	}
	if (const APawn* AsPawn = Cast<APawn>(Pawn))
	{
		if (const APlayerState* PlayerState = AsPawn->GetPlayerState())
		{
			if (UGSTN_LoadoutComponent* Component = PlayerState->FindComponentByClass<UGSTN_LoadoutComponent>())
			{
				return Component;
			}
		}
	}
	return Pawn->FindComponentByClass<UGSTN_LoadoutComponent>();
}
