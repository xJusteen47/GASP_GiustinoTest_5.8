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
#include "GSTN_ProjectileActor.h"
#include "GSTN_ProjectileSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "NetworkPredictionWorldManager.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNHeldRuntime);

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
	Objects.Reset();
	Pending.Reset();
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
	ResolvePickups(ServerFrame);
	StepObjects(ServerFrame);
}

void UGSTN_HeldSubsystem::ResolvePickups(int32 Frame)
{
	if (Pending.IsEmpty())
	{
		return;
	}
	const UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(GetWorld());
	const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(GetWorld());

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
		Requests.Add({ Id, Queued.Frame, Entry ? Entry->Transform.GetLocation() : Holder->GetActorLocation() });
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
		UE_LOG(LogGSTNHeld, Log, TEXT("HeldPickup entity %d holds object %d at frame %d"), Won->HolderId, Won->ObjectId, Frame);
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
			Object->SetIdleAt(FloorBelow(Owner->GetActorLocation(), HalfHeight, Owner));
			UE_LOG(LogGSTNHeld, Log, TEXT("HeldObject %d dropped at frame %d (holder gone)"), Object->GetObjectId(), Frame);
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
	FAutoConsoleCommandWithWorldAndArgs GHeldReportCommand(
		TEXT("gstn.Held.Report"),
		TEXT("Logs every held object of the world it runs in (state, holder entity, location): HeldReport / HeldObjectReport lines"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Report));
}
