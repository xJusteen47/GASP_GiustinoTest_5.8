// Held objects, world side (needs the GAS-NPP fork). M11 K1: the kit on the ground as an interact target.

#include "GSTN_KitInteract.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GSTN_CombatTags.h"
#include "GSTN_HeldSettings.h"
#include "GSTN_HeldSubsystem.h"
#include "GSTN_HeldTags.h"
#include "GSTN_InteractRegistry.h"
#include "GSTN_InteractSubsystem.h"
#include "GSTN_ProjectileSubsystem.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "NetworkPredictionWorldManager.h"
#include "Pickups/GSTN_KitPickup.h"
#include "Pickups/GSTN_KitSwapSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNKitInteract, Log, All);

// ---------------------------------------------------------------------------------------------------------------------
// Record component
// ---------------------------------------------------------------------------------------------------------------------

UGSTN_KitInteractableComponent::UGSTN_KitInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
	RadiusCm = 30.f;
}

void UGSTN_KitInteractableComponent::PostInitProperties()
{
	Super::PostInitProperties();
	// Here, not in BeginPlay: a client may build the record from the replicated stamp before the component begins play, and the record
	// takes the meaning and reach from these fields. Same values on every machine (class defaults and the pickup's radius)
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		Meaning = GSTNHeldTags::Interact_Target_Kit;
		if (const AGSTN_KitPickup* Pickup = GetTypedOuter<AGSTN_KitPickup>())
		{
			ReachOverrideCm = Pickup->InteractRadius;
		}
	}
}

AGSTN_KitPickup* UGSTN_KitInteractableComponent::GetPickup() const
{
	return Cast<AGSTN_KitPickup>(GetOwner());
}

void UGSTN_KitInteractableComponent::WithSimFrame(TFunctionRef<void()> Call)
{
	UGSTN_InteractSubsystem* Interact = UGSTN_InteractSubsystem::Get(GetWorld());
	const UGSTN_KitInteractSubsystem* Kits = UGSTN_KitInteractSubsystem::Get(this);
	if (!Interact || !Kits)
	{
		Call();
		return;
	}
	// The stamps must be in the frames GSTNLink's probe queries with (the combat simulation's), as the held objects' records are: GSTNInteract's
	// own clock (server time x 62.5) runs ahead of them, which would keep a new kit unframeable for as long as the gap
	Interact->SetFrameOverride(FMath::Max(Kits->GetSimFrame() + 1, 0));
	Call();
	Interact->ClearFrameOverride();
}

void UGSTN_KitInteractableComponent::BeginPlay()
{
	if (const AGSTN_KitPickup* Pickup = GetPickup())
	{
		DisplayName = FText::FromName(Pickup->GetLoadout().PrimaryKit.PrimaryAssetName);
	}
	WithSimFrame([this]() { UGSTN_InteractableComponent::BeginPlay(); });
}

void UGSTN_KitInteractableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Prompt)
	{
		Prompt->DestroyComponent();
		Prompt = nullptr;
	}
	WithSimFrame([this, EndPlayReason]() { UGSTN_InteractableComponent::EndPlay(EndPlayReason); });
}

void UGSTN_KitInteractableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdatePrompt();
}

void UGSTN_KitInteractableComponent::UpdatePrompt()
{
	AGSTN_KitPickup* Pickup = GetPickup();
	UWorld* World = GetWorld();
	if (!Pickup || !World || Pickup->IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	bool bShown = false;
	const APlayerController* Player = World->GetFirstPlayerController();
	const APawn* Pawn = Player && Player->IsLocalController() ? Player->GetPawn() : nullptr;
	const UAbilitySystemComponent* Abilities = Pawn ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr;
	if (UGSTN_HeldSettings::Get().KitSwap.bShowPrompt && !Pickup->IsEmpty() && Abilities && Abilities->HasMatchingGameplayTag(GSTNHeldTags::Interact_Target_Kit))
	{
		const UGSTN_KitInteractSubsystem* Kits = UGSTN_KitInteractSubsystem::Get(this);
		bShown = Kits && Kits->FindSwapTarget(Pawn, Kits->GetSimFrame()) == Pickup;
	}
	if (bShown && !Prompt)
	{
		Prompt = NewObject<UTextRenderComponent>(Pickup, TEXT("KitPrompt"), RF_Transient);
		Prompt->SetupAttachment(Pickup->GetRootComponent());
		Prompt->SetHorizontalAlignment(EHTA_Center);
		Prompt->SetWorldSize(32.f);
		Prompt->SetTextRenderColor(FColor::Yellow);
		Prompt->SetRelativeLocation(FVector(0.f, 0.f, 130.f));
		Prompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Prompt->RegisterComponent();
	}
	if (Prompt)
	{
		Prompt->SetVisibility(bShown);
		if (bShown)
		{
			// Placeholder text (no visual polish until the owner asks): the button and the kit that would come to hand
			Prompt->SetText(FText::FromString(FString::Printf(TEXT("Tieni B: %s"), *Pickup->GetLoadout().PrimaryKit.PrimaryAssetName.ToString())));
			if (const APlayerCameraManager* Camera = Player->PlayerCameraManager)
			{
				const FVector ToCamera = Camera->GetCameraLocation() - Prompt->GetComponentLocation();
				Prompt->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Ability
// ---------------------------------------------------------------------------------------------------------------------

void UGSTN_GA_KitSwap::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// Host side effect only (the host never resimulates, so one request per activation)
	const UWorld* World = GetWorld();
	if (UGSTN_KitInteractSubsystem* Kits = UGSTN_KitInteractSubsystem::Get(World); Kits && World->GetNetMode() != NM_Client)
	{
		Kits->RequestSwap(GetAvatarActorFromActorInfo());
	}
	if (IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Subsystem
// ---------------------------------------------------------------------------------------------------------------------

UGSTN_KitInteractSubsystem* UGSTN_KitInteractSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UGSTN_KitInteractSubsystem>() : nullptr;
}

bool UGSTN_KitInteractSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UGSTN_KitInteractSubsystem::HasAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

void UGSTN_KitInteractSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UNetworkPredictionWorldManager* Manager = Collection.InitializeDependency<UNetworkPredictionWorldManager>())
	{
		PostFixedTickHandle = Manager->OnPostFixedTick.AddUObject(this, &UGSTN_KitInteractSubsystem::HandlePostFixedTick);
	}
	Collection.InitializeDependency<UGSTN_HeldSubsystem>();
	// The authoritative event stream (KO): only the host publishes, so a client's listener never fires
	if (UGSTN_SimEventSubsystem* Events = Collection.InitializeDependency<UGSTN_SimEventSubsystem>())
	{
		FGameplayTagContainer KO;
		KO.AddTag(GSTNCombatTags::Event_KO);
		TWeakObjectPtr<UGSTN_KitInteractSubsystem> WeakThis(this);
		EventListener = Events->ListenNative(FGameplayTagQuery::MakeQuery_MatchAnyTags(KO), [WeakThis](const FGSTN_SimEvent& Event)
		{
			if (UGSTN_KitInteractSubsystem* This = WeakThis.Get())
			{
				This->HandleSimEvent(Event);
			}
		});
	}
}

void UGSTN_KitInteractSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!UGSTN_HeldSettings::Get().KitSwap.bEnabled)
	{
		return;
	}
	// One trigger per hold: the TH-1b hold-near-a-kit path would swap the nearest kit on a shorter release of the same button
	if (UGSTN_KitSwapSubsystem* Legacy = UGSTN_KitSwapSubsystem::Get(&InWorld))
	{
		Legacy->bSwapOnInteractHoldEvent = false;
	}
	for (TActorIterator<AGSTN_KitPickup> It(&InWorld); It; ++It)
	{
		EnsureRecord(*It);
	}
	if (HasAuthority())
	{
		ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UGSTN_KitInteractSubsystem::HandleActorSpawned));
	}
}

void UGSTN_KitInteractSubsystem::Deinitialize()
{
	UWorld* World = GetWorld();
	if (PostFixedTickHandle.IsValid())
	{
		if (UNetworkPredictionWorldManager* Manager = World ? World->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr)
		{
			Manager->OnPostFixedTick.Remove(PostFixedTickHandle);
		}
		PostFixedTickHandle.Reset();
	}
	if (EventListener.IsValid())
	{
		if (UGSTN_SimEventSubsystem* Events = World ? World->GetSubsystem<UGSTN_SimEventSubsystem>() : nullptr)
		{
			Events->StopListening(EventListener);
		}
		EventListener = FGSTN_SimEventListenerHandle();
	}
	if (World && ActorSpawnedHandle.IsValid())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
		ActorSpawnedHandle.Reset();
	}
	Pending.Reset();
	Super::Deinitialize();
}

int32 UGSTN_KitInteractSubsystem::GetSimFrame() const
{
	const UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(GetWorld());
	return Held ? Held->GetLastFrame() : INDEX_NONE;
}

void UGSTN_KitInteractSubsystem::HandleActorSpawned(AActor* Actor)
{
	if (AGSTN_KitPickup* Pickup = Cast<AGSTN_KitPickup>(Actor))
	{
		EnsureRecord(Pickup);
	}
}

void UGSTN_KitInteractSubsystem::EnsureRecord(AGSTN_KitPickup* Pickup) const
{
	if (!IsValid(Pickup) || !UGSTN_HeldSettings::Get().KitSwap.bEnabled || Pickup->FindComponentByClass<UGSTN_KitInteractableComponent>())
	{
		return;
	}
	const bool bLevelPlaced = Pickup->IsNetStartupActor();
	// A client's copy of a spawned kit gets the server's component (and its stamp) by replication
	if (!bLevelPlaced && !HasAuthority())
	{
		return;
	}
	UGSTN_KitInteractableComponent* Record = NewObject<UGSTN_KitInteractableComponent>(Pickup, TEXT("KitInteract"));
	// Level-placed: every machine adds its own and derives the same handle from the actor's path, nothing replicates
	Record->SetIsReplicated(!bLevelPlaced);
	Record->RegisterComponent();
}

void UGSTN_KitInteractSubsystem::RequestSwap(AActor* Holder)
{
	if (Holder && HasAuthority())
	{
		// The request belongs to the frame being simulated now: the post tick of that frame resolves it
		Pending.Add({ Holder, GetSimFrame() + 1 });
	}
}

AGSTN_KitPickup* UGSTN_KitInteractSubsystem::FindSwapTarget(const AActor* Holder, int32 Frame, bool* bOutFocused) const
{
	if (bOutFocused)
	{
		*bOutFocused = false;
	}
	UWorld* World = GetWorld();
	if (!Holder || !World)
	{
		return nullptr;
	}
	const FGSTN_KitSwapTuning& Tuning = UGSTN_HeldSettings::Get().KitSwap;
	if (Tuning.bTakesFocusedKit)
	{
		const UGSTN_InteractSubsystem* Interact = UGSTN_InteractSubsystem::Get(World);
		const UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(World);
		const int32 EntityId = Projectiles ? Projectiles->FindEntityIdOf(Holder) : INDEX_NONE;
		FGSTN_InteractCandidateId Focus;
		if (Interact && EntityId != INDEX_NONE && Interact->FindProbeFocus(EntityId, Frame, Focus) && Focus.Provider == GSTNInteractProviders::Registry)
		{
			for (TActorIterator<AGSTN_KitPickup> It(World); It; ++It)
			{
				const UGSTN_KitInteractableComponent* Record = It->FindComponentByClass<UGSTN_KitInteractableComponent>();
				if (Record && Record->GetCandidateId() == Focus && !It->IsEmpty())
				{
					if (bOutFocused)
					{
						*bOutFocused = true;
					}
					return *It;
				}
			}
		}
		if (!Tuning.bFocusMissFallsBackToNearest)
		{
			return nullptr;
		}
	}
	const UGSTN_KitSwapSubsystem* Swaps = UGSTN_KitSwapSubsystem::Get(World);
	return Swaps ? Swaps->FindPickupFor(Holder) : nullptr;
}

void UGSTN_KitInteractSubsystem::HandlePostFixedTick(int32 ServerFrame, int32 EndTimeMS, bool bResimulating)
{
	if (bResimulating || !HasAuthority() || Pending.IsEmpty())
	{
		return;
	}
	// Kits already swapped this pass: a second player on the same kit and frame would otherwise take the first player's old kit
	TSet<const AGSTN_KitPickup*> Taken;
	for (const FQueuedSwap& Queued : Pending)
	{
		AActor* Holder = Queued.Holder.Get();
		if (!Holder || Queued.Frame > ServerFrame)
		{
			continue;
		}
		bool bFocused = false;
		AGSTN_KitPickup* Pickup = FindSwapTarget(Holder, Queued.Frame, &bFocused);
		UGSTN_LoadoutComponent* Component = UGSTN_KitSwapSubsystem::FindLoadoutComponent(Holder);
		if (!Pickup || Taken.Contains(Pickup) || !Component)
		{
			UE_LOG(LogGSTNKitInteract, Log, TEXT("KitSwap %s frame=%d: %s"), *Holder->GetName(), Queued.Frame,
				!Component ? TEXT("no loadout component") : Pickup ? TEXT("kit already taken this frame") : TEXT("no kit in reach"));
			continue;
		}
		const FPrimaryAssetId Offered = Pickup->GetLoadout().PrimaryKit;
		const FPrimaryAssetId Previous = Component->GetApplied().PrimaryKit;
		TArray<FString> Errors;
		const bool bSwapped = UGSTN_KitSwapSubsystem::SwapWithPickup(*Component, *Pickup, Errors);
		UE_LOG(LogGSTNKitInteract, Log, TEXT("KitSwap %s frame=%d %s kit=%s left=%s at %s (%s)"), *Holder->GetName(), Queued.Frame,
			bSwapped ? TEXT("took") : TEXT("refused"), *Offered.ToString(), *Previous.ToString(), *Pickup->GetName(), bFocused ? TEXT("focus") : TEXT("nearest"));
		if (bSwapped)
		{
			Taken.Add(Pickup);
		}
		else if (Errors.Num() > 0)
		{
			Component->ClientLoadoutRejected(Errors);
		}
	}
	Pending.RemoveAll([ServerFrame](const FQueuedSwap& Queued) { return Queued.Frame <= ServerFrame || !Queued.Holder.IsValid(); });
}

void UGSTN_KitInteractSubsystem::HandleSimEvent(const FGSTN_SimEvent& Event)
{
	if (Event.EventType.MatchesTagExact(GSTNCombatTags::Event_KO) && HasAuthority())
	{
		DropKitOnKO(Event.Target.Get());
	}
}

AGSTN_KitPickup* UGSTN_KitInteractSubsystem::DropKitOnKO(AActor* Victim)
{
	const FGSTN_KitSwapTuning& Tuning = UGSTN_HeldSettings::Get().KitSwap;
	const APawn* Pawn = Cast<APawn>(Victim);
	const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	UWorld* World = GetWorld();
	if (Tuning.DeathDrop == EGSTN_KitDeathDrop::Off || !PlayerState || PlayerState->IsABot() || !World || !HasAuthority())
	{
		return nullptr;
	}
	UGSTN_LoadoutComponent* Component = UGSTN_KitSwapSubsystem::FindLoadoutComponent(Pawn);
	UGSTN_KitSwapSubsystem* Swaps = UGSTN_KitSwapSubsystem::Get(World);
	if (!Component || !Component->HasLoadout() || !Swaps)
	{
		return nullptr;
	}
	// On the floor under the player (a KO in the air drops the kit where the player would land), else where the player is
	FVector Where = Pawn->GetActorLocation();
	FHitResult Floor;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNKitDrop), false, Pawn);
	if (World->LineTraceSingleByChannel(Floor, Where, Where - FVector(0.0, 0.0, 2000.0), ECC_WorldStatic, Params))
	{
		Where = Floor.ImpactPoint + FVector(0.0, 0.0, 20.0);
	}
	const FPrimaryAssetId Dropped = Component->GetApplied().PrimaryKit;
	AGSTN_KitPickup* Pickup = Swaps->DropKit(Component, FTransform(FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f), Where), nullptr);
	if (!Pickup)
	{
		return nullptr;
	}
	if (Tuning.DroppedKitLifetimeSeconds > 0.f)
	{
		Pickup->SetLifeSpan(Tuning.DroppedKitLifetimeSeconds);
	}
	FString Kept = TEXT("kept");
	if (Tuning.DeathDrop == EGSTN_KitDeathDrop::DropAndLose && Tuning.RespawnKit.IsValid())
	{
		FGSTN_LoadoutData Respawn;
		Respawn.PrimaryKit = Tuning.RespawnKit;
		TArray<FString> Errors;
		Kept = Component->AcceptLoadout(Respawn, Errors) ? FString::Printf(TEXT("now %s"), *Tuning.RespawnKit.ToString())
			: FString::Printf(TEXT("kept (respawn kit refused: %s)"), Errors.Num() > 0 ? *Errors[0] : TEXT("?"));
	}
	UE_LOG(LogGSTNKitInteract, Log, TEXT("KitDrop %s KO dropped=%s player %s at %s"), *Pawn->GetName(), *Dropped.ToString(), *Kept, *Pickup->GetName());
	return Pickup;
}
