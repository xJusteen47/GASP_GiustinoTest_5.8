// Projectile, world side (needs the GAS-NPP fork for the fixed tick it reads).

#pragma once

#include "CoreMinimal.h"
#include "Movement/GSTN_BallisticMode.h"
#include "Subsystems/WorldSubsystem.h"

#include "GSTN_ProjectileSubsystem.generated.h"

class AGSTN_Projectile;
class UGSTN_CombatWorldSubsystem;
struct FGSTN_HitInboxEntry;
struct FGSTN_StepSnapshot;

/** One live projectile on the host: the full parameters (hit data included, which never leaves the host) and its segment */
USTRUCT()
struct GSTNPROJECTILENP_API FGSTN_ActiveProjectile
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AGSTN_Projectile> Actor;

	UPROPERTY()
	FGSTN_ProjectileParams Params;

	UPROPERTY()
	FGSTN_BallisticSegment Segment;

	/** Spawn order on this host: the order projectiles are stepped in, so hits of one frame are posted in the same order every run */
	UPROPERTY()
	uint32 Serial = 0;

	/** Entities already hit (one hit per target per projectile, combat-m10 §10); consumed at Params.MaxHits */
	UPROPERTY()
	TArray<int32> HitTargets;

	/**
	 * Credit snapshot at the release (M10 G3): the shooter, and its player state for when the shooter is gone (death, disconnect). Its hits
	 * carry the first that is still valid as FGSTN_HitInboxEntry::DetachedInstigator; events only, never the simulation
	 */
	TWeakObjectPtr<AActor> CreditShooter;
	TWeakObjectPtr<AActor> CreditPlayerState;
};

/** A ghost the firing client draws while the host's projectile is on its way (slice 3) */
USTRUCT()
struct GSTNPROJECTILENP_API FGSTN_PendingGhost
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AGSTN_Projectile> Actor;

	/** The shooter, matched against the Owner the host's projectile replicates with */
	TWeakObjectPtr<AActor> Owner;

	int32 StartFrame = INDEX_NONE;
};

/** What became of this client's ghosts (harness, gstn.Projectile.GhostReport) */
struct FGSTN_ProjectileGhostStats
{
	int32 Spawned = 0;
	int32 Adopted = 0;
	int32 Snapped = 0;
	int32 Expired = 0;
	double LastErrorCm = 0.0;
	double MaxAdoptedErrorCm = 0.0;
	/** Frames from the ghost's start to the host projectile's arrival, last adoption (the round trip the ghost hid) */
	int32 LastArrivalFrames = 0;
};

/**
 * The host's projectiles (entita-dinamiche review §3, option C; the shooter's predicted ghost since slice 3). FireProjectile spawns the actor once with its
 * ballistic segment, starting at the next fixed frame; after the Network Prediction fixed tick of every frame F the host sweeps each
 * projectile from F - 1 to F against the combat step snapshot of F and posts a normal hit entry (AttackerId = the owner) into the
 * victim's inbox through UGSTN_CombatWorldSubsystem::PostHit, the call minion attacks use. The victim drains it at F + 1 and runs the
 * melee pipeline unchanged: relationship, block and parry, armour and hardness, wounds and parts (fighters and minions alike).
 *
 * Rollback safety: nothing here is simulation state of a Network Prediction model. The host never resimulates, so the step runs only
 * on non-resimulated frames, and positions are a closed form of the frame number. Clients only read GetLastFrame to draw.
 *
 * Predicted ghost: the firing client draws its own prediction of the shot at once (SpawnGhost) instead of one round trip late; the host's
 * projectile adopts it on arrival (ReconcileGhostFor). Pure presentation: the spawn stays once, on the host, and hits stay the host's.
 *
 * Out of these slices: world collision, projectile versus projectile, deflect, lag compensation,
 * a timeline track key (a move fires through FireProjectile from its ability on the host for now).
 */
UCLASS()
class GSTNPROJECTILENP_API UGSTN_ProjectileSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_FourParams(FOnProjectileHit, uint32 /*Serial*/, int32 /*VictimEntityId*/, int32 /*Frame*/, const FGSTN_HitInboxEntry& /*Entry*/);

	static UGSTN_ProjectileSubsystem* Get(const UWorld* World);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Host only: spawns a projectile that starts at the frame after the last fixed frame. An unset OwnerTeamId is read from the owner's
	 * snapshot entry. Null on a client, past gstn.Projectile.MaxActive or without a fixed tick yet
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "GSTN|Projectile")
	AGSTN_Projectile* FireProjectile(const FGSTN_ProjectileParams& Params);

	/**
	 * FireProjectile with the start frame given (INDEX_NONE = the next frame) and the shooter actor that the projectile replicates as
	 * its Owner (null = the owner entity's actor). A start frame other than the next one is for dev commands and the harness only
	 * (gstn.Projectile.GhostFire stands in for a predicted activation, which fires on the host at the frame the client predicted)
	 */
	AGSTN_Projectile* FireProjectileAtFrame(const FGSTN_ProjectileParams& Params, int32 StartFrame, AActor* OwnerActor);

	/**
	 * Firing client only (gstn.Projectile.PredictGhost): draws the shot it just predicted as a local ghost starting at StartFrame
	 * (INDEX_NONE = the next frame, the frame the host will start it at). Once per owner and start frame, so a resimulated activation
	 * does not spawn a second one. The host's projectile adopts or drops it on arrival; unmatched it goes after GhostTimeoutFrames
	 */
	AGSTN_Projectile* SpawnGhost(const FGSTN_ProjectileParams& Params, AActor* OwnerActor, int32 StartFrame = INDEX_NONE);

	/** Client: the host's projectile arrived (its BeginPlay); adopt or drop the matching ghost of its owner, if any */
	void ReconcileGhostFor(AGSTN_Projectile& Authority);

	const FGSTN_ProjectileGhostStats& GetGhostStats() const { return GhostStats; }
	int32 GetNumGhosts() const { return Ghosts.Num(); }
	/** Client: one more host projectile arrived (its BeginPlay); counts spawns received, the "once per projectile" check */
	void NoteReceivedFromHost() { ++NumReceivedFromHost; }
	int32 GetNumReceivedFromHost() const { return NumReceivedFromHost; }

	/**
	 * Host only: ends a live projectile now (destroys its actor, no more sweeps). For owners that decide an end the sweep does not see,
	 * such as a thrown object that lands on the world (GSTNHeld). False when it is not one of this host's live projectiles
	 */
	bool RemoveProjectile(const AGSTN_Projectile* Projectile);

	/** Projectiles fired on this host since the world started */
	uint32 GetNumFired() const { return NextSerial - 1; }

	/** Whether FireProjectile can spawn here now: the host, with a fixed tick seen. The ability's quiet check (a client skips its fire) */
	bool CanFire() const { return HasAuthority() && LastFrame != INDEX_NONE; }

	/** The combat pipeline id Actor is registered under in this world, INDEX_NONE if none (a lookup, not simulation state) */
	int32 FindEntityIdOf(const AActor* Actor) const;

	/** Sweeps every live projectile into Frame and posts its hits; removes the consumed and expired ones. Public for tests */
	void StepFrame(int32 Frame);

	/** The last Network Prediction fixed frame seen on this machine (a client's predicted frame), INDEX_NONE before the first */
	int32 GetLastFrame() const { return LastFrame; }

	double GetStepSeconds() const { return StepSeconds; }

	int32 GetNumActive() const { return Active.Num(); }

	/** Every hit this host posted (harness and tests) */
	FOnProjectileHit OnProjectileHit;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandlePostFixedTick(int32 ServerFrame, int32 EndTimeMS, bool bResimulating);

	/** True when the projectile is consumed (MaxHits reached) */
	bool StepProjectile(FGSTN_ActiveProjectile& Projectile, int32 Frame, const FGSTN_StepSnapshot& Snapshot, UGSTN_CombatWorldSubsystem& Combat);

	bool HasAuthority() const;

	/** Client: removes ghosts whose host projectile never came (refused or mispredicted shot) */
	void ExpireGhosts(int32 Frame);

	UPROPERTY()
	TArray<FGSTN_ActiveProjectile> Active;

	UPROPERTY()
	TArray<FGSTN_PendingGhost> Ghosts;

	FGSTN_ProjectileGhostStats GhostStats;
	int32 NumReceivedFromHost = 0;

	uint32 NextSerial = 1;
	int32 LastFrame = INDEX_NONE;
	double StepSeconds = 0.016;
	FDelegateHandle PostFixedTickHandle;
	TArray<int32> CandidateScratch;
};
