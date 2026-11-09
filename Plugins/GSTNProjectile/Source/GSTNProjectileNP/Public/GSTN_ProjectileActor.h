// Projectile, world side (needs the GAS-NPP fork for the fixed tick it reads).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Movement/GSTN_BallisticMode.h"

#include "GSTN_ProjectileActor.generated.h"

class UStaticMeshComponent;

/**
 * The visible side of one projectile. The host spawns it once with its ballistic segment; the segment replicates with the spawn only
 * (COND_InitialOnly, no movement replication), and every machine places the actor at FGSTN_BallisticMode::PositionAt of the last fixed
 * frame its projectile subsystem saw. On a client that frame is its own predicted frame, so the projectile is drawn where the host will
 * test it against that client (entita-dinamiche review §3).
 *
 * The actor holds no gameplay: hits are decided by UGSTN_ProjectileSubsystem on the host from its own copy of the parameters. The host
 * destroys the actor when the projectile is consumed or its lifetime ends; that destroy is the only other thing that crosses the wire.
 */
UCLASS(NotBlueprintable)
class GSTNPROJECTILENP_API AGSTN_Projectile : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_Projectile();

	/** Host only, before FinishSpawning: the motion every machine replays, the frames it lives and the size of its visual */
	void InitProjectile(const FGSTN_BallisticSegment& InSegment, int32 InLifetimeFrames, float InVisualRadius);

	const FGSTN_BallisticSegment& GetSegment() const { return Segment; }
	int32 GetLifetimeFrames() const { return LifetimeFrames; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Replicated once with the spawn; never changes afterwards */
	UPROPERTY(Replicated)
	FGSTN_BallisticSegment Segment;

	UPROPERTY(Replicated)
	int32 LifetimeFrames = 0;

	UPROPERTY(Replicated)
	float VisualRadius = 10.f;

private:
	/** Places the actor at the subsystem's last fixed frame; hides it past its lifetime (a client waiting for the host's destroy) */
	void UpdateFromFrame();
};
