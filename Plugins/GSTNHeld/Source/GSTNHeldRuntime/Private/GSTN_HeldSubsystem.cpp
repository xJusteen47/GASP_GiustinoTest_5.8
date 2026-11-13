// Held objects, world side (needs the GAS-NPP fork for the fixed tick it reads).

#include "GSTN_HeldSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GSTN_HeldAbilities.h"
#include "GSTN_HeldObjectActor.h"
#include "GSTN_HeldObjectComponent.h"
#include "GSTN_HeldSim.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "GSTN_HeldSettings.h"
#include "GSTN_HeldLoadEffect.h"
#include "GSTN_HeldTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "GSTN_InteractSubsystem.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkWatchdog.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "State/GSTN_LinkState.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "GSTN_ProjectileActor.h"
#include "GSTN_ProjectileSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"
#include "Modules/ModuleManager.h"
#include "NetworkPredictionWorldManager.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

static TAutoConsoleVariable<int32> CVarHeldDebug(TEXT("gstn.Held.Debug"), 0,
	TEXT("1 = draws every held object of the world each fixed frame: state (Idle green, Held yellow, Flying red, Broken grey) and id, a line to its ")
	TEXT("holder, the flight's positions 10 frames back and ahead (white) and a blue line to the authority shot (on the thrower's client: the ghost's error)"),
	ECVF_Cheat);

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
	// M10 G8: a client's object drawn at a throw's ghost follows the host's projectile that takes the ghost over
	if (UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(&InWorld); Projectiles && !HasAuthority())
	{
		GhostReconciledHandle = Projectiles->OnGhostReconciled.AddUObject(this, &UGSTN_HeldSubsystem::HandleGhostReconciled);
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
	if (GhostReconciledHandle.IsValid())
	{
		if (UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld()))
		{
			Projectiles->OnGhostReconciled.Remove(GhostReconciledHandle);
		}
		GhostReconciledHandle.Reset();
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
		Object->Bounces = 0;
		Object->Thrower = Holder;
		Object->SetFlying(Shot);
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldObject %d thrown by %s at frame %d"), Object->GetObjectId(), *GetNameSafe(Holder), Shot.GetSegment().StartFrame);
	}
}

void UGSTN_HeldSubsystem::DrawDebugFrame(int32 Frame) const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	// One fixed frame (62.5 Hz), so the drawing neither flickers nor piles up
	const float Life = 0.016f;
	for (const TWeakObjectPtr<UGSTN_HeldObjectComponent>& Weak : Objects)
	{
		const UGSTN_HeldObjectComponent* Object = Weak.Get();
		const AActor* Actor = Object ? Object->GetOwner() : nullptr;
		if (!Actor)
		{
			continue;
		}
		const FVector At = Actor->GetActorLocation();
		const EGSTN_HeldObjectState State = Object->GetState();
		const FColor Color = State == EGSTN_HeldObjectState::Idle ? FColor::Green : State == EGSTN_HeldObjectState::Held ? FColor::Yellow
			: State == EGSTN_HeldObjectState::Flying ? FColor::Red : FColor(128, 128, 128);
		DrawDebugSphere(World, At, Object->GetTuning().RestHalfHeight, 8, Color, false, Life);
		DrawDebugString(World, At + FVector(0.0, 0.0, 40.0), FString::Printf(TEXT("%d %s"), Object->GetObjectId(),
			*StaticEnum<EGSTN_HeldObjectState>()->GetNameStringByValue(static_cast<int64>(State))), nullptr, Color, Life);
		if (State == EGSTN_HeldObjectState::Held && Object->GetHolder())
		{
			DrawDebugLine(World, At, Object->GetHolder()->GetActorLocation(), Color, false, Life);
		}
		if (const AGSTN_Projectile* Flight = State == EGSTN_HeldObjectState::Flying ? Object->GetFlight() : nullptr)
		{
			// The flight's closed form 10 frames back and ahead (white), and where the authority shot is now (blue line): on the throwing client
			// the object is drawn at its ghost, so the line is the ghost's error against the server
			for (int32 Offset = -10; Offset <= 10; Offset += 2)
			{
				DrawDebugPoint(World, Flight->GetDrawnPositionAt(Frame + Offset), Offset == 0 ? 10.f : 5.f, FColor::White, false, Life);
			}
			DrawDebugLine(World, At, Flight->GetDrawnPositionAt(Frame), FColor::Blue, false, Life, 0, 2.f);
		}
	}
#endif
}

void UGSTN_HeldSubsystem::HandlePostFixedTick(int32 ServerFrame, int32 EndTimeMS, bool bResimulating)
{
	LastFrame = ServerFrame;
#if ENABLE_DRAW_DEBUG
	if (!bResimulating && CVarHeldDebug.GetValueOnGameThread() != 0)
	{
		DrawDebugFrame(ServerFrame);
	}
#endif
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
		// M10 G6: the carry load rides on the link (size class index and weight over the holder's capacity, FGSTN_HeldLoad)
		const UGSTN_HeldRules& Settings = UGSTN_HeldRules::Get();
		int32 SizeIndex = INDEX_NONE;
		Settings.FindSizeClass(Object.GetTuning().SizeTag, &SizeIndex);
		const uint8 Payload = SizeIndex == INDEX_NONE ? 0
			: FGSTN_HeldLoad::EncodePayload(SizeIndex, FGSTN_HeldLoad::LoadRatio(Object.GetTuning().WeightKg, CarryCapacityOf(Holder), Settings.DefaultCarryCapacityKg));
		Object.HoldLoadPayload = Payload;
		Entry = FGSTN_HeldLinkType::MakeHold(TypeIndex, Serial, Object.GetObjectId(), Object.GetHoldSerial(), Payload);
	}
	else
	{
		Entry = FGSTN_HeldLinkType::MakeRelease(Object.GetHandLinkSerial(), FGSTN_HeldLinkType::ToLinkReason(Reason));
	}
	Entry.Frame = Sync ? Sync->Core.Input.History.GetLatestFrame() : LastFrame;
	Entry.SenderId = EntityId;
	const bool bPosted = Combat->PostToInbox(EntityId, Entry);
	UE_LOG(LogGSTNHeld, Log, TEXT("HeldLink %s entity=%d object=%d serial=%d stamp=%d posted=%d load=%d"), bHold ? TEXT("hold") : TEXT("release"), EntityId,
		Object.GetObjectId(), static_cast<int32>(Object.GetHandLinkSerial()), Entry.Frame, bPosted ? 1 : 0, bHold ? static_cast<int32>(Object.HoldLoadPayload) : -1);
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
		Request.CarryCapacityKg = FGSTN_HeldLoad::EffectiveCapacity(CarryCapacityOf(Holder), UGSTN_HeldRules::Get().DefaultCarryCapacityKg);
		// M10 slice 5: the object whose record the interact probe focused for this holder on the request's frame (or just before it)
		FGSTN_InteractCandidateId Focus;
		Request.bFallBackToNearest = UGSTN_HeldRules::Get().bFocusMissFallsBackToNearest;
		if (UGSTN_HeldRules::Get().bPickupTakesFocusedObject && Interact && Interact->FindProbeFocus(Id, Queued.Frame, Focus) && Focus.Provider == GSTNInteractProviders::Registry)
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
			FGSTN_PickupCandidate& Candidate = Candidates.Add_GetRef({ Each->GetObjectId(), Each->GetOwner()->GetActorLocation(), Each->GetTuning().PickupRadius,
				Each->GetState() == EGSTN_HeldObjectState::Idle });
			const FGSTN_HeldSizeClass* SizeClass = UGSTN_HeldRules::Get().FindSizeClass(Each->GetTuning().SizeTag);
			Candidate.WeightKg = Each->GetTuning().WeightKg;
			Candidate.bRequiresCapacity = SizeClass && SizeClass->bRequiresCapacity;
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

float UGSTN_HeldSubsystem::CarryCapacityOf(const AActor* Holder)
{
	// The holder's own attribute (0 by default = none of its own: the settings' default applies, FGSTN_HeldLoad::EffectiveCapacity)
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Holder);
	bool bFound = false;
	const float Capacity = AbilitySystem ? AbilitySystem->GetGameplayAttributeValue(UGSTN_CombatAttributeSet::GetMaxCarryWeightAttribute(), bFound) : 0.f;
	return bFound ? Capacity : 0.f;
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
		// M10 G7: a broken object comes back at its home once its respawn frame is reached
		if (Object->GetState() == EGSTN_HeldObjectState::Broken)
		{
			if (Object->RespawnFrame != INDEX_NONE && Frame >= Object->RespawnFrame)
			{
				Object->SetIdleAt(Object->HomeLocation);
				UE_LOG(LogGSTNHeld, Log, TEXT("HeldImpact object=%d respawned frame=%d at %s"), Object->GetObjectId(), Frame, *Object->HomeLocation.ToCompactString());
			}
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
			// The shot hit (consumed) or expired: the object falls to the floor below where the shot was last (or breaks on the fighter, M10 G7)
			const int32 LastFrameOfShot = FMath::Clamp(Frame - 1, Segment.StartFrame, Object->FlightEndFrame);
			FVector Unused;
			const EGSTN_HeldImpactBehaviour OnFighter = Frame <= Object->FlightEndFrame
				? FGSTN_HeldRules::ResolveImpact(Object->GetTuning(), false, FVector::ZeroVector, FVector::ZeroVector, 0, Unused) : EGSTN_HeldImpactBehaviour::Land;
			if (OnFighter == EGSTN_HeldImpactBehaviour::Break)
			{
				BreakObject(*Object, Frame, TEXT("hit"));
				continue;
			}
			if (OnFighter == EGSTN_HeldImpactBehaviour::Explode)
			{
				ExplodeObject(*Object, FGSTN_BallisticMode::PositionAt(Segment, LastFrameOfShot), Frame, TEXT("hit"));
				continue;
			}
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
			// M10 G7: the throw's hits so far, carried into a rebound (one hit per target per throw)
			TArray<int32> ThrowHitTargets;
			if (Projectiles)
			{
				Projectiles->GetHitTargets(Shot, ThrowHitTargets);
				Projectiles->RemoveProjectile(Shot);
			}
			// M10 G7: the object's impact behaviour (Land, Break, Bounce, Stick)
			FVector ReboundVelocity;
			const EGSTN_HeldImpactBehaviour Impact = FGSTN_HeldRules::ResolveImpact(Object->GetTuning(), true,
				FGSTN_BallisticMode::VelocityAt(Segment, Frame), Hit.ImpactNormal, Object->Bounces, ReboundVelocity);
			if (Impact == EGSTN_HeldImpactBehaviour::Break)
			{
				BreakObject(*Object, Frame, GetNameSafe(Hit.GetActor()));
				continue;
			}
			if (Impact == EGSTN_HeldImpactBehaviour::Explode)
			{
				ExplodeObject(*Object, Hit.ImpactPoint + Hit.ImpactNormal * HalfHeight, Frame, GetNameSafe(Hit.GetActor()));
				continue;
			}
			if (Impact == EGSTN_HeldImpactBehaviour::Bounce && Projectiles && Rebound(*Object, Hit.ImpactPoint + Hit.ImpactNormal * HalfHeight, ReboundVelocity, Frame, ThrowHitTargets))
			{
				continue;
			}
			const FVector Rest = Impact == EGSTN_HeldImpactBehaviour::Stick ? Hit.ImpactPoint + Hit.ImpactNormal * HalfHeight
				: Hit.ImpactNormal.Z > 0.7 ? Hit.ImpactPoint + FVector(0.0, 0.0, HalfHeight)
				: FloorBelow(Hit.ImpactPoint + Hit.ImpactNormal * HalfHeight, HalfHeight, Owner);
			Object->SetIdleAt(Rest);
			UE_LOG(LogGSTNHeld, Log, TEXT("HeldImpact object=%d behaviour=%s frame=%d on=%s at=%s"), Object->GetObjectId(),
				Impact == EGSTN_HeldImpactBehaviour::Stick ? TEXT("Stick") : TEXT("Land"), Frame, *GetNameSafe(Hit.GetActor()), *Rest.ToCompactString());
			UE_LOG(LogGSTNHeld, Log, TEXT("HeldObject %d landed at frame %d on %s, at %s"), Object->GetObjectId(), Frame, *GetNameSafe(Hit.GetActor()), *Rest.ToCompactString());
		}
	}
}

void UGSTN_HeldSubsystem::BreakObject(UGSTN_HeldObjectComponent& Object, int32 Frame, const FString& On)
{
	const float Seconds = Object.GetTuning().BreakRespawnSeconds;
	const UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld());
	const double Step = Projectiles && Projectiles->GetStepSeconds() > 0.0 ? Projectiles->GetStepSeconds() : 1.0 / 60.0;
	const int32 Respawn = Seconds > 0.f ? Frame + FMath::Max(1, FMath::RoundToInt(Seconds / Step)) : INDEX_NONE;
	Object.SetBroken(Respawn);
	UE_LOG(LogGSTNHeld, Log, TEXT("HeldImpact object=%d behaviour=Break frame=%d on=%s respawn=%d"), Object.GetObjectId(), Frame, *On, Respawn);
}

void UGSTN_HeldSubsystem::ExplodeObject(UGSTN_HeldObjectComponent& Object, const FVector& At, int32 Frame, const FString& On)
{
	UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld());
	const FGSTN_HeldObjectTuning& Tuning = Object.GetTuning();
	AActor* Thrower = Object.Thrower.Get();
	const AGSTN_Projectile* Blast = nullptr;
	if (Projectiles && Projectiles->CanFire())
	{
		// A shot that does not move and lives one step: the projectile pipeline gives relationships, friendly fire by source, the inbox and the
		// credit as for the throw. It hits every fighter in reach once, the throw's direct victims too (a direct hit and the blast, TEST CHOICE)
		FGSTN_ProjectileParams Params;
		Params.Origin = At;
		Params.Speed = 0.f;
		Params.GravityAccel = FVector::ZeroVector;
		Params.LifetimeFrames = 1;
		Params.Shape.Type = EGSTN_ShapeType::Sphere;
		Params.Shape.Extent = FVector(Tuning.ExplodeRadiusCm);
		Params.Hit = Tuning.bExplodeOwnHit ? Tuning.ExplodeHit : Tuning.Throw.Hit;
		Params.SourceTag = GSTNCombatHitTags::Source_Thrown;
		Params.MaxHits = MAX_uint8;
		Params.bRadialPush = true;
		Params.OwnerEntityId = Thrower ? Projectiles->FindEntityIdOf(Thrower) : INDEX_NONE;
		Blast = Projectiles->FireProjectileAtFrame(Params, Frame + 1, Thrower);
	}
	UE_LOG(LogGSTNHeld, Log, TEXT("HeldImpact object=%d behaviour=Explode frame=%d on=%s at=%.0f,%.0f,%.0f radius=%.0f blast=%d"), Object.GetObjectId(), Frame, *On,
		At.X, At.Y, At.Z, Tuning.ExplodeRadiusCm, Blast ? 1 : 0);
	BreakObject(Object, Frame, On);
}

void UGSTN_HeldSubsystem::NotePredictedThrow(AActor* Holder, AGSTN_Projectile& Ghost)
{
	UGSTN_HeldObjectComponent* Object = FindHeldBy(Holder);
	if (!Object || HasAuthority())
	{
		return;
	}
	// A re-simulated release finds the ghost already drawn (SpawnGhost returns it): counted once
	if (Object->PredictedFlight.Get() != &Ghost)
	{
		++GhostStats.Predicted;
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldGhost predicted object=%d start=%d"), Object->GetObjectId(), Ghost.GetSegment().StartFrame);
	}
	Object->PredictedFlight = &Ghost;
	Object->bPredicting = true;
	Object->bPredictionHanded = false;
}

void UGSTN_HeldSubsystem::HandleGhostReconciled(AGSTN_Projectile* Ghost, AGSTN_Projectile* Authority, bool bAdopted)
{
	const UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld());
	for (const TWeakObjectPtr<UGSTN_HeldObjectComponent>& Each : Objects)
	{
		UGSTN_HeldObjectComponent* Object = Each.Get();
		if (!Object || !Object->bPredicting || Object->PredictedFlight.Get() != Ghost)
		{
			continue;
		}
		Object->PredictedFlight = Authority;
		Object->bPredictionHanded = true;
		GhostStats.LastErrorCm = Projectiles ? Projectiles->GetGhostStats().LastErrorCm : 0.0;
		if (bAdopted)
		{
			++GhostStats.Handed;
			GhostStats.MaxHandedErrorCm = FMath::Max(GhostStats.MaxHandedErrorCm, GhostStats.LastErrorCm);
		}
		else
		{
			++GhostStats.Snapped;
		}
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldGhost handover object=%d adopted=%d errorCm=%.2f"), Object->GetObjectId(), bAdopted ? 1 : 0, GhostStats.LastErrorCm);
	}
}

void UGSTN_HeldSubsystem::NoteGhostExpired(const UGSTN_HeldObjectComponent& Object)
{
	++GhostStats.Expired;
	UE_LOG(LogGSTNHeld, Log, TEXT("HeldGhost expired object=%d"), Object.GetObjectId());
}

bool UGSTN_HeldSubsystem::Rebound(UGSTN_HeldObjectComponent& Object, const FVector& From, const FVector& Velocity, int32 Frame, const TArray<int32>& ThrowHitTargets)
{
	UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld());
	if (!Projectiles || !Projectiles->CanFire())
	{
		return false;
	}
	// The same shot as the throw (hit, shape, gravity, source), from the contact along the rebound, credited to the thrower
	FGSTN_ProjectileParams Params = Object.GetTuning().Throw;
	Params.SourceTag = GSTNCombatHitTags::Source_Thrown;
	Params.Origin = From;
	Params.Direction = Velocity.GetSafeNormal();
	Params.Speed = Velocity.Size();
	AActor* Thrower = Object.Thrower.Get();
	Params.OwnerEntityId = Thrower ? Projectiles->FindEntityIdOf(Thrower) : INDEX_NONE;
	AGSTN_Projectile* Shot = Projectiles->FireProjectileAtFrame(Params, Frame + 1, Thrower);
	if (!Shot)
	{
		return false;
	}
	if (Object.GetTuning().bOneHitPerTargetPerThrow && !ThrowHitTargets.IsEmpty())
	{
		Projectiles->SeedHitTargets(Shot, ThrowHitTargets);
	}
	++Object.Bounces;
	Object.SetFlying(*Shot);
	UE_LOG(LogGSTNHeld, Log, TEXT("HeldImpact object=%d behaviour=Bounce frame=%d bounce=%d speed=%.0f carried=%d"), Object.GetObjectId(), Frame, Object.Bounces, Params.Speed, ThrowHitTargets.Num());
	return true;
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
			// M10 G6: the projected size class (index of the Carrying.<size> tag, -1 = none) and the walk speed the carry load leaves
			int32 SizeIndex = INDEX_NONE;
			const TArray<FGSTN_HeldSizeClass>& SizeClasses = UGSTN_HeldRules::Get().SizeClasses;
			for (int32 Index = 0; AbilitySystem && Index < SizeClasses.Num(); ++Index)
			{
				if (SizeClasses[Index].CarryingTag.IsValid() && AbilitySystem->HasMatchingGameplayTag(SizeClasses[Index].CarryingTag))
				{
					SizeIndex = Index;
				}
			}
			bool bFound = false;
			const float Walk = AbilitySystem ? AbilitySystem->GetGameplayAttributeValue(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), bFound) : 0.f;
			// The carry load effects active on this entity and the first one's evaluated walk modifier (diagnosis of the load's removal)
			int32 Loads = 0;
			float LoadMagnitude = -1.f;
			if (AbilitySystem)
			{
				FGameplayEffectQuery Query;
				Query.EffectDefinition = UGSTN_GE_HoldLoad::StaticClass();
				const TArray<FActiveGameplayEffectHandle> Handles = AbilitySystem->GetActiveEffects(Query);
				Loads = Handles.Num();
				const FActiveGameplayEffect* First = Loads > 0 ? AbilitySystem->GetActiveGameplayEffect(Handles[0]) : nullptr;
				LoadMagnitude = First && First->Spec.Modifiers.Num() > 0 ? First->Spec.Modifiers[0].GetEvaluatedMagnitude() : -1.f;
			}
			const float WalkBase = AbilitySystem ? AbilitySystem->GetNumericAttributeBase(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute()) : -1.f;
			UE_LOG(LogGSTNHeld, Log, TEXT("HeldHandReport world=%s entity=%d carrying=%d target=%d size=%d walk=%.1f base=%.1f loads=%d loadMag=%.3f"), *GetNameSafe(GetWorld()), Id,
				AbilitySystem && AbilitySystem->HasMatchingGameplayTag(GSTNHeldTags::Held_Carrying) ? 1 : 0,
				AbilitySystem && AbilitySystem->HasMatchingGameplayTag(GSTNHeldTags::Interact_Target_Pickup) ? 1 : 0, SizeIndex, bFound ? Walk : -1.f,
				WalkBase, Loads, LoadMagnitude);
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

	void Spawn(const TArray<FString>& InArgs, UWorld* InWorld)
	{
		// Slice 10: a "Windup=N" anywhere gives the object its own wind-up of N frames (host-only definition, as the size)
		TArray<FString> Args;
		int32 WindupFrames = INDEX_NONE;
		for (const FString& Arg : InArgs)
		{
			if (Arg.StartsWith(TEXT("Windup="), ESearchCase::IgnoreCase))
			{
				WindupFrames = FMath::Max(FCString::Atoi(*Arg.Mid(7)), 0);
				continue;
			}
			Args.Add(Arg);
		}
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
		// M10 G6: an optional size class (Light, Medium, Heavy, Huge) and weight, on a host-only transient definition (clients keep the test tuning:
		// the size and the weight are read by the host alone, into the hold's payload and the throw)
		if (Actor && Actor->GetHeldObject() && Args.IsValidIndex(2))
		{
			UGSTN_HeldObjectDefinition* Definition = NewObject<UGSTN_HeldObjectDefinition>(Actor);
			Definition->Tuning.SizeTag = FGameplayTag::RequestGameplayTag(FName(*(TEXT("Combat.Held.Size.") + Args[2])), false);
			if (Args.IsValidIndex(3))
			{
				Definition->Tuning.WeightKg = FCString::Atof(*Args[3]);
			}
			// M10 G7: an optional impact behaviour by name (Land, Break, Bounce, Stick)
			if (Args.IsValidIndex(4))
			{
				const int64 Value = StaticEnum<EGSTN_HeldImpactBehaviour>()->GetValueByNameString(Args[4]);
				Definition->Tuning.ImpactBehaviour = Value == INDEX_NONE ? EGSTN_HeldImpactBehaviour::Land : static_cast<EGSTN_HeldImpactBehaviour>(Value);
			}
			Actor->GetHeldObject()->Definition = Definition;
		}
		if (Actor && Actor->GetHeldObject() && WindupFrames != INDEX_NONE)
		{
			UGSTN_HeldObjectDefinition* Definition = Actor->GetHeldObject()->Definition;
			if (!Definition)
			{
				Definition = NewObject<UGSTN_HeldObjectDefinition>(Actor);
				Actor->GetHeldObject()->Definition = Definition;
			}
			Definition->Tuning.bOverrideWindup = true;
			Definition->Tuning.Windup = UGSTN_HeldRules::Get().DefaultThrowWindup;
			Definition->Tuning.Windup.ReleaseFrames = WindupFrames;
		}
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

	/**
	 * gstn.Held.GhostThrow, run in a client world of a one-process PIE whose pawn holds an object: the harness stand-in for a predicted throw
	 * (M10 G8), as gstn.Projectile.GhostFire is for a shot. The client draws the throw's ghost at its next frame and the object at it; the
	 * host throws the same pawn's object with the same shot starting at that frame. Arg: [PitchDeg = the throw ability's LaunchPitchDeg]
	 */
	void GhostThrow(const TArray<FString>& Args, UWorld* World)
	{
		UGSTN_HeldSubsystem* ClientHeld = UGSTN_HeldSubsystem::Get(World);
		UGSTN_ProjectileSubsystem* Client = UGSTN_ProjectileSubsystem::Get(World);
		UWorld* ServerWorld = FindAuthorityWorld(nullptr);
		UGSTN_HeldSubsystem* ServerHeld = UGSTN_HeldSubsystem::Get(ServerWorld);
		UGSTN_ProjectileSubsystem* Server = UGSTN_ProjectileSubsystem::Get(ServerWorld);
		const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!ClientHeld || !Client || !ServerHeld || !Server || World == ServerWorld || !Pawn || !Pawn->GetPlayerState())
		{
			UE_LOG(LogGSTNHeld, Warning, TEXT("gstn.Held.GhostThrow: run it in a client world of a one-process PIE with a pawn"));
			return;
		}
		APawn* ServerPawn = nullptr;
		for (FConstPlayerControllerIterator It = ServerWorld->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* ServerController = It->Get();
			if (ServerController && ServerController->PlayerState && ServerController->PlayerState->GetPlayerId() == Pawn->GetPlayerState()->GetPlayerId())
			{
				ServerPawn = ServerController->GetPawn();
			}
		}
		const UGSTN_HeldObjectComponent* ClientObject = ClientHeld->FindHeldBy(Pawn);
		const UGSTN_HeldObjectComponent* ServerObject = ServerPawn ? ServerHeld->FindHeldBy(ServerPawn) : nullptr;
		if (!ClientObject || !ServerObject)
		{
			UE_LOG(LogGSTNHeld, Warning, TEXT("gstn.Held.GhostThrow: the pawn holds nothing (client %d, host %d)"), ClientObject ? 1 : 0, ServerObject ? 1 : 0);
			return;
		}
		const UGSTN_GA_HeldThrow* Defaults = GetDefault<UGSTN_GA_HeldThrow>();
		const float Pitch = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : Defaults->LaunchPitchDeg;
		// The same shot as the throw ability's (the object's tuning, the size class speed, from the muzzle along the facing raised by Pitch)
		auto MakeShot = [Defaults, Pitch](const APawn& From, const UGSTN_HeldObjectComponent& Object)
		{
			FGSTN_ProjectileParams Params = Object.GetTuning().Throw;
			if (const FGSTN_HeldSizeClass* SizeClass = UGSTN_HeldRules::Get().FindSizeClass(Object.GetTuning().SizeTag))
			{
				Params.Speed *= SizeClass->ThrowSpeedScale;
			}
			Params.SourceTag = GSTNCombatHitTags::Source_Thrown;
			Params.Origin = From.GetActorTransform().TransformPosition(Defaults->MuzzleOffset);
			FRotator Launch = From.GetActorForwardVector().Rotation();
			Launch.Pitch = FMath::Clamp(Launch.Pitch + Pitch, -89.0, 89.0);
			Params.Direction = Launch.Vector();
			return Params;
		};
		const int32 StartFrame = Client->GetLastFrame() + 1;
		AGSTN_Projectile* Ghost = Client->SpawnGhost(MakeShot(*Pawn, *ClientObject), Pawn, StartFrame);
		if (Ghost)
		{
			ClientHeld->NotePredictedThrow(Pawn, *Ghost);
		}
		FGSTN_ProjectileParams HostShot = MakeShot(*ServerPawn, *ServerObject);
		HostShot.OwnerEntityId = Server->FindEntityIdOf(ServerPawn);
		AGSTN_Projectile* Fired = Server->FireProjectileAtFrame(HostShot, StartFrame, ServerPawn);
		if (Fired)
		{
			ServerHeld->NoteThrown(ServerPawn, *Fired);
		}
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldGhostThrow player=%d start=%d ghost=%d fired=%d"), Pawn->GetPlayerState()->GetPlayerId(), StartFrame,
			Ghost ? 1 : 0, Fired ? 1 : 0);
	}

	void GhostReport(const TArray<FString>& Args, UWorld* World)
	{
		if (const UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(World))
		{
			const UGSTN_HeldSubsystem::FGhostStats& Stats = Held->GetGhostStats();
			UE_LOG(LogGSTNHeld, Log, TEXT("HeldGhostReport world=%s predicted=%d handed=%d snapped=%d expired=%d lastErrorCm=%.2f maxHandedErrorCm=%.2f"),
				*World->GetName(), Stats.Predicted, Stats.Handed, Stats.Snapped, Stats.Expired, Stats.LastErrorCm, Stats.MaxHandedErrorCm);
		}
	}

	void Windup(const TArray<FString>& Args, UWorld* InWorld)
	{
		// Dev, one process (PIE): the settings object is shared by every world of the process, so host and clients read the same value. Never
		// across processes: two machines with different wind-ups would desync the throw
		UGSTN_HeldRules* Settings = &FGSTN_ActiveGameRules::GetMutablePieceForDev<UGSTN_HeldRules>();
		if (Args.IsValidIndex(0))
		{
			Settings->DefaultThrowWindup.ReleaseFrames = FMath::Max(FCString::Atoi(*Args[0]), 0);
			Settings->ThrownEnemies.Windup.ReleaseFrames = Args.IsValidIndex(1) ? FMath::Max(FCString::Atoi(*Args[1]), 0) : Settings->DefaultThrowWindup.ReleaseFrames;
			if (Settings->ThrownEnemies.Windup.Montage.IsNull())
			{
				Settings->ThrownEnemies.Windup.Montage = Settings->DefaultThrowWindup.Montage;
			}
		}
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldWindupSettings objects=%d enemies=%d montage=%s"), Settings->DefaultThrowWindup.ReleaseFrames,
			Settings->ThrownEnemies.Windup.ReleaseFrames, *Settings->DefaultThrowWindup.Montage.ToString());
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
		TEXT("Host: spawns a test held object in front of a combat pawn (pawns by ascending entity id), on the floor. Args: [Pawn=0] [ForwardCm=120] [Size=Light|Medium|Heavy|Huge] [WeightKg] [Impact=Land|Break|Bounce|Stick|Explode]"),
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
	FAutoConsoleCommandWithWorldAndArgs GHeldGhostThrowCommand(
		TEXT("gstn.Held.GhostThrow"),
		TEXT("Client world of a one-process PIE: stands in for a predicted throw of what the local pawn holds (ghost here, the same throw on the host at that frame). Args: [PitchDeg]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GhostThrow));
	FAutoConsoleCommandWithWorldAndArgs GHeldGhostReportCommand(
		TEXT("gstn.Held.GhostReport"),
		TEXT("Logs what became of this world's object ghosts: HeldGhostReport predicted handed snapped expired lastErrorCm maxHandedErrorCm"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GhostReport));
	FAutoConsoleCommandWithWorldAndArgs GHeldWindupCommand(
		TEXT("gstn.Held.Windup"),
		TEXT("PIE only (one process): the throw's wind-up of every object without its own, in frames (0 = instant), and of thrown enemies (default: the same). No args: logs them. Args: [ObjectFrames] [EnemyFrames]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Windup));
}

AActor* UGSTN_HeldSubsystem::FindLiftedEnemy(const AActor* Holder, const FGSTN_LinkState& Body, float* OutLoadRatio, FString* OutWhyNot)
{
	const FGSTN_EnemyThrowTuning& Tuning = UGSTN_HeldRules::Get().ThrownEnemies;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	if (!Tuning.bEnabled || !Holder || !Registry || !Body.IsLinked() || Body.Role != 1 || Body.PartnerId.Kind != GSTNEntityKinds::Fighter)
	{
		if (OutWhyNot)
		{
			*OutWhyNot = FString::Printf(TEXT("body linked=%d role=%d type=%d partner=%s"), Body.IsLinked() ? 1 : 0, Body.Role, Body.LinkTypeIndex, *Body.PartnerId.ToString());
		}
		return nullptr;
	}
	bool bLiftableType = false;
	for (const FGameplayTag& Type : Tuning.LiftableLinkTypes)
	{
		const uint8 Index = Registry->GetTypeIndex(Type);
		bLiftableType |= Index != 0 && Index == Body.LinkTypeIndex;
	}
	const UGSTN_CombatWorldSubsystem* Combat = bLiftableType ? UGSTN_CombatWorldSubsystem::Get(Holder->GetWorld()) : nullptr;
	AActor* Enemy = Combat ? Combat->FindOwnerActor(static_cast<int32>(Body.PartnerId.Id)) : nullptr;
	const UAbilitySystemComponent* EnemyAbilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy);
	if (!EnemyAbilities)
	{
		if (OutWhyNot)
		{
			*OutWhyNot = FString::Printf(TEXT("liftable type=%d type=%d partner=%s actor=%s"), bLiftableType ? 1 : 0, Body.LinkTypeIndex, *Body.PartnerId.ToString(), *GetNameSafe(Enemy));
		}
		return nullptr;
	}
	bool bFound = false;
	const float WeightAttribute = EnemyAbilities->GetGameplayAttributeValue(UGSTN_CombatAttributeSet::GetWeightAttribute(), bFound);
	const float Weight = FGSTN_EnemyLiftRules::EnemyWeight(Tuning, bFound ? WeightAttribute : 0.f);
	// The fighters' weight units, not the held objects' kilograms: the enemy capacity's own default
	const float Capacity = FGSTN_HeldLoad::EffectiveCapacity(CarryCapacityOf(Holder), Tuning.DefaultEnemyCarryCapacity);
	if (!FGSTN_EnemyLiftRules::CanLift(Tuning, Weight, Capacity, EnemyAbilities->HasMatchingGameplayTag(GSTNHeldTags::Held_NoLift)))
	{
		if (OutWhyNot)
		{
			*OutWhyNot = FString::Printf(TEXT("too heavy or no-lift: weight=%.1f capacity=%.1f"), Weight, Capacity);
		}
		return nullptr;
	}
	if (OutLoadRatio)
	{
		*OutLoadRatio = Capacity > 0.f ? FMath::Clamp(Weight / Capacity, 0.f, 1.f) : 1.f;
	}
	return Enemy;
}

AActor* UGSTN_HeldSubsystem::FindLiftedEnemy(const AActor* Holder, float* OutLoadRatio, FString* OutWhyNot)
{
	const UGSTN_CombatSimComponent* Sim = Holder ? Holder->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const FGSTN_CombatSyncState* Sync = Sim ? Sim->GetSyncState() : nullptr;
	const FGSTN_LinkFragment* Links = Sync ? FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Sync->Fragments) : nullptr;
	return Links ? FindLiftedEnemy(Holder, Links->Links.Body, OutLoadRatio, OutWhyNot) : nullptr;
}

void UGSTN_HeldSubsystem::NoteEnemyThrown(AActor* Holder, AGSTN_Projectile& Shot)
{
	UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld());
	UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(GetWorld());
	const UGSTN_CombatSimComponent* HolderSim = Holder ? Holder->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const FGSTN_CombatSyncState* HolderSync = HolderSim ? HolderSim->GetSyncState() : nullptr;
	const FGSTN_LinkFragment* Links = HolderSync ? FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(HolderSync->Fragments) : nullptr;
	AActor* Enemy = Links ? FindLiftedEnemy(Holder, Links->Links.Body) : nullptr;
	if (!HasAuthority() || !Projectiles || !Combat || !Enemy)
	{
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldEnemyThrow: %s lifts no enemy, the shot flies alone"), *GetNameSafe(Holder));
		return;
	}
	const int32 HolderId = Projectiles->FindEntityIdOf(Holder);
	const int32 EnemyId = static_cast<int32>(Links->Links.Body.PartnerId.Id);
	const uint8 Serial = Links->Links.Body.Serial;
	// The shot stands for the body: the body counts as already hit by it (PrepareShot added one to MaxHits for it)
	Projectiles->SeedHitTargets(&Shot, { EnemyId });

	const FVector Velocity = Shot.GetSegment().Velocity;
	const int32 RegrabLockFrames = UGSTN_HeldRules::Get().ThrownEnemies.RegrabLockFrames;
	auto PostBreak = [&](AActor* Side, int32 SideId, bool bThrown)
	{
		const UGSTN_CombatSimComponent* Sim = Side->FindComponentByClass<UGSTN_CombatSimComponent>();
		const FGSTN_CombatSyncState* Sync = Sim ? Sim->GetSyncState() : nullptr;
		const int32 Stamp = Sync ? Sync->Core.Input.History.GetLatestFrame() : LastFrame;
		FGSTN_LinkCommandEntry Command;
		Command.Kind = EGSTN_LinkCommandKind::Break;
		Command.Channel = EGSTN_LinkChannel::Body;
		Command.Serial = Serial;
		Command.Reason = static_cast<uint8>(EGSTN_LinkBreakReason::Throw);
		if (bThrown)
		{
			// Leaves the parked grab where it is (the grabber's socket), moving: the exit request launches it on its own movement
			Command.ExitPosition = FVector3f(Side->GetActorLocation());
			Command.ExitYawDeg = static_cast<float>(Side->GetActorRotation().Yaw);
			Command.ExitVelocity = FVector3f(Velocity);
			Command.ExitFlags = GSTNLinkExitFlags::Teleport;
			Command.ImmunityEndFrame = Stamp + 1 + RegrabLockFrames;
		}
		FGSTN_InboxEntry Entry = FGSTN_LinkInboxHandlers::MakeCommand(Command);
		Entry.Frame = Stamp;
		Entry.SenderId = HolderId;
		return Combat->PostToInbox(SideId, Entry);
	};
	const bool bHolderPosted = PostBreak(Holder, HolderId, false);
	const bool bEnemyPosted = PostBreak(Enemy, EnemyId, true);
	UE_LOG(LogGSTNHeld, Log, TEXT("HeldEnemyThrow holder=%d enemy=%d serial=%d speed=%.0f posted=%d%d frame=%d"), HolderId, EnemyId, static_cast<int32>(Serial),
		Velocity.Size(), bHolderPosted ? 1 : 0, bEnemyPosted ? 1 : 0, Shot.GetSegment().StartFrame);
}
