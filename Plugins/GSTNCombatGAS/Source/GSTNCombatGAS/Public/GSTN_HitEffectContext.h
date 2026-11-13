// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "GSTN_HitEffectContext.generated.h"

class UGSTN_CombatRules;

/**
 * The effect context of a hit (combat-m4-hits 4): what a damage execution and the cues of a hit need to know about it. It
 * rides on every effect the hit applies, so the one handle tells the execution which damage mode the match plays and tells a cue
 * where and how the victim was hit.
 *
 * Hit id: the hitbox is addressed by indices into the attacker's compiled timeline table plus the frame the timeline started,
 * never by pointer or name, so it is the same on every machine and cheap to send. Tags travel as tags (net indexed), the reaction
 * as one byte.
 *
 * Rules: a weak, local pointer to the rules of the match. It is NOT serialized: contexts are rebuilt by the simulation on each
 * machine from the same rules, and a cue only needs the fields above. A context that arrives from the network therefore has no
 * rules, and a damage execution on it falls back to the percent mode, the identity.
 *
 * Make builds the handle. The stock ability system allocates plain contexts, and a custom allocator would need an
 * AbilitySystemGlobals class named in a project ini, which a plugin must not require, so the code that applies a hit builds this
 * context itself and passes it to MakeOutgoingSpec.
 */
USTRUCT()
struct GSTNCOMBATGAS_API FGSTN_HitEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

	/** Index of the attacker's timeline in its compiled table */
	int32 TimelineIndex = INDEX_NONE;

	/** Index of the hitbox inside that timeline */
	int32 HitboxIndex = INDEX_NONE;

	/** Simulation frame the attacker's timeline started on: with the two indices it identifies one hit attempt */
	int32 StartFrame = INDEX_NONE;

	/** Where the hit connected */
	FVector_NetQuantize10 HitLocation = FVector_NetQuantize10(ForceInitToZero);

	/** Combat.Relationship.* of the victim for the attacker */
	FGameplayTag Relationship;

	/** What the hit did to the victim's movement */
	EGSTN_HitReaction Reaction = EGSTN_HitReaction::StaggerInPlace;

	/** True when the victim was put in hitstun */
	bool bStaggered = false;

	/** Index of the hurtbox that was hit, INDEX_NONE if unknown */
	int32 HurtboxIndex = INDEX_NONE;

	/** Combat.KOReason.* when the hit knocked the victim out, invalid otherwise */
	FGameplayTag KOReason;

	/** The rules of the match, for the damage mode. Local, not serialized */
	TWeakObjectPtr<const UGSTN_CombatRules> Rules;

	/** A handle owning a new context whose instigator and causer are the given actors (the usual source of an effect) */
	static FGameplayEffectContextHandle Make(AActor* Instigator, AActor* Causer, const UGSTN_CombatRules* InRules);

	/** The hit context behind Handle, or null when the handle holds a plain context */
	static const FGSTN_HitEffectContext* FromHandle(const FGameplayEffectContextHandle& Handle);
	static FGSTN_HitEffectContext* FromHandleMutable(const FGameplayEffectContextHandle& Handle);

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual FGSTN_HitEffectContext* Duplicate() const override;
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;
};

template<>
struct TStructOpsTypeTraits<FGSTN_HitEffectContext> : public TStructOpsTypeTraitsBase2<FGSTN_HitEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
