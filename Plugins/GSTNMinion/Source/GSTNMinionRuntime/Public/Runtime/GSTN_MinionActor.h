// Minion runtime (authority batch, M8 G3).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSTN_MinionActor.generated.h"

class UGSTN_MinionCombatComponent;

/**
 * The server-side actor of a minion (combat-m8-minions.md 4): a root scene and the combat component, no mesh and no movement
 * component (the batch moves it). Not replicated: clients see minions through the packs (G6) and their own puppets (G7).
 *
 * Pooled by UGSTN_MinionCombatSubsystem: spawned once, then acquired and released as slots come and go, so a wave does not pay
 * actor construction. Released actors are hidden, without collision and without tick; the transform of a T2/T3 minion is stale on
 * purpose (only T0/T1 are published), which is why nothing in the simulation reads it.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class GSTNMINIONRUNTIME_API AGSTN_MinionActor : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_MinionActor();

	UGSTN_MinionCombatComponent* GetMinionComponent() const { return MinionComponent; }

	/** Out of the pool: visible and placed at Transform. The subsystem binds the component afterwards */
	void OnAcquired(const FTransform& Transform);

	/** Back to the pool: component reset, hidden, no collision, no tick */
	void OnReleased();

	bool IsPooled() const { return bPooled; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Minion")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Minion")
	TObjectPtr<UGSTN_MinionCombatComponent> MinionComponent;

	bool bPooled = false;
};
