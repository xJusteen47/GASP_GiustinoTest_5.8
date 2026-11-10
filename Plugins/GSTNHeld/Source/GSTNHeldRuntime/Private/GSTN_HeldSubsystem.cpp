// Held objects, world side (needs the GAS-NPP fork for the fixed tick it reads).

#include "GSTN_HeldSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GSTN_HeldAbilities.h"
#include "GSTN_HeldObjectActor.h"
#include "GSTN_HeldObjectComponent.h"
#include "GSTN_HeldSim.h"
#include "GSTN_HeldTags.h"
#include "GSTN_InteractSubsystem.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "GSTN_ProjectileActor.h"
#include "GSTN_ProjectileSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "NetworkPredictionWorldManager.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

UGSTN_HeldSubsystem* UGSTN_HeldSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UGSTN_HeldSubsystem>() : nullptr;
}

bool UGSTN_HeldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_HeldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UNetworkPredictionWorldManager* Manager = Collection.InitializeDependency<UNetworkPredictionWorldManager>())
	{
		PostFixedTickHandle = Manager->OnPostFixedTick.AddUObject(this, &UGSTN_HeldSubsystem::HandlePostFixedTick);
	}
	// The authoritative event stream (KO and the like): only the host has one, and only the host breaks holds
	if (UGSTN_SimEventSubsystem* Events = Collection.InitializeDependency<UGSTN_SimEventSubsystem>())
	{
		TWeakObjectPtr<UGSTN_HeldSubsystem> WeakThis(this);
		EventListener = Events->ListenNative(FGameplayTagQuery(), [WeakThis](const FGSTN_SimEvent& Event)
		{
			if (UGSTN_HeldSubsystem* This = WeakThis.Get())
			{
				This->HandleSimEvent(Event);
			}
		});
	}
}

void UGSTN_HeldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UGSTN_LinkWatchdog* Watchdog = UGSTN_LinkWatchdog::Get(&InWorld))
	{
		Watchdog->RegisterPartnerResolver(GSTNEntityKinds::Object, this);
	}
}

bool UGSTN_HeldSubsystem::ResolveLinkPartner(const FGSTN_EntityRef& Partner, FGSTN_LinkPartnerView& OutView) const
{
	if (Partner.Kind != GSTNEntityKinds::Object)
	{
		return false;
	}
	for (const TWeakObjectPtr<UGSTN_HeldObjectComponent>& Each : Objects)
	{
		if (Each.IsValid() && Each->GetOwner() && Each->GetObjectId() == static_cast<int32>(Partner.Id))
		{
			OutView.bValid = true;
			OutView.Transform = Each->GetOwner()->GetActorTransform();
			return true;
		}
	}
	// Known kind, no such object: it was destroyed, the hold is an orphan
	OutView.bValid = false;
	return true;
}

void UGSTN_HeldSubsystem::HandleSimEvent(const FGSTN_SimEvent& Event)
{
	AActor* Target = Event.Target.Get();
	const UGSTN_HeldObjectComponent* Object = Target ? FindHeldBy(Target) : nullptr;
	if (Object && FGSTN_HeldRules::BreaksOnEvent(Event.EventType, Object->GetBreakRules()))
	{
		PendingEventBreaks.Emplace(Target, Event.EventType);
	}
}

void UGSTN_HeldSubsystem::Deinitialize()
{
	if (PostFixedTickHandle.IsValid())
	{
		if (UNetworkPredictionWorldManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr)
		{
			Manager->OnPostFixedTick.Remove(PostFixedTickHandle);
		}
		PostFixedTickHandle.Reset();
	}
	if (EventListener.IsValid())
	{
		if (UGSTN_SimEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UGSTN_SimEventSubsystem>() : nullptr)
		{
			Events->StopListening(EventListener);
		}
		EventListener = FGSTN_SimEventListenerHandle();
	}
	if (UGSTN_LinkWatchdog* Watchdog = UGSTN_LinkWatchdog::Get(GetWorld()))
	{
		Watchdog->UnregisterPartnerResolver(GSTNEntityKinds::Object, this);
	}
	Objects.Reset();
	LastHandSerial.Reset();
	Pending.Reset();
	PendingDrops.Reset();
	PendingEventBreaks.Reset();
	Super::Deinitialize();
}

bool UGSTN_HeldSubsystem::HasAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

void UGSTN_HeldSubsystem::RegisterObject(UGSTN_HeldObjectComponent& Object)
{
	if (HasAuthority())
	{
		Object.ObjectId = NextObjectId++;
	}
	Objects.AddUnique(&Object);
}

void UGSTN_HeldSubsystem::UnregisterObject(UGSTN_HeldObjectComponent& Object)
{
	Objects.Remove(&Object);
}

void UGSTN_HeldSubsystem::RequestPickup(AActor* Holder)
{
	if (!Holder || !HasAuthority())
	{
		return;
	}
	// The request belongs to the frame being simulated now: the post tick of that frame resolves it
	Pending.Add({ Holder, LastFrame + 1 });
}

void UGSTN_HeldSubsystem::RequestDrop(AActor* Holder)
{
	if (Holder && HasAuthority())
	{
		PendingDrops.AddUnique(Holder);
	}
}

UGSTN_HeldObjectComponent* UGSTN_HeldSubsystem::FindHeldBy(const AActor* Holder) const
{
	if (!Holder)
	{
		return nullptr;
	}
	for (const TWeakObjectPtr<UGSTN_HeldObjectComponent>& Each : Objects)
	{
		if (Each.IsValid() && Each->GetState() == EGSTN_HeldObjectState::Held && Each->GetHolder() == Holder)
		{
			return Each.Get();
		}
	}
	return nullptr;
}

void UGSTN_HeldSubsystem::NoteThrown(AActor* Holder, AGSTN_Projectile& Shot)
{
	if (!HasAuthority())
	{
		return;
	}
	if (UGSTN_HeldObjectComponent* Object = FindHeldBy(Holder))
	{
		PostHoldCommand(Holder, *Object, false, EGSTN_HeldBreakReason::Thrown);
		Object->SetFlying(Shot);
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldObject %d thrown by %s at frame %d"), Object->GetObjectId(), *GetNameSafe(Holder), Shot.GetSegment().StartFrame);
	}
}

void UGSTN_HeldSubsystem::HandlePostFixedTick(int32 ServerFrame, int32 EndTimeMS, bool bResimulating)
{
	LastFrame = ServerFrame;
	if (bResimulating || !HasAuthority())
	{
		return;
	}
	// Holds end before new ones start: a holder that drops and picks up in one frame gets the new object, never loses it to its own drop
	StepHolds(ServerFrame);
	ResolvePickups(ServerFrame);
	StepObjects(ServerFrame);
}

void UGSTN_HeldSubsystem::PostHoldCommand(AActor* Holder, UGSTN_HeldObjectComponent& Object, bool bHold, EGSTN_HeldBreakReason Reason)
{
	const UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld());
	UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(GetWorld());
	const int32 EntityId = Projectiles && Holder ? Projectiles->FindEntityIdOf(Holder) : INDEX_NONE;
	const uint8 TypeIndex = FGSTN_HeldLinkType::GetTypeIndex();
	if (!Combat || EntityId == INDEX_NONE || TypeIndex == 0)
	{
		return;
	}
	// Stamped with the holder's own latest frame (the link watchdog's rule): the holder consumes it on its next frame on the host, and its
	// client gets the result by correction
	const UGSTN_CombatSimComponent* Sim = Holder->FindComponentByClass<UGSTN_CombatSimComponent>();
	const FGSTN_CombatSyncState* Sync = Sim ? Sim->GetSyncState() : nullptr;
	FGSTN_InboxEntry Entry;
	if (bHold)
	{
		// Newer than the holder's Hand serial and than any hold formed for it that it has not consumed yet
		const FGSTN_LinkFragment* Links = Sync ? FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Sync->Fragments) : nullptr;
		const uint8* Last = LastHandSerial.Find(Holder);
		const uint8 Serial = FGSTN_LinkState::NextSerial(Links ? Links->Links.Hand.Serial : 0, Last ? *Last : 0);
		LastHandSerial.Add(Holder, Serial);
		Object.HandLinkSerial = Serial;
		Entry = FGSTN_HeldLinkType::MakeHold(TypeIndex, Serial, Object.GetObjectId(), Object.GetHoldSerial());
	}
	else
	{
		Entry = FGSTN_HeldLinkType::MakeRelease(Object.GetHandLinkSerial(), FGSTN_HeldLinkType::ToLinkReason(Reason));
	}
	Entry.Frame = Sync ? Sync->Core.Input.History.GetLatestFrame() : LastFrame;
	Entry.SenderId = EntityId;
	const bool bPosted = Combat->PostToInbox(EntityId, Entry);
	UE_LOG(LogGSTNHeld, Log, TEXT("HeldLink %s entity=%d object=%d serial=%d stamp=%d posted=%d"), bHold ? TEXT("hold") : TEXT("release"), EntityId,
		Object.GetObjectId(), static_cast<int32>(Object.GetHandLinkSerial()), Entry.Frame, bPosted ? 1 : 0);
}

void UGSTN_HeldSubsystem::EndHold(UGSTN_HeldObjectComponent& Object, EGSTN_HeldBreakReason Reason, int32 Frame, const FString& Detail)
{
	AActor* Owner = Object.GetOwner();
	AActor* Holder = Object.GetHolder();
	if (!Owner)
	{
		return;
	}
	const FGSTN_HeldBreakRules& Rules = Object.GetBreakRules();
	FVector Point = Owner->GetActorLocation();
	if (IsValid(Holder))
	{
		PostHoldCommand(Holder, Object, false, Reason);
		// In front of the holder as hits judge it (its snapshot entry), the actor without one
		const UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld());
		const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(GetWorld());
		const int32 Id = Projectiles ? Projectiles->FindEntityIdOf(Holder) : INDEX_NONE;
		const FGSTN_SnapshotEntry* Entry = Combat && Id != INDEX_NONE ? Combat->GetSnapshot().Find(Id) : nullptr;
		Point = FGSTN_HeldRules::DropPoint(Entry ? Entry->Transform : Holder->GetActorTransform(), Rules);
	}
	Object.SetIdleAt(FloorBelow(Point, Object.GetTuning().RestHalfHeight, Owner));
	const UGSTN_ProjectileSubsystem* Ids = UGSTN_ProjectileSubsystem::Get(GetWorld());
	UE_LOG(LogGSTNHeld, Log, TEXT("HeldBreak object=%d entity=%d reason=%s frame=%d %s"), Object.GetObjectId(),
		Ids && IsValid(Holder) ? Ids->FindEntityIdOf(Holder) : INDEX_NONE,
		*StaticEnum<EGSTN_HeldBreakReason>()->GetNameStringByValue(static_cast<int64>(Reason)), Frame, *Detail);
}

void UGSTN_HeldSubsystem::StepHolds(int32 Frame)
{
	TArray<TWeakObjectPtr<AActor>> Drops = MoveTemp(PendingDrops);
	PendingDrops.Reset();
	for (const TWeakObjectPtr<AActor>& Holder : Drops)
	{
		if (UGSTN_HeldObjectComponent* Object = FindHeldBy(Holder.Get()))
		{
			EndHold(*Object, EGSTN_HeldBreakReason::Dropped, Frame);
		}
	}
	TArray<TPair<TWeakObjectPtr<AActor>, FGameplayTag>> Events = MoveTemp(PendingEventBreaks);
	PendingEventBreaks.Reset();
	for (const TPair<TWeakObjectPtr<AActor>, FGameplayTag>& Break : Events)
	{
		if (UGSTN_HeldObjectComponent* Object = FindHeldBy(Break.Key.Get()))
		{
			EndHold(*Object, EGSTN_HeldBreakReason::HolderEvent, Frame, Break.Value.ToString());
		}
	}
	// The holder's state at the end of this frame: its tags are the combat tick's projection of its sync state
	FGameplayTagContainer HolderTags;
	for (const TWeakObjectPtr<UGSTN_HeldObjectComponent>& Each : Objects)
	{
		UGSTN_HeldObjectComponent* Object = Each.Get();
		if (!Object || Object->GetState() != EGSTN_HeldObjectState::Held || !IsValid(Object->GetHolder()))
		{
			continue;
		}
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Object->GetHolder());
		if (!AbilitySystem)
		{
			continue;
		}
		HolderTags.Reset();
		AbilitySystem->GetOwnedGameplayTags(HolderTags);
		const FGSTN_HeldBreakRules& Rules = Object->GetBreakRules();
		if (FGSTN_HeldRules::BreaksOnState(HolderTags, Rules))
		{
			EndHold(*Object, EGSTN_HeldBreakReason::HolderState, Frame, HolderTags.Filter(Rules.DropWhenHolderHasAny).ToStringSimple());
		}
	}
}

UGSTN_HeldObjectComponent* UGSTN_HeldSubsystem::FindPickupTarget(const FVector& From) const
{
	TArray<FGSTN_PickupCandidate, TInlineAllocator<16>> Candidates;
	for (int32 Index = 0; Index < Objects.Num(); ++Index)
	{
		const UGSTN_HeldObjectComponent* Each = Objects[Index].Get();
		if (Each && Each->GetOwner())
		{
			// Ids are the host's; a client has none and ties on the list order instead (the same on that machine every frame)
			const int32 Id = Each->GetObjectId() != INDEX_NONE ? Each->GetObjectId() : Index;
			Candidates.Add({ Id, Each->GetOwner()->GetActorLocation(), Each->GetTuning().PickupRadius, Each->GetState() == EGSTN_HeldObjectState::Idle });
		}
	}
	const int32 Target = FGSTN_HeldRules::FindPickupTarget(From, Candidates);
	if (Target == INDEX_NONE)
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < Objects.Num(); ++Index)
	{
		UGSTN_HeldObjectComponent* Each = Objects[Index].Get();
		if (Each && (Each->GetObjectId() != INDEX_NONE ? Each->GetObjectId() : Index) == Target)
		{
			return Each;
		}
	}
	return nullptr;
}

void UGSTN_HeldSubsystem::ResolvePickups(int32 Frame)
{
	if (Pending.IsEmpty())
	{
		return;
	}
	const UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld());
	const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(GetWorld());
	const UGSTN_InteractSubsystem* Interact = UGSTN_InteractSubsystem::Get(GetWorld());

	TArray<FGSTN_PickupRequest> Requests;
	TMap<int32, AActor*> HolderOf;
	for (const FQueuedPickup& Queued : Pending)
	{
		AActor* Holder = Queued.Holder.Get();
		if (!Holder || Queued.Frame > Frame || FindHeldBy(Holder))
		{
			continue;
		}
		const int32 Id = Projectiles ? Projectiles->FindEntityIdOf(Holder) : INDEX_NONE;
		if (Id == INDEX_NONE)
		{
			continue;
		}
		// The pose hits are judged against: the holder's snapshot entry, the actor without one
		const FGSTN_SnapshotEntry* Entry = Combat ? Combat->GetSnapshot().Find(Id) : nullptr;
		FGSTN_PickupRequest& Request = Requests.Add_GetRef({ Id, Queued.Frame, Entry ? Entry->Transform.GetLocation() : Holder->GetActorLocation() });
		// M10 slice 5: the object whose record the interact probe focused for this holder on the request's frame (or just before it)
		FGSTN_InteractCandidateId Focus;
		if (Interact && Interact->FindProbeFocus(Id, Queued.Frame, Focus) && Focus.Provider == GSTNInteractProviders::Registry)
		{
			for (const TWeakObjectPtr<UGSTN_HeldObjectComponent>& Each : Objects)
			{
				if (Each.IsValid() && Each->RecordStamp.Handle != 0 && Each->RecordStamp.Handle == Focus.Handle)
				{
					Request.TargetObjectId = Each->GetObjectId();
					break;
				}
			}
		}
		HolderOf.Add(Id, Holder);
	}
	Pending.RemoveAll([Frame](const FQueuedPickup& Queued) { return Queued.Frame <= Frame; });

	TArray<FGSTN_PickupCandidate> Candidates;
	TMap<int32, UGSTN_HeldObjectComponent*> ObjectOf;
	for (const TWeakObjectPtr<UGSTN_HeldObjectComponent>& Each : Objects)
	{
		if (Each.IsValid() && Each->GetOwner())
		{
			Candidates.Add({ Each->GetObjectId(), Each->GetOwner()->GetActorLocation(), Each->GetTuning().PickupRadius,
				Each->GetState() == EGSTN_HeldObjectState::Idle });
			ObjectOf.Add(Each->GetObjectId(), Each.Get());
		}
	}

	TArray<FGSTN_PickupAssignment> Assignments;
	FGSTN_HeldRules::ResolvePickups(Requests, Candidates, Assignments);
	for (const FGSTN_PickupRequest& Request : Requests)
	{
		const FGSTN_PickupAssignment* Won = Assignments.FindByPredicate([&Request](const FGSTN_PickupAssignment& A) { return A.HolderId == Request.HolderId; });
		if (!Won)
		{
			UE_LOG(LogGSTNHeld, Log, TEXT("HeldPickup refused for entity %d at frame %d (nothing free in range)"), Request.HolderId, Frame);
			continue;
		}
		ObjectOf[Won->ObjectId]->SetHeld(HolderOf[Won->HolderId]);
		PostHoldCommand(HolderOf[Won->HolderId], *ObjectOf[Won->ObjectId], true, EGSTN_HeldBreakReason::None);
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldPickup entity %d holds object %d at frame %d (focus %d)"), Won->HolderId, Won->ObjectId, Frame, Request.TargetObjectId);
	}
}

FVector UGSTN_HeldSubsystem::FloorBelow(const FVector& Location, float HalfHeight, const AActor* Ignore) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNHeldFloor), false, Ignore);
	const FVector From = Location + FVector(0.0, 0.0, 50.0);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, From, From - FVector(0.0, 0.0, 5000.0), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.ImpactPoint + FVector(0.0, 0.0, HalfHeight);
	}
	return Location;
}

void UGSTN_HeldSubsystem::StepObjects(int32 Frame)
{
	UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld());
	for (const TWeakObjectPtr<UGSTN_HeldObjectComponent>& Each : Objects)
	{
		UGSTN_HeldObjectComponent* Object = Each.Get();
		AActor* Owner = Object ? Object->GetOwner() : nullptr;
		if (!Owner)
		{
			continue;
		}
		const float HalfHeight = Object->GetTuning().RestHalfHeight;
		if (Object->GetState() == EGSTN_HeldObjectState::Held && !IsValid(Object->GetHolder()))
		{
			EndHold(*Object, EGSTN_HeldBreakReason::HolderGone, Frame);
			continue;
		}
		if (Object->GetState() != EGSTN_HeldObjectState::Flying)
		{
			continue;
		}
		const FGSTN_BallisticSegment& Segment = Object->FlightSegment;
		AGSTN_Projectile* Shot = Object->GetFlight();
		if (!IsValid(Shot) || Shot->IsActorBeingDestroyed() || Frame > Object->FlightEndFrame)
		{
			// The shot hit (consumed) or expired: the object falls to the floor below where the shot was last
			const int32 LastFrameOfShot = FMath::Clamp(Frame - 1, Segment.StartFrame, Object->FlightEndFrame);
			const FVector Rest = FloorBelow(FGSTN_BallisticMode::PositionAt(Segment, LastFrameOfShot), HalfHeight, Owner);
			Object->SetIdleAt(Rest);
			UE_LOG(LogGSTNHeld, Log, TEXT("HeldObject %d landed at frame %d after its shot ended, at %s"), Object->GetObjectId(), Frame, *Rest.ToCompactString());
			continue;
		}
		if (Frame <= Segment.StartFrame)
		{
			continue;
		}
		// World impact on this frame's step: the object lands on what it met (the shot's own sweep sees only hurtboxes)
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNHeldFlight), false, Owner);
		Params.AddIgnoredActor(Shot);
		const FVector From = FGSTN_BallisticMode::PositionAt(Segment, Frame - 1);
		const FVector To = FGSTN_BallisticMode::PositionAt(Segment, Frame);
		if (GetWorld()->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			if (Projectiles)
			{
				Projectiles->RemoveProjectile(Shot);
			}
			const FVector Rest = Hit.ImpactNormal.Z > 0.7 ? Hit.ImpactPoint + FVector(0.0, 0.0, HalfHeight)
				: FloorBelow(Hit.ImpactPoint + Hit.ImpactNormal * HalfHeight, HalfHeight, Owner);
			Object->SetIdleAt(Rest);
			UE_LOG(LogGSTNHeld, Log, TEXT("HeldObject %d landed at frame %d on %s, at %s"), Object->GetObjectId(), Frame, *GetNameSafe(Hit.GetActor()), *Rest.ToCompactString());
		}
	}
}

void UGSTN_HeldSubsystem::LogReport() const
{
	const UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld());
	UE_LOG(LogGSTNHeld, Log, TEXT("HeldReport world=%s authority=%d frame=%d objects=%d"), *GetNameSafe(GetWorld()), HasAuthority() ? 1 : 0, LastFrame, Objects.Num());
	for (const TWeakObjectPtr<UGSTN_HeldObjectComponent>& Each : Objects)
	{
		if (!Each.IsValid() || !Each->GetOwner())
		{
			continue;
		}
		const AActor* Holder = Each->GetHolder();
		const FVector Location = Each->GetOwner()->GetActorLocation();
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldObjectReport world=%s name=%s state=%d holderEntity=%d holderDistCm=%.1f loc=%.1f,%.1f,%.1f"),
			*GetNameSafe(GetWorld()), *Each->GetOwner()->GetName(), static_cast<int32>(Each->GetState()),
			Holder && Projectiles ? Projectiles->FindEntityIdOf(Holder) : INDEX_NONE, Holder ? FVector::Dist(Holder->GetActorLocation(), Location) : -1.0,
			Location.X, Location.Y, Location.Z);
	}
	// Every combat pawn's hand in this world: the projected Combat.Held.Carrying (from its predicted or corrected fragment)
	if (const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(GetWorld()))
	{
		TArray<int32> Ids;
		Combat->GetRegisteredEntityIds(Ids);
		Ids.Sort();
		for (const int32 Id : Ids)
		{
			const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Combat->FindOwnerActor(Id));
			UE_LOG(LogGSTNHeld, Log, TEXT("HeldHandReport world=%s entity=%d carrying=%d target=%d"), *GetNameSafe(GetWorld()), Id,
				AbilitySystem && AbilitySystem->HasMatchingGameplayTag(GSTNHeldTags::Held_Carrying) ? 1 : 0,
				AbilitySystem && AbilitySystem->HasMatchingGameplayTag(GSTNHeldTags::Interact_Target_Pickup) ? 1 : 0);
		}
	}
}

namespace GSTNHeldConsole
{
	UWorld* FindAuthorityWorld(UWorld* InWorld)
	{
		if (InWorld && InWorld->GetNetMode() != NM_Client && UGSTN_HeldSubsystem::Get(InWorld))
		{
			return InWorld;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && World->GetNetMode() != NM_Client && UGSTN_HeldSubsystem::Get(World))
			{
				return World;
			}
		}
		return nullptr;
	}

	/** Combat pawns of the host world, by ascending entity id (the numbering of gstn.Projectile.Ability) */
	void CombatPawns(UWorld* World, TArray<APawn*>& OutPawns)
	{
		OutPawns.Reset();
		const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World);
		if (!Combat)
		{
			return;
		}
		TArray<int32> Ids;
		Combat->GetRegisteredEntityIds(Ids);
		Ids.Sort();
		for (const int32 Id : Ids)
		{
			if (APawn* Pawn = Cast<APawn>(Combat->FindOwnerActor(Id)))
			{
				OutPawns.Add(Pawn);
			}
		}
	}

	template <typename TAbility>
	TAbility* GrantedInstance(APawn* Pawn, FGameplayAbilitySpecHandle& OutHandle, UAbilitySystemComponent*& OutAbilitySystem)
	{
		OutAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		if (!OutAbilitySystem)
		{
			return nullptr;
		}
		FGameplayAbilitySpec* Spec = OutAbilitySystem->FindAbilitySpecFromClass(TAbility::StaticClass());
		if (!Spec)
		{
			OutAbilitySystem->GiveAbility(FGameplayAbilitySpec(TAbility::StaticClass(), 1));
			Spec = OutAbilitySystem->FindAbilitySpecFromClass(TAbility::StaticClass());
		}
		if (!Spec)
		{
			return nullptr;
		}
		OutHandle = Spec->Handle;
		return Cast<TAbility>(Spec->GetPrimaryInstance());
	}

	void Spawn(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = FindAuthorityWorld(InWorld);
		TArray<APawn*> Pawns;
		CombatPawns(World, Pawns);
		const int32 PawnIndex = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		if (!World || !Pawns.IsValidIndex(PawnIndex))
		{
			UE_LOG(LogGSTNHeld, Warning, TEXT("gstn.Held.Spawn: no host world or no combat pawn %d"), PawnIndex);
			return;
		}
		const double Forward = Args.IsValidIndex(1) ? FCString::Atod(*Args[1]) : 120.0;
		const APawn* Pawn = Pawns[PawnIndex];
		FVector Location = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * Forward;
		AGSTN_HeldObjectActor* Actor = World->SpawnActor<AGSTN_HeldObjectActor>(AGSTN_HeldObjectActor::StaticClass(), Location, FRotator::ZeroRotator);
		if (Actor && Actor->GetHeldObject())
		{
			const float HalfHeight = Actor->GetHeldObject()->GetTuning().RestHalfHeight;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNHeldSpawn), false, Actor);
			if (World->LineTraceSingleByObjectType(Hit, Location, Location - FVector(0.0, 0.0, 5000.0), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				Location = Hit.ImpactPoint + FVector(0.0, 0.0, HalfHeight);
			}
			Actor->GetHeldObject()->SetIdleAt(Location);
		}
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldSpawn %s near pawn %d at %s"), *GetNameSafe(Actor), PawnIndex, *Location.ToCompactString());
	}

	void Pickup(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = FindAuthorityWorld(InWorld);
		TArray<APawn*> Pawns;
		CombatPawns(World, Pawns);
		const int32 PawnIndex = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		if (!Pawns.IsValidIndex(PawnIndex))
		{
			UE_LOG(LogGSTNHeld, Warning, TEXT("gstn.Held.Pickup: no combat pawn %d"), PawnIndex);
			return;
		}
		FGameplayAbilitySpecHandle Handle;
		UAbilitySystemComponent* AbilitySystem = nullptr;
		if (!GrantedInstance<UGSTN_GA_HeldPickupServer>(Pawns[PawnIndex], Handle, AbilitySystem))
		{
			UE_LOG(LogGSTNHeld, Warning, TEXT("gstn.Held.Pickup: no ability system or instance on pawn %d"), PawnIndex);
			return;
		}
		FGameplayEventData Payload;
		Payload.Instigator = Pawns[PawnIndex];
		const bool bActivated = AbilitySystem->TriggerAbilityFromGameplayEvent(Handle, AbilitySystem->AbilityActorInfo.Get(), FGameplayTag(), &Payload, *AbilitySystem);
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldPickupAbility pawn=%d activated=%d"), PawnIndex, bActivated ? 1 : 0);
	}

	void Throw(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = FindAuthorityWorld(InWorld);
		TArray<APawn*> Pawns;
		CombatPawns(World, Pawns);
		const int32 ThrowerIndex = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		const int32 TargetIndex = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 1;
		if (!Pawns.IsValidIndex(ThrowerIndex) || !Pawns.IsValidIndex(TargetIndex) || ThrowerIndex == TargetIndex)
		{
			UE_LOG(LogGSTNHeld, Warning, TEXT("gstn.Held.Throw: thrower %d / target %d not among %d combat pawns"), ThrowerIndex, TargetIndex, Pawns.Num());
			return;
		}
		FGameplayAbilitySpecHandle Handle;
		UAbilitySystemComponent* AbilitySystem = nullptr;
		UGSTN_GA_HeldThrowServer* Instance = GrantedInstance<UGSTN_GA_HeldThrowServer>(Pawns[ThrowerIndex], Handle, AbilitySystem);
		if (!Instance)
		{
			UE_LOG(LogGSTNHeld, Warning, TEXT("gstn.Held.Throw: no ability system or instance on pawn %d"), ThrowerIndex);
			return;
		}
		// The harness aims at the other pawn (event target, flat arc): a dev command only, a player aims himself
		Instance->Aim = EGSTN_ProjectileAim::EventTarget;
		FGameplayEventData Payload;
		Payload.Instigator = Pawns[ThrowerIndex];
		Payload.Target = Pawns[TargetIndex];
		const bool bActivated = AbilitySystem->TriggerAbilityFromGameplayEvent(Handle, AbilitySystem->AbilityActorInfo.Get(), FGameplayTag(), &Payload, *AbilitySystem);
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldThrowAbility thrower=%d target=%d activated=%d"), ThrowerIndex, TargetIndex, bActivated ? 1 : 0);
	}

	void Drop(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = FindAuthorityWorld(InWorld);
		TArray<APawn*> Pawns;
		CombatPawns(World, Pawns);
		const int32 PawnIndex = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		if (!Pawns.IsValidIndex(PawnIndex))
		{
			UE_LOG(LogGSTNHeld, Warning, TEXT("gstn.Held.Drop: no combat pawn %d"), PawnIndex);
			return;
		}
		FGameplayAbilitySpecHandle Handle;
		UAbilitySystemComponent* AbilitySystem = nullptr;
		if (!GrantedInstance<UGSTN_GA_HeldDropServer>(Pawns[PawnIndex], Handle, AbilitySystem))
		{
			UE_LOG(LogGSTNHeld, Warning, TEXT("gstn.Held.Drop: no ability system or instance on pawn %d"), PawnIndex);
			return;
		}
		FGameplayEventData Payload;
		Payload.Instigator = Pawns[PawnIndex];
		const bool bActivated = AbilitySystem->TriggerAbilityFromGameplayEvent(Handle, AbilitySystem->AbilityActorInfo.Get(), FGameplayTag(), &Payload, *AbilitySystem);
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldDropAbility pawn=%d activated=%d"), PawnIndex, bActivated ? 1 : 0);
	}

	void Report(const TArray<FString>& Args, UWorld* InWorld)
	{
		if (const UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(InWorld))
		{
			Held->LogReport();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GHeldSpawnCommand(
		TEXT("gstn.Held.Spawn"),
		TEXT("Host: spawns a test held object in front of a combat pawn (pawns by ascending entity id), on the floor. Args: [Pawn=0] [ForwardCm=120]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Spawn));
	FAutoConsoleCommandWithWorldAndArgs GHeldPickupCommand(
		TEXT("gstn.Held.Pickup"),
		TEXT("Host: grants the ServerOnly pickup ability to a combat pawn and activates it. Args: [Pawn=0]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Pickup));
	FAutoConsoleCommandWithWorldAndArgs GHeldThrowCommand(
		TEXT("gstn.Held.Throw"),
		TEXT("Host: grants the ServerOnly throw ability to a combat pawn and throws its held object at another pawn. Args: [Thrower=0] [Target=1]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Throw));
	FAutoConsoleCommandWithWorldAndArgs GHeldDropCommand(
		TEXT("gstn.Held.Drop"),
		TEXT("Host: grants the ServerOnly drop ability to a combat pawn and activates it (puts its object down in front of it). Args: [Pawn=0]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Drop));
	FAutoConsoleCommandWithWorldAndArgs GHeldReportCommand(
		TEXT("gstn.Held.Report"),
		TEXT("Logs every held object of the world it runs in (state, holder entity, location): HeldReport / HeldObjectReport lines"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Report));
}
