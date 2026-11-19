// Combat presentation (CP4 of the Blueprint seams): the gameplay cue of a combat hit (spark, hit sound, camera shake, rumble).

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Burst.h"
#include "GameplayTagContainer.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_GCN_CombatHit.generated.h"

/**
 * The cue a hitbox names in its Cue Tag (GameplayCue.Hit.*): a GCN Burst (all its data: Niagara systems, sounds, camera shakes, force feedback,
 * decals, each with its own spawn condition, e.g. "only for the local attacker or victim") that knows about combat hits:
 *  - placed at the hit: the spark appears where the hitbox touched the hurtbox (the hit context's location), facing along the hit;
 *  - played once on the attacker's machine: the attacker's prediction plays it at once (UGSTN_HitPresentationComponent) and the real cue of
 *    the same hit, when it arrives, is skipped (ShouldPlayAuthoritativeCue). On every other machine the real cue is the only one;
 *  - a cue is one shot: a predicted hit that a correction removes cannot take its spark back (it is a few frames long).
 *
 * Make a Blueprint child per hit size (GameplayCue.Hit.Light, .Medium, .Heavy, .Finisher: owner rule "size follows importance", four steps)
 * and fill Burst Effects; no graph needed. On Burst (inherited) runs after the effects for anything else; Get Combat Hit Info reads the hit.
 * VFX checklist (owner-approved, 2026-11-15): spark core always faces the camera (a sprite in the Niagara system), camera shake only on heavy
 * hits and never on jabs, a different sound for whiff and hit (the whiff is the move's cosmetic window), the effect never hides the contact pose.
 */
UCLASS(Blueprintable, meta = (DisplayName = "GCN Combat Hit", ShortTooltip = "The cue of a combat hit: a GCN Burst placed at the hit and played once."))
class GSTNCOMBATNP_API UGSTN_GCN_CombatHit : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()

public:
	UGSTN_GCN_CombatHit();

	/** Skip the real cue of a hit the attacker's machine already showed from its prediction (on: no double spark on the attacker's screen) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Hit")
	bool bSkipWhenPredicted = true;

	/** Place the effects at the hit location of the combat hit context (else the target actor, as a plain Burst) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Hit")
	bool bPlaceAtHit = true;

	/** Turn the effects along the hit: X points from the attacker to the victim, flat (sparks stream the way the victim flies) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Hit")
	bool bFaceAlongHit = true;

	/** The parameters a combat hit cue plays with: the hit location and direction filled in from the hit context. Pure */
	static FGameplayCueParameters MakeHitParameters(const AActor* Target, const FGameplayCueParameters& Parameters, bool bPlaceAtHit, bool bFaceAlongHit);

protected:
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
};

/** What a combat hit cue knows about its hit, for a Blueprint */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_CombatHitCueInfo
{
	GENERATED_BODY()

	/** Where the hitbox touched the hurtbox */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	FVector Location = FVector::ZeroVector;

	/** From the attacker to the victim, flat; zero when unknown */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	FVector Direction = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	TObjectPtr<AActor> Attacker = nullptr;

	/** What the hit did to the victim's movement. Only the real cue knows it; the attacker's predicted cue says StaggerInPlace */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	EGSTN_HitReaction Reaction = EGSTN_HitReaction::StaggerInPlace;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	bool bStaggered = false;

	/** Combat.Relationship.* of the victim for the attacker */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	FGameplayTag Relationship;

	/** Combat.KOReason.* when the hit knocked the victim out (the finisher: zoom, slow motion), else empty */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	FGameplayTag KOReason;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	int32 HurtboxIndex = INDEX_NONE;
};

UCLASS()
class GSTNCOMBATNP_API UGSTN_CombatCueLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The combat hit behind a cue's parameters (Target: the cue's target, the victim); false when the cue was not made by a combat hit */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category = "Combat|Cosmetics")
	static bool GetCombatHitInfo(const AActor* Target, const FGameplayCueParameters& Parameters, FGSTN_CombatHitCueInfo& OutInfo);
};
