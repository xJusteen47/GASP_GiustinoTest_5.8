// Projectile, world side (needs the GAS-NPP fork for the fixed tick it reads).

#include "GSTN_ProjectileSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "GSTNProjectileCoreModule.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GSTN_GA_FireProjectile.h"
#include "GSTN_ProjectileActor.h"
#include "HAL/IConsoleManager.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitShapeUtils.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hits/GSTN_RelationshipResolver.h"
#include "Input/GSTN_CombatSettings.h"
#include "NetworkPredictionWorldManager.h"
#include "Rules/GSTN_CombatRules.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_HitInbox.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

namespace
{
	TAutoConsoleVariable<int32> CVarProjectileMaxActive(
		TEXT("gstn.Projectile.MaxActive"), 64,
		TEXT("Most projectiles alive at once per world (review entita-dinamiche §3: a cap, visuals pooled later). Fire is refused beyond it."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarProjectilePredictGhost(
		TEXT("gstn.Projectile.PredictGhost"), 1,
		TEXT("1: the client that fires a projectile ability draws its predicted shot at once as a local ghost, adopted by the host's projectile on arrival. 0: it sees its shot one round trip late."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarProjectileGhostToleranceCm(
		TEXT("gstn.Projectile.GhostToleranceCm"), 50.f,
		TEXT("Largest distance (cm, at the arrival frame) between a ghost and the host's projectile that is adopted with a blend; beyond it the ghost is dropped and the projectile snaps."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarProjectileGhostMatchFrames(
		TEXT("gstn.Projectile.GhostMatchFrames"), 3,
		TEXT("Most start frames apart a ghost and the host's projectile of the same owner may be and still be the same shot."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarProjectileGhostBlendFrames(
		TEXT("gstn.Projectile.GhostBlendFrames"), 6,
		TEXT("Frames over which an adopted ghost's offset fades into the host's projectile (0 = jump at once)."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarProjectileGhostTimeoutFrames(
		TEXT("gstn.Projectile.GhostTimeoutFrames"), 60,
		TEXT("Frames after its start a ghost waits for the host's projectile before it is dropped (a refused or mispredicted shot)."),
		ECVF_Default);

	FVector ShapeCenterOf(const FGSTN_HitShape& Shape, const FTransform& Carrier)
	{
		return (Shape.Local * Carrier).GetLocation();
	}
}

UGSTN_ProjectileSubsystem* UGSTN_ProjectileSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UGSTN_ProjectileSubsystem>() : nullptr;
}

bool UGSTN_ProjectileSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_ProjectileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UNetworkPredictionWorldManager* Manager = Collection.InitializeDependency<UNetworkPredictionWorldManager>())
	{
		PostFixedTickHandle = Manager->OnPostFixedTick.AddUObject(this, &UGSTN_ProjectileSubsystem::HandlePostFixedTick);
	}
}

void UGSTN_ProjectileSubsystem::Deinitialize()
{
	if (PostFixedTickHandle.IsValid())
	{
		if (UNetworkPredictionWorldManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr)
		{
			Manager->OnPostFixedTick.Remove(PostFixedTickHandle);
		}
		PostFixedTickHandle.Reset();
	}
	Active.Reset();
	Super::Deinitialize();
}

bool UGSTN_ProjectileSubsystem::HasAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

void UGSTN_ProjectileSubsystem::HandlePostFixedTick(int32 ServerFrame, int32 EndTimeMS, bool bResimulating)
{
	if (const UNetworkPredictionWorldManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr)
	{
		const int32 StepMs = Manager->GetFixedTickState().FixedStepMS;
		if (StepMs > 0)
		{
			StepSeconds = static_cast<double>(StepMs) / 1000.0;
		}
	}
	LastFrame = ServerFrame;
	// The host never resimulates; a client draws only, from LastFrame
	if (!bResimulating && HasAuthority())
	{
		StepFrame(ServerFrame);
	}
	else if (!HasAuthority())
	{
		ExpireGhosts(ServerFrame);
	}
}

int32 UGSTN_ProjectileSubsystem::FindEntityIdOf(const AActor* Actor) const
{
	const UGSTN_CombatWorldSubsystem* Combat = Actor ? UGSTN_CombatWorldSubsystem::Get(GetWorld()) : nullptr;
	if (!Combat)
	{
		return INDEX_NONE;
	}
	TArray<int32> Ids;
	Combat->GetRegisteredEntityIds(Ids);
	for (const int32 Id : Ids)
	{
		if (Combat->FindOwnerActor(Id) == Actor)
		{
			return Id;
		}
	}
	return INDEX_NONE;
}

AGSTN_Projectile* UGSTN_ProjectileSubsystem::FireProjectile(const FGSTN_ProjectileParams& InParams)
{
	return FireProjectileAtFrame(InParams, INDEX_NONE, nullptr);
}

AGSTN_Projectile* UGSTN_ProjectileSubsystem::FireProjectileAtFrame(const FGSTN_ProjectileParams& InParams, int32 StartFrame, AActor* OwnerActor)
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		UE_LOG(LogGSTNProjectile, Warning, TEXT("FireProjectile: host only"));
		return nullptr;
	}
	if (LastFrame == INDEX_NONE)
	{
		UE_LOG(LogGSTNProjectile, Warning, TEXT("FireProjectile: no fixed tick yet in this world"));
		return nullptr;
	}
	if (Active.Num() >= CVarProjectileMaxActive.GetValueOnGameThread())
	{
		UE_LOG(LogGSTNProjectile, Warning, TEXT("FireProjectile: %d projectiles alive, refused (gstn.Projectile.MaxActive)"), Active.Num());
		return nullptr;
	}

	FGSTN_ProjectileParams Params = InParams;
	Params.LifetimeFrames = FMath::Max(Params.LifetimeFrames, 1);
	Params.MaxHits = FMath::Max<uint8>(Params.MaxHits, 1);
	if (Params.OwnerTeamId == 255 && Params.OwnerEntityId != INDEX_NONE)
	{
		const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World);
		if (const FGSTN_SnapshotEntry* Owner = Combat ? Combat->GetSnapshot().Find(Params.OwnerEntityId) : nullptr)
		{
			Params.OwnerTeamId = Owner->Team.GetId();
		}
	}

	if (!OwnerActor && Params.OwnerEntityId != INDEX_NONE)
	{
		if (const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World))
		{
			OwnerActor = Combat->FindOwnerActor(Params.OwnerEntityId);
		}
	}

	// Starts at the next frame: the first sweep is LastFrame + 1 -> LastFrame + 2, the same frame numbers on every machine. Quantized
	// first: the host sweeps exactly the segment the clients decode (FGSTN_BallisticSegment::NetSerialize)
	const int32 First = StartFrame == INDEX_NONE ? LastFrame + 1 : StartFrame;
	const FGSTN_BallisticSegment Segment = FGSTN_BallisticMode::Quantize(FGSTN_BallisticMode::MakeSegment(Params, First, StepSeconds));
	const FTransform SpawnTransform = FGSTN_BallisticMode::CarrierAt(Segment, Segment.StartFrame);
	// The shooter as Owner: one object reference in the spawn bunch, what the firing client matches its ghost with
	AGSTN_Projectile* Actor = World->SpawnActorDeferred<AGSTN_Projectile>(AGSTN_Projectile::StaticClass(), SpawnTransform, OwnerActor,
		Cast<APawn>(OwnerActor), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->InitProjectile(Segment, Params.LifetimeFrames, static_cast<float>(Params.Shape.Extent.X));
	Actor->FinishSpawning(SpawnTransform);
	// Sent in this frame's net tick, not at the actor's next scheduled update
	Actor->ForceNetUpdate();

	FGSTN_ActiveProjectile& Projectile = Active.AddDefaulted_GetRef();
	Projectile.Actor = Actor;
	Projectile.Params = Params;
	Projectile.Segment = Segment;
	Projectile.Serial = NextSerial++;
	UE_LOG(LogGSTNProjectile, Log, TEXT("Projectile %u fired by %d at frame %d from %s, v %s, life %d"), Projectile.Serial, Params.OwnerEntityId,
		Segment.StartFrame, *Segment.Start.ToCompactString(), *Segment.Velocity.ToCompactString(), Params.LifetimeFrames);
	return Actor;
}

AGSTN_Projectile* UGSTN_ProjectileSubsystem::SpawnGhost(const FGSTN_ProjectileParams& InParams, AActor* OwnerActor, int32 StartFrame)
{
	UWorld* World = GetWorld();
	if (!World || HasAuthority() || !OwnerActor || LastFrame == INDEX_NONE || CVarProjectilePredictGhost.GetValueOnGameThread() == 0)
	{
		return nullptr;
	}
	const int32 First = StartFrame == INDEX_NONE ? LastFrame + 1 : StartFrame;
	// A resimulated activation fires again at the same frame: the ghost of that shot exists already
	for (const FGSTN_PendingGhost& Ghost : Ghosts)
	{
		if (Ghost.StartFrame == First && Ghost.Owner.Get() == OwnerActor && IsValid(Ghost.Actor))
		{
			return Ghost.Actor;
		}
	}

	FGSTN_ProjectileParams Params = InParams;
	Params.LifetimeFrames = FMath::Max(Params.LifetimeFrames, 1);
	const FGSTN_BallisticSegment Segment = FGSTN_BallisticMode::Quantize(FGSTN_BallisticMode::MakeSegment(Params, First, StepSeconds));
	const FTransform SpawnTransform = FGSTN_BallisticMode::CarrierAt(Segment, Segment.StartFrame);
	AGSTN_Projectile* Actor = World->SpawnActorDeferred<AGSTN_Projectile>(AGSTN_Projectile::StaticClass(), SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->InitGhost(Segment, Params.LifetimeFrames, static_cast<float>(Params.Shape.Extent.X));
	Actor->FinishSpawning(SpawnTransform);

	FGSTN_PendingGhost& Ghost = Ghosts.AddDefaulted_GetRef();
	Ghost.Actor = Actor;
	Ghost.Owner = OwnerActor;
	Ghost.StartFrame = First;
	++GhostStats.Spawned;
	UE_LOG(LogGSTNProjectile, Log, TEXT("Projectile ghost spawned for %s at frame %d (last frame %d)"), *OwnerActor->GetName(), First, LastFrame);
	return Actor;
}

void UGSTN_ProjectileSubsystem::ReconcileGhostFor(AGSTN_Projectile& Authority)
{
	const AActor* Owner = Authority.GetOwner();
	if (!Owner || Ghosts.IsEmpty() || LastFrame == INDEX_NONE)
	{
		return;
	}
	const int32 MaxFrameDelta = CVarProjectileGhostMatchFrames.GetValueOnGameThread();
	const double Tolerance = CVarProjectileGhostToleranceCm.GetValueOnGameThread();
	int32 Best = INDEX_NONE;
	double BestError = 0.0;
	FGSTN_ProjectileGhostReconcile::EResult BestResult = FGSTN_ProjectileGhostReconcile::EResult::NoMatch;
	for (int32 Index = 0; Index < Ghosts.Num(); ++Index)
	{
		const FGSTN_PendingGhost& Ghost = Ghosts[Index];
		if (Ghost.Owner.Get() != Owner || !IsValid(Ghost.Actor))
		{
			continue;
		}
		double Error = 0.0;
		const FGSTN_ProjectileGhostReconcile::EResult Result = FGSTN_ProjectileGhostReconcile::Decide(Ghost.Actor->GetSegment(), Authority.GetSegment(),
			LastFrame, MaxFrameDelta, Tolerance, Error);
		if (Result != FGSTN_ProjectileGhostReconcile::EResult::NoMatch && (Best == INDEX_NONE || Error < BestError))
		{
			Best = Index;
			BestError = Error;
			BestResult = Result;
		}
	}
	if (Best == INDEX_NONE)
	{
		return;
	}

	AGSTN_Projectile* Ghost = Ghosts[Best].Actor;
	const int32 GhostStart = Ghosts[Best].StartFrame;
	GhostStats.LastErrorCm = BestError;
	GhostStats.LastArrivalFrames = LastFrame - GhostStart;
	if (BestResult == FGSTN_ProjectileGhostReconcile::EResult::Adopt)
	{
		// Drawn where the ghost was this frame, then fading into its own path: no pop for a good prediction
		Authority.StartBlend(Ghost->GetDrawnPositionAt(LastFrame) - FGSTN_BallisticMode::PositionAt(Authority.GetSegment(), LastFrame), LastFrame,
			CVarProjectileGhostBlendFrames.GetValueOnGameThread());
		++GhostStats.Adopted;
		GhostStats.MaxAdoptedErrorCm = FMath::Max(GhostStats.MaxAdoptedErrorCm, BestError);
	}
	else
	{
		++GhostStats.Snapped;
	}
	UE_LOG(LogGSTNProjectile, Log, TEXT("Projectile ghost %s for %s: ghost start %d, host start %d, frame %d, error %.2f cm"),
		BestResult == FGSTN_ProjectileGhostReconcile::EResult::Adopt ? TEXT("adopted") : TEXT("snapped"), *Owner->GetName(), GhostStart,
		Authority.GetSegment().StartFrame, LastFrame, BestError);
	Ghost->Destroy();
	Ghosts.RemoveAt(Best);
}

void UGSTN_ProjectileSubsystem::ExpireGhosts(int32 Frame)
{
	const int32 Timeout = FMath::Max(CVarProjectileGhostTimeoutFrames.GetValueOnGameThread(), 1);
	for (int32 Index = Ghosts.Num() - 1; Index >= 0; --Index)
	{
		const FGSTN_PendingGhost& Ghost = Ghosts[Index];
		if (IsValid(Ghost.Actor) && Ghost.Owner.IsValid() && Frame <= Ghost.StartFrame + Timeout)
		{
			continue;
		}
		if (IsValid(Ghost.Actor))
		{
			Ghost.Actor->Destroy();
		}
		++GhostStats.Expired;
		UE_LOG(LogGSTNProjectile, Log, TEXT("Projectile ghost expired: start %d, frame %d, no host projectile"), Ghost.StartFrame, Frame);
		Ghosts.RemoveAt(Index);
	}
}

void UGSTN_ProjectileSubsystem::StepFrame(int32 Frame)
{
	if (Active.IsEmpty())
	{
		return;
	}
	UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(GetWorld());
	const FGSTN_StepSnapshot* Snapshot = nullptr;
	if (Combat && Combat->GetNumRegisteredEntities() > 0)
	{
		// The combat ticks of Frame built S(Frame) already (this hook runs after every model); without one, build it as they would
		Snapshot = Combat->GetSnapshot().Frame == Frame ? &Combat->GetSnapshot() : &Combat->EnsureSnapshot(Frame);
	}

	// Active is in spawn order (serials ascend), so hits of one frame are posted in the same order on every run
	for (int32 Index = 0; Index < Active.Num();)
	{
		FGSTN_ActiveProjectile& Projectile = Active[Index];
		const int32 EndFrame = Projectile.Segment.StartFrame + Projectile.Params.LifetimeFrames;
		bool bRemove = !IsValid(Projectile.Actor) || Frame > EndFrame;
		if (!bRemove && Frame > Projectile.Segment.StartFrame && Snapshot)
		{
			bRemove = StepProjectile(Projectile, Frame, *Snapshot, *Combat);
		}
		if (bRemove)
		{
			if (IsValid(Projectile.Actor))
			{
				Projectile.Actor->Destroy();
			}
			Active.RemoveAt(Index);
			continue;
		}
		++Index;
	}
}

bool UGSTN_ProjectileSubsystem::StepProjectile(FGSTN_ActiveProjectile& Projectile, int32 Frame, const FGSTN_StepSnapshot& Snapshot, UGSTN_CombatWorldSubsystem& Combat)
{
	const FGSTN_ProjectileParams& Params = Projectile.Params;
	const FGSTN_HitShape& Shape = Params.Shape;
	const FTransform Previous = FGSTN_BallisticMode::CarrierAt(Projectile.Segment, Frame - 1);
	const FTransform Current = FGSTN_BallisticMode::CarrierAt(Projectile.Segment, Frame);
	const FVector Center = ShapeCenterOf(Shape, Current);
	const FVector Velocity = FGSTN_BallisticMode::VelocityAt(Projectile.Segment, Frame);
	const FVector Facing = Velocity.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

	FBox Bounds(ForceInit);
	Bounds += ShapeCenterOf(Shape, Previous);
	Bounds += Center;
	Bounds = Bounds.ExpandBy(Shape.Extent.GetMax() + Shape.Local.GetLocation().Size());
	CandidateScratch.Reset();
	Snapshot.QueryCandidates(Bounds, CandidateScratch);

	const UGSTN_CombatRules* Rules = UGSTN_CombatSettings::Get().ResolveDefaultRules();
	TArray<int32, TInlineAllocator<8>> Overlapped;
	// Candidates come by ascending entity id: the order hits are posted in
	for (const int32 EntryIndex : CandidateScratch)
	{
		const FGSTN_SnapshotEntry& Victim = Snapshot.Entries[EntryIndex];
		if (Victim.EntityId == Params.OwnerEntityId || Projectile.HitTargets.Contains(Victim.EntityId) || !Victim.Hurtboxes || Victim.Hurtboxes->Hurtboxes.IsEmpty())
		{
			continue;
		}

		// Relationship and the rules' hittable list, exactly as a melee attacker decides (no hitbox variants on a projectile yet)
		FGSTN_RelationshipQuery Query;
		Query.AttackerEntityId = Params.OwnerEntityId;
		Query.AttackerTeamId = Params.OwnerTeamId;
		Query.VictimEntityId = Victim.EntityId;
		Query.VictimTeamId = Victim.Team.GetId();
		const FGameplayTag Relationship = Rules ? Rules->ResolveRelationship(Query) : UGSTN_RelationshipResolver::DefaultRule(Query);
		if (Rules ? !Rules->IsRelationshipHittable(Relationship)
			: (Relationship != GSTNCombatHitTags::Relationship_Enemy.GetTag() && Relationship != GSTNCombatHitTags::Relationship_Neutral.GetTag()))
		{
			continue;
		}

		// The same swept hurtbox test, opening rule and pick as a melee hitbox with bSweepBetweenFrames
		const TArray<FGSTN_Hurtbox>& Hurtboxes = Victim.Hurtboxes->Hurtboxes;
		Overlapped.Reset();
		for (int32 HurtIndex = 0; HurtIndex < Hurtboxes.Num(); ++HurtIndex)
		{
			if (Victim.HurtStates.IsValidIndex(HurtIndex) && Victim.HurtStates[HurtIndex].State == EGSTN_HurtState::Intangible)
			{
				continue;
			}
			if (UGSTN_HitShapeUtils::SweptOverlap(Shape, Previous, Current, Hurtboxes[HurtIndex].Shape, Victim.Transform)
				&& (!Hurtboxes[HurtIndex].Opening.bEnabled || UGSTN_HitShapeUtils::PassesOpening(Hurtboxes[HurtIndex].Opening, Shape, Previous, Current, true, Victim.Transform)))
			{
				Overlapped.Add(HurtIndex);
			}
		}
		for (int32 ExtraPosition = 0; ExtraPosition < Victim.ExtraHurtboxes.Num(); ++ExtraPosition)
		{
			if (UGSTN_HitShapeUtils::SweptOverlap(Shape, Previous, Current, Victim.ExtraHurtboxes[ExtraPosition].Shape, Victim.Transform))
			{
				Overlapped.Add(GSTNHurtboxIndex::FromExtraPosition(ExtraPosition));
			}
		}
		if (Overlapped.IsEmpty())
		{
			continue;
		}
		const int32 Picked = UGSTN_HitShapeUtils::PickHurtboxIndex(Overlapped, Hurtboxes, Victim.ExtraHurtboxes, Params.Hit.HitType);
		const FGSTN_HitShape* PickedShape = Victim.FindHurtboxShape(Picked);
		if (Picked == INDEX_NONE || !PickedShape)
		{
			continue;
		}

		FGSTN_HitInboxEntry Entry;
		Entry.Frame = Frame;
		Entry.AttackerId = Params.OwnerEntityId;
		Entry.AttackerTeam = Params.OwnerTeamId;
		Entry.Hit = Params.Hit;
		Entry.HitLocation = (Center + ShapeCenterOf(*PickedShape, Victim.Transform)) * 0.5;
		Entry.HitboxVelocity = Velocity;
		Entry.AttackerYaw = static_cast<float>(Facing.Rotation().Yaw);
		Entry.Relationship = GSTNHitRelationship::ToIndex(Relationship);
		Entry.HurtboxIndex = static_cast<uint8>(Picked);
		Entry.AttackerHitboxIndex = 0;
		Entry.HurtState = static_cast<uint8>(Victim.HurtStates.IsValidIndex(Picked) ? Victim.HurtStates[Picked].State : EGSTN_HurtState::Normal);
		// Into the victim's inbox (fighter or provider), stamped F: drained at F + 1, the minion attackers' path
		if (Combat.PostHit(Victim.EntityId, Entry))
		{
			Projectile.HitTargets.Add(Victim.EntityId);
			UE_LOG(LogGSTNProjectile, Log, TEXT("Projectile %u hit %d at frame %d (hurtbox %d)"), Projectile.Serial, Victim.EntityId, Frame, Picked);
			OnProjectileHit.Broadcast(Projectile.Serial, Victim.EntityId, Frame, Entry);
			if (Projectile.HitTargets.Num() >= Params.MaxHits)
			{
				return true;
			}
		}
	}
	return false;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// gstn.Projectile.Fire [Speed] [GravityZ] [LifetimeFrames] [Damage]

namespace
{
	/** The world that owns the projectiles: this one on a host, the server world of the same process in a one-process PIE */
	UWorld* FindAuthorityWorld(UWorld* World)
	{
		if (World && World->GetNetMode() != NM_Client)
		{
			return World;
		}
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* Candidate = Context.World();
				if (Candidate && Candidate->IsGameWorld() && Candidate->GetNetMode() != NM_Client)
				{
					return Candidate;
				}
			}
		}
		return nullptr;
	}

	void FireFromConsole(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = FindAuthorityWorld(InWorld);
		UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(World);
		if (!Projectiles)
		{
			UE_LOG(LogGSTNProjectile, Warning, TEXT("gstn.Projectile.Fire: no host world with projectiles in this process"));
			return;
		}
		const APlayerController* Controller = World->GetFirstPlayerController();
		const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Pawn)
		{
			UE_LOG(LogGSTNProjectile, Warning, TEXT("gstn.Projectile.Fire: the host has no player pawn to fire from"));
			return;
		}

		FGSTN_ProjectileParams Params;
		Params.Speed = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 1500.f;
		Params.GravityAccel = FVector(0.0, 0.0, Args.IsValidIndex(1) ? FCString::Atod(*Args[1]) : 0.0);
		Params.LifetimeFrames = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : 120;
		Params.Shape.Type = EGSTN_ShapeType::Sphere;
		Params.Shape.Extent = FVector(15.0);
		Params.Hit.Damage = Args.IsValidIndex(3) ? FCString::Atof(*Args[3]) : 8.f;
		Params.Hit.BaseKnockback = 30.f;
		Params.Hit.AngleDeg = 30.f;
		Params.Direction = Pawn->GetActorForwardVector();
		// Clear of the shooter's own capsule; the owner is never hit anyway
		Params.Origin = Pawn->GetActorLocation() + Params.Direction * 80.0;

		Params.OwnerEntityId = Projectiles->FindEntityIdOf(Pawn);
		Projectiles->FireProjectile(Params);
	}

	/**
	 * The ability path the harness drives: the host grants the ServerOnly fire ability to the shooter pawn (once) and triggers it with
	 * the target pawn as event target, so the shot goes ability -> FireProjectile -> host sweep -> PostHit -> victim drain. Pawns are
	 * numbered by ascending combat entity id. Speed, gravity and arc are written on the shooter's ability instance: a dev command only
	 */
	void FireAbilityFromConsole(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = FindAuthorityWorld(InWorld);
		UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(World);
		const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World);
		if (!Projectiles || !Combat)
		{
			UE_LOG(LogGSTNProjectile, Warning, TEXT("gstn.Projectile.Ability: no host world with projectiles and combat in this process"));
			return;
		}
		TArray<int32> Ids;
		Combat->GetRegisteredEntityIds(Ids);
		Ids.Sort();
		TArray<APawn*> Pawns;
		TArray<int32> PawnIds;
		for (const int32 Id : Ids)
		{
			if (APawn* Pawn = Cast<APawn>(Combat->FindOwnerActor(Id)))
			{
				Pawns.Add(Pawn);
				PawnIds.Add(Id);
			}
		}
		const int32 ShooterIndex = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
		const int32 TargetIndex = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 1;
		if (!Pawns.IsValidIndex(ShooterIndex) || !Pawns.IsValidIndex(TargetIndex) || ShooterIndex == TargetIndex)
		{
			UE_LOG(LogGSTNProjectile, Warning, TEXT("gstn.Projectile.Ability: shooter %d / target %d not among %d combat pawns"), ShooterIndex, TargetIndex, Pawns.Num());
			return;
		}
		UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawns[ShooterIndex]);
		if (!AbilitySystem)
		{
			UE_LOG(LogGSTNProjectile, Warning, TEXT("gstn.Projectile.Ability: the shooter has no ability system"));
			return;
		}
		FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(UGSTN_GA_FireProjectileServer::StaticClass());
		if (!Spec)
		{
			AbilitySystem->GiveAbility(FGameplayAbilitySpec(UGSTN_GA_FireProjectileServer::StaticClass(), 1));
			Spec = AbilitySystem->FindAbilitySpecFromClass(UGSTN_GA_FireProjectileServer::StaticClass());
		}
		UGSTN_GA_FireProjectile* Instance = Spec ? Cast<UGSTN_GA_FireProjectile>(Spec->GetPrimaryInstance()) : nullptr;
		if (!Instance)
		{
			UE_LOG(LogGSTNProjectile, Warning, TEXT("gstn.Projectile.Ability: no ability instance after the grant"));
			return;
		}
		Instance->Aim = EGSTN_ProjectileAim::EventTarget;
		Instance->Projectile.Speed = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 1500.f;
		Instance->Projectile.GravityAccel = FVector(0.0, 0.0, Args.IsValidIndex(3) ? FCString::Atod(*Args[3]) : 0.0);
		Instance->bHighArc = Args.IsValidIndex(4) && FCString::Atoi(*Args[4]) != 0;
		// A lob at a near target flies longer than the default 120 frames: the template's lifetime is never stretched to the solved flight
		Instance->Projectile.LifetimeFrames = Args.IsValidIndex(5) ? FMath::Max(FCString::Atoi(*Args[5]), 1) : 120;

		FGameplayEventData Payload;
		Payload.Instigator = Pawns[ShooterIndex];
		Payload.Target = Pawns[TargetIndex];
		const bool bActivated = AbilitySystem->TriggerAbilityFromGameplayEvent(Spec->Handle, AbilitySystem->AbilityActorInfo.Get(), FGameplayTag(), &Payload, *AbilitySystem);
		UE_LOG(LogGSTNProjectile, Log, TEXT("ProjectileAbility shooter=%d target=%d speed=%.0f gravityZ=%.0f high=%d activated=%d active=%d"),
			PawnIds[ShooterIndex], PawnIds[TargetIndex], Instance->Projectile.Speed, Instance->Projectile.GravityAccel.Z, Instance->bHighArc ? 1 : 0,
			bActivated ? 1 : 0, Projectiles->GetNumActive());
	}

	FAutoConsoleCommandWithWorldAndArgs GProjectileAbilityCommand(
		TEXT("gstn.Projectile.Ability"),
		TEXT("Host: grants the ServerOnly fire ability to a combat pawn and triggers it at another (pawns by ascending entity id). Args: [Shooter=0] [Target=1] [Speed=1500] [GravityZ=0] [HighArc=0] [LifetimeFrames=120]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FireAbilityFromConsole));

	FAutoConsoleCommandWithWorldAndArgs GProjectileFireCommand(
		TEXT("gstn.Projectile.Fire"),
		TEXT("Fires a test projectile from the host's first player pawn along its facing. Args: [Speed=1500 cm/s] [GravityZ=0 cm/s^2] [LifetimeFrames=120] [Damage=8]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FireFromConsole));

	/** A dev shot from Pawn along its facing raised by PitchDeg, clear of its capsule (the owner is never hit anyway) */
	FGSTN_ProjectileParams MakeDevShot(const APawn& Pawn, float Speed, double GravityZ, int32 LifetimeFrames, float PitchDeg, float YawOffsetDeg)
	{
		FGSTN_ProjectileParams Params;
		Params.Speed = Speed;
		Params.GravityAccel = FVector(0.0, 0.0, GravityZ);
		Params.LifetimeFrames = FMath::Max(LifetimeFrames, 1);
		Params.Shape.Type = EGSTN_ShapeType::Sphere;
		Params.Shape.Extent = FVector(15.0);
		Params.Hit.Damage = 8.f;
		Params.Hit.BaseKnockback = 30.f;
		Params.Hit.AngleDeg = 30.f;
		FRotator Aim = Pawn.GetActorForwardVector().Rotation();
		Aim.Pitch = FMath::Clamp(Aim.Pitch + PitchDeg, -89.0, 89.0);
		Aim.Yaw += YawOffsetDeg;
		Params.Direction = Aim.Vector();
		Params.Origin = Pawn.GetActorLocation() + Pawn.GetActorForwardVector() * 80.0;
		return Params;
	}

	/**
	 * gstn.Projectile.GhostFire, run in a client world of a one-process PIE: the harness stand-in for a predicted fire ability (a player's
	 * shot needs a command binding in content). The client draws its ghost at its own next frame, and the server world fires the shot of
	 * the same pawn starting at that frame, as the host does for a predicted activation (the input of frame F runs at frame F there)
	 */
	void GhostFireFromConsole(const TArray<FString>& Args, UWorld* World)
	{
		UGSTN_ProjectileSubsystem* Client = UGSTN_ProjectileSubsystem::Get(World);
		UWorld* ServerWorld = FindAuthorityWorld(nullptr);
		UGSTN_ProjectileSubsystem* Server = UGSTN_ProjectileSubsystem::Get(ServerWorld);
		const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Client || !Server || World == ServerWorld || !Pawn || !Pawn->GetPlayerState())
		{
			UE_LOG(LogGSTNProjectile, Warning, TEXT("gstn.Projectile.GhostFire: run it in a client world of a one-process PIE with a pawn"));
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
		if (!ServerPawn)
		{
			UE_LOG(LogGSTNProjectile, Warning, TEXT("gstn.Projectile.GhostFire: no server pawn for player %d"), Pawn->GetPlayerState()->GetPlayerId());
			return;
		}
		const float Speed = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 1500.f;
		const double GravityZ = Args.IsValidIndex(1) ? FCString::Atod(*Args[1]) : 0.0;
		const int32 Lifetime = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : 120;
		const float Pitch = Args.IsValidIndex(3) ? FCString::Atof(*Args[3]) : 30.f;
		const int32 StartFrame = Client->GetLastFrame() + 1;
		const AGSTN_Projectile* Ghost = Client->SpawnGhost(MakeDevShot(*Pawn, Speed, GravityZ, Lifetime, Pitch, 0.f), Pawn, StartFrame);
		FGSTN_ProjectileParams HostShot = MakeDevShot(*ServerPawn, Speed, GravityZ, Lifetime, Pitch, 0.f);
		HostShot.OwnerEntityId = Server->FindEntityIdOf(ServerPawn);
		const AGSTN_Projectile* Fired = Server->FireProjectileAtFrame(HostShot, StartFrame, ServerPawn);
		UE_LOG(LogGSTNProjectile, Log, TEXT("ProjectileGhostFire player=%d start=%d clientLast=%d serverLast=%d ghost=%d fired=%d"),
			Pawn->GetPlayerState()->GetPlayerId(), StartFrame, Client->GetLastFrame(), Server->GetLastFrame(), Ghost ? 1 : 0, Fired ? 1 : 0);
	}

	void GhostReportFromConsole(const TArray<FString>& Args, UWorld* World)
	{
		const UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(World);
		if (!Projectiles)
		{
			return;
		}
		const FGSTN_ProjectileGhostStats& Stats = Projectiles->GetGhostStats();
		UE_LOG(LogGSTNProjectile, Log, TEXT("ProjectileGhostReport world=%s spawned=%d adopted=%d snapped=%d expired=%d pending=%d lastErrorCm=%.2f maxAdoptedErrorCm=%.2f lastArrivalFrames=%d received=%d"),
			*World->GetName(), Stats.Spawned, Stats.Adopted, Stats.Snapped, Stats.Expired, Projectiles->GetNumGhosts(), Stats.LastErrorCm,
			Stats.MaxAdoptedErrorCm, Stats.LastArrivalFrames, Projectiles->GetNumReceivedFromHost());
	}

	/** gstn.Projectile.Burst [Count=20] [LifetimeFrames=62] [Speed=1500]: a fan of shots raised 75 degrees from the host's first pawn (misses everyone) */
	void BurstFromConsole(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = FindAuthorityWorld(InWorld);
		UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(World);
		APawn* Pawn = nullptr;
		if (World)
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It && !Pawn; ++It)
			{
				Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
			}
		}
		if (!Projectiles || !Pawn)
		{
			UE_LOG(LogGSTNProjectile, Warning, TEXT("gstn.Projectile.Burst: no host world with a pawn"));
			return;
		}
		const int32 Count = Args.IsValidIndex(0) ? FMath::Clamp(FCString::Atoi(*Args[0]), 1, 64) : 20;
		const int32 Lifetime = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 62;
		const float Speed = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 1500.f;
		int32 Fired = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGSTN_ProjectileParams Params = MakeDevShot(*Pawn, Speed, 0.0, Lifetime, 75.f, 360.f * Index / Count);
			Params.OwnerEntityId = Projectiles->FindEntityIdOf(Pawn);
			Fired += Projectiles->FireProjectile(Params) ? 1 : 0;
		}
		UE_LOG(LogGSTNProjectile, Log, TEXT("ProjectileBurst fired=%d active=%d total=%u"), Fired, Projectiles->GetNumActive(), Projectiles->GetNumFired());
	}

	/** gstn.Projectile.NetReport [Label]: the host driver's cumulative wire bytes, per client connection, and the projectiles fired so far */
	void NetReportFromConsole(const TArray<FString>& Args, UWorld* InWorld)
	{
		UWorld* World = FindAuthorityWorld(InWorld);
		const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
		const UGSTN_ProjectileSubsystem* Projectiles = UGSTN_ProjectileSubsystem::Get(World);
		if (!Driver || !Projectiles)
		{
			UE_LOG(LogGSTNProjectile, Warning, TEXT("gstn.Projectile.NetReport: no host net driver"));
			return;
		}
		FString PerConn;
		for (const UNetConnection* Conn : Driver->ClientConnections)
		{
			PerConn += FString::Printf(TEXT("%s%d/%d"), PerConn.IsEmpty() ? TEXT("") : TEXT(","), Conn->OutTotalBytes, Conn->OutTotalPackets);
		}
		UE_LOG(LogGSTNProjectile, Log, TEXT("ProjectileNet label=%s time=%.3f outBytes=%u conns=%d perConn=%s fired=%u active=%d"),
			Args.IsValidIndex(0) ? *Args[0] : TEXT("-"), FPlatformTime::Seconds(), Driver->OutTotalBytes, Driver->ClientConnections.Num(), *PerConn,
			Projectiles->GetNumFired(), Projectiles->GetNumActive());
	}

	FAutoConsoleCommandWithWorldAndArgs GProjectileGhostFireCommand(
		TEXT("gstn.Projectile.GhostFire"),
		TEXT("Client world of a one-process PIE: draws a predicted ghost and has the server fire the same pawn's shot at the ghost's frame. Args: [Speed=1500] [GravityZ=0] [LifetimeFrames=120] [PitchDeg=30]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GhostFireFromConsole));

	FAutoConsoleCommandWithWorldAndArgs GProjectileGhostReportCommand(
		TEXT("gstn.Projectile.GhostReport"),
		TEXT("Logs this world's ghost counts (spawned, adopted, snapped, expired) and the last reconcile error."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GhostReportFromConsole));

	FAutoConsoleCommandWithWorldAndArgs GProjectileBurstCommand(
		TEXT("gstn.Projectile.Burst"),
		TEXT("Host: fires a fan of projectiles raised 75 degrees from the first pawn. Args: [Count=20] [LifetimeFrames=62] [Speed=1500]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BurstFromConsole));

	FAutoConsoleCommandWithWorldAndArgs GProjectileNetReportCommand(
		TEXT("gstn.Projectile.NetReport"),
		TEXT("Host: logs the net driver's cumulative out bytes per client connection and the projectiles fired. Args: [Label]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&NetReportFromConsole));
}
