// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "GSTN_HitEvents.generated.h"

struct FGSTN_HitEffectContext;
class UGSTN_CombatRules;

/**
 * What an ability learns when its owner is hit: the payload of the Combat.Event.Hit.Received gameplay event (combat-m4-hits 4).
 * It is a view of the hit effect context, because a gameplay event can only carry an effect context handle for structured data;
 * the context is what the cues already read, so one handle serves both. An ability that triggers on the event reads it with
 * FromEventData.
 *
 * This is not the outbox event Combat.Event.HitReceived (published once per occurrence by the combat tick); the gameplay event
 * is sent to the victim's abilities so a triggered ability (a counter, a hurt reaction) can start.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_HitEventPayload
{
	GENERATED_BODY()

	/** What the hit did to the victim's movement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	EGSTN_HitReaction Reaction = EGSTN_HitReaction::StaggerInPlace;

	/** True when the victim was put in hitstun */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	bool bStaggered = false;

	/** The hurtbox that was hit, INDEX_NONE if unknown */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	int32 HurtboxIndex = INDEX_NONE;

	/** Combat.Relationship.* of the victim for the attacker */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (Categories = "Combat.Relationship"))
	FGameplayTag Relationship;

	/** Combat.KOReason.* when this hit knocked the victim out, invalid otherwise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (Categories = "Combat.KOReason"))
	FGameplayTag KOReason;

	/** Copies the payload fields into Context (the other fields of the context are left alone) */
	void WriteToContext(FGSTN_HitEffectContext& Context) const;

	/** The payload fields of a hit context */
	static FGSTN_HitEventPayload FromContext(const FGSTN_HitEffectContext& Context);

	/**
	 * The gameplay event for Victim: tag Combat.Event.Hit.Received, Instigator the attacker, and a new hit context carrying this
	 * payload. Rules goes into the context for the damage execution of whatever the event triggers.
	 */
	FGameplayEventData MakeEventData(AActor* Attacker, AActor* Victim, const UGSTN_CombatRules* Rules) const;

	/** Reads the payload from an event made by MakeEventData. False when the event carries no hit context */
	static bool FromEventData(const FGameplayEventData& EventData, FGSTN_HitEventPayload& OutPayload);
};
