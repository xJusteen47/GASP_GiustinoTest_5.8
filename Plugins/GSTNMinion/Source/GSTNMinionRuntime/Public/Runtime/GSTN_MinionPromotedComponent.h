// Minion runtime (authority batch, M8 G9a; readers G9b).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Loadout/GSTN_StartingHealthSource.h"
#include "GSTN_MinionPromotedComponent.generated.h"

/**
 * Marks a fighter that began as a minion (UGSTN_MinionSpawner::Promote) and carries what the swap needs, replicated once at the fighter's
 * spawn. No tick, no behaviour of its own: it is data, plus two hand-offs (G9b).
 *
 * - Presentation: at BeginPlay, on every machine, it hands the fighter to UGSTN_MinionPresentationSubsystem, which finds the puppet it shows
 *   for PromotedFromPackId / PromotedFromMinionId (the listen host: SourceSlot), hides the fighter while the puppet is still the one shown,
 *   and swaps them (see the subsystem, "promotion swap").
 * - Health: the fighter's loadout asks it for the starting health (IGSTN_StartingHealthSource) right after the kit's init effect, so the
 *   kit sets Health = MaxHealth x HealthFraction. The combat layer never names this class.
 */
UCLASS(ClassGroup = "GSTN", meta = (BlueprintSpawnableComponent))
class GSTNMINIONRUNTIME_API UGSTN_MinionPromotedComponent : public UActorComponent, public IGSTN_StartingHealthSource
{
	GENERATED_BODY()

public:
	UGSTN_MinionPromotedComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	//~ IGSTN_StartingHealthSource
	virtual float GetStartingHealthFraction() const override { return HealthFraction; }

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

	/** Store slot of the minion on the authority (the listen host's puppets are keyed by slot). Not replicated: meaningless elsewhere */
	int32 SourceSlot = INDEX_NONE;
};
