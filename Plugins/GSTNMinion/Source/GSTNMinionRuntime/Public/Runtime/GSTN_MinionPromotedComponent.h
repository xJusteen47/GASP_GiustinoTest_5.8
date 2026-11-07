// Minion runtime (authority batch, M8 G9a).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GSTN_MinionPromotedComponent.generated.h"

/**
 * Marks a fighter that began as a minion (UGSTN_MinionSpawner::Promote) and carries what the swap needs, replicated once at the fighter's
 * spawn. No tick, no behaviour: it is data.
 *
 * Two readers, both G9b: the client reads PromotedFromPackId / PromotedFromMinionId to find the puppet it shows for that item and
 * cross-fades it into this fighter, and the fighter's kit reads HealthFraction to set Health = MaxHealth x HealthFraction after its init
 * effect. G9a only writes the values; nothing reads them yet.
 */
UCLASS(ClassGroup = "GSTN", meta = (BlueprintSpawnableComponent))
class GSTNMINIONRUNTIME_API UGSTN_MinionPromotedComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_MinionPromotedComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Pack-local id of the net item the minion was published as (what the clients know it by); INDEX_NONE if it never had one */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Minion|Promotion")
	int32 PromotedFromMinionId = INDEX_NONE;

	/** Id of the pack that held the item (with PromotedFromMinionId it names the puppet); INDEX_NONE if none */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Minion|Promotion")
	int32 PromotedFromPackId = INDEX_NONE;

	/** The minion type it was (a leaf of Minion.Type) */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Minion|Promotion")
	FGameplayTag SourceTypeTag;

	/** Health / MaxHealth of the minion at the swap, or 1 when the promotion did not keep it. The kit applies it to the fighter's Health */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Minion|Promotion")
	float HealthFraction = 1.f;

	/** Team id (FGenericTeamId value) of the minion type; 255 = none */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Minion|Promotion")
	uint8 Team = 255;
};
