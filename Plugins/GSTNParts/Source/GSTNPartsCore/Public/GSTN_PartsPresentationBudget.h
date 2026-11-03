// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_PartsPresentationBudget.generated.h"

/**
 * Ceilings of the cosmetic debris of a match (combat-parts.md §6.4). Plain numbers, read only by the presentation module, which
 * lives outside the simulation and is skipped on a dedicated server. They are host-tier ceilings: a listen-server host renders
 * too, so the figures bound one process that also simulates. Presentation scales them down by scalability level and by distance.
 * The figures are proposals to be measured (spike P0b), so they are data, never constants in code.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartsPresentationBudget
{
	GENERATED_BODY()

	/** Pooled chunk actors (limbs and large shards) alive at once, all entities together; the oldest is recycled when the pool is full */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = 0, UIMax = 256))
	int32 MaxChunks = 48;

	/** Seconds a chunk lives before it shrinks and fades out */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = 0, ForceUnits = "s"))
	float ChunkLifetime = 4.f;

	/** Live shatter shard particles, all entities together (CPU sim, simple ground collision, no pawn collision) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = 0, UIMax = 4096))
	int32 MaxShardParticles = 600;

	/** Geometry Collections alive at once; kept for players and bosses only */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = 0, UIMax = 8))
	int32 MaxGeometryCollections = 2;

	/** Seconds after which a resting chunk stops simulating */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = 0, ForceUnits = "s"))
	float SleepAfter = 1.5f;
};
