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
};

/**
 * The host's projectiles (entita-dinamiche review §3, option C without the ghost yet). FireProjectile spawns the actor once with its
 * ballistic segment, starting at the next fixed frame; after the Network Prediction fixed tick of every frame F the host sweeps each
 * projectile from F - 1 to F against the combat step snapshot of F and posts a normal hit entry (AttackerId = the owner) into the
 * victim's inbox through UGSTN_CombatWorldSubsystem::PostHit, the call minion attacks use. The victim drains it at F + 1 and runs the
 * melee pipeline unchanged: relationship, block and parry, armour and hardness, wounds and parts (fighters and minions alike).
 *
 * Rollback safety: nothing here is simulation state of a Network Prediction model. The host never resimulates, so the step runs only
 * on non-resimulated frames, and positions are a closed form of the frame number. Clients only read GetLastFrame to draw.
 *
 * Out of this slice: the predicted ghost on the shooter, world collision, projectile versus projectile, deflect, lag compensation,
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

	UPROPERTY()
	TArray<FGSTN_ActiveProjectile> Active;

	uint32 NextSerial = 1;
	int32 LastFrame = INDEX_NONE;
	double StepSeconds = 0.016;
	FDelegateHandle PostFixedTickHandle;
	TArray<int32> CandidateScratch;
};
