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
 * The spawn payload is the quantized segment (FGSTN_BallisticSegment::NetSerialize), the lifetime, the visual size and the owner actor.
 *
 * The same class draws the predicted ghost (slice 3): a local, never replicated copy the firing client spawns from its own prediction.
 * When the host's projectile of that owner arrives, it adopts the ghost (drawn at the ghost's place, the offset fading out) or the ghost
 * is dropped (a snap), see FGSTN_ProjectileGhostReconcile.
 */
UCLASS(NotBlueprintable)
class GSTNPROJECTILENP_API AGSTN_Projectile : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_Projectile();

	/** Host only, before FinishSpawning: the motion every machine replays, the frames it lives and the size of its visual */
	void InitProjectile(const FGSTN_BallisticSegment& InSegment, int32 InLifetimeFrames, float InVisualRadius);

	/** Firing client only, before FinishSpawning: the same, as a local ghost that never replicates */
	void InitGhost(const FGSTN_BallisticSegment& InSegment, int32 InLifetimeFrames, float InVisualRadius);

	/** Client: the host's projectile took over a ghost; drawn offset by Offset at Frame, fading to zero over BlendFrames */
	void StartBlend(const FVector& Offset, int32 Frame, int32 BlendFrames);

	const FGSTN_BallisticSegment& GetSegment() const { return Segment; }
	int32 GetLifetimeFrames() const { return LifetimeFrames; }
	bool IsGhost() const { return bGhost; }
	/** Where it is drawn at Frame (blend offset included) */
	FVector GetDrawnPositionAt(int32 Frame) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Replicated once with the spawn; never changes afterwards */
	UPROPERTY(Replicated)
	FGSTN_BallisticSegment Segment;

	/** Frames after StartFrame (16 bits on the wire: up to ~17 minutes at 62.5 Hz) */
	UPROPERTY(Replicated)
	uint16 LifetimeFrames = 0;

	/** Whole cm (8 bits on the wire); the look comes later */
	UPROPERTY(Replicated)
	uint8 VisualRadius = 10;

	/** A local prediction on the firing client, never replicated */
	bool bGhost = false;

	FVector BlendStartOffset = FVector::ZeroVector;
	int32 BlendStartFrame = INDEX_NONE;
	int32 BlendFrames = 0;

private:
	/** Places the actor at the subsystem's last fixed frame; hides it past its lifetime (a client waiting for the host's destroy) */
	void UpdateFromFrame();
};
