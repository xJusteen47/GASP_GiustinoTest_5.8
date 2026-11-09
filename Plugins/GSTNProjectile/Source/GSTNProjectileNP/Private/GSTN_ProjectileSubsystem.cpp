// Projectile, world side (needs the GAS-NPP fork for the fixed tick it reads).

#include "GSTN_ProjectileSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "GSTNProjectileCoreModule.h"
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
}

AGSTN_Projectile* UGSTN_ProjectileSubsystem::FireProjectile(const FGSTN_ProjectileParams& InParams)
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

	// Starts at the next frame: the first sweep is LastFrame + 1 -> LastFrame + 2, the same frame numbers on every machine
	const FGSTN_BallisticSegment Segment = FGSTN_BallisticMode::MakeSegment(Params, LastFrame + 1, StepSeconds);
	const FTransform SpawnTransform = FGSTN_BallisticMode::CarrierAt(Segment, Segment.StartFrame);
	AGSTN_Projectile* Actor = World->SpawnActorDeferred<AGSTN_Projectile>(AGSTN_Projectile::StaticClass(), SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->InitProjectile(Segment, Params.LifetimeFrames, static_cast<float>(Params.Shape.Extent.X));
	Actor->FinishSpawning(SpawnTransform);

	FGSTN_ActiveProjectile& Projectile = Active.AddDefaulted_GetRef();
	Projectile.Actor = Actor;
	Projectile.Params = Params;
	Projectile.Segment = Segment;
	Projectile.Serial = NextSerial++;
	UE_LOG(LogGSTNProjectile, Log, TEXT("Projectile %u fired by %d at frame %d from %s, v %s, life %d"), Projectile.Serial, Params.OwnerEntityId,
		Segment.StartFrame, *Segment.Start.ToCompactString(), *Segment.Velocity.ToCompactString(), Params.LifetimeFrames);
	return Actor;
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

		if (const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World))
		{
			TArray<int32> Ids;
			Combat->GetRegisteredEntityIds(Ids);
			for (const int32 Id : Ids)
			{
				if (Combat->FindOwnerActor(Id) == Pawn)
				{
					Params.OwnerEntityId = Id;
					break;
				}
			}
		}
		Projectiles->FireProjectile(Params);
	}

	FAutoConsoleCommandWithWorldAndArgs GProjectileFireCommand(
		TEXT("gstn.Projectile.Fire"),
		TEXT("Fires a test projectile from the host's first player pawn along its facing. Args: [Speed=1500 cm/s] [GravityZ=0 cm/s^2] [LifetimeFrames=120] [Damage=8]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FireFromConsole));
}
