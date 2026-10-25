// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "GSTN_RelationshipResolver.generated.h"

/** The two parties of a hit, as plain ids: everything a relationship rule may read */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_RelationshipQuery
{
	GENERATED_BODY()

	/** Network Prediction proxy id of the attacker; INDEX_NONE for an attacker that is not a proxy (a trap, a projectile owner-less) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relationship")
	int32 AttackerEntityId = INDEX_NONE;

	/** 255 = no team */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relationship")
	uint8 AttackerTeamId = 255;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relationship")
	int32 VictimEntityId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relationship")
	uint8 VictimTeamId = 255;
};

/**
 * Decides which Combat.Relationship.* tag a victim has for an attacker (combat-m4-hits 3, 3b). The relationship picks the
 * hitbox's variant and whether the TeamFilter step lets the hit through.
 *
 * The default rule is: the same entity is Self, the same team is Ally, a victim or an attacker with no team is Neutral,
 * anything else is Enemy. A game that needs more (factions, hostility toward the player only) subclasses this in Blueprint or
 * C++ and picks it in the rules asset. The hook is pure by contract: everything it may read is in the query, no world, actor or
 * component, because it runs inside the simulation and again on every re-simulation.
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class GSTNCOMBATCORE_API UGSTN_RelationshipResolver : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GSTN|Hits")
	FGameplayTag Resolve(const FGSTN_RelationshipQuery& Query) const;
	virtual FGameplayTag Resolve_Implementation(const FGSTN_RelationshipQuery& Query) const;

	/** The default rule, also used when the rules asset names no resolver */
	static FGameplayTag DefaultRule(const FGSTN_RelationshipQuery& Query);
};
