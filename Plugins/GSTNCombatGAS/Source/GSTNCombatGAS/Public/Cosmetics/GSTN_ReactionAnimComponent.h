// Ability-system layer of the combat framework.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GSTN_AdditiveReactionSource.h"
#include "Cosmetics/GSTN_ReactionAnimSet.h"
#include "GSTN_ReactionAnimComponent.generated.h"

class UAbilitySystemComponent;
class UAnimInstance;
class UBlendProfile;
class UAnimMontage;
class UAnimSequenceBase;
class USkeletalMeshComponent;
class UGSTN_ReactionAnimSet;
struct FGameplayEventData;
struct FGSTN_ReactionAnimRule;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnReactionStarted, FName, RuleName, bool, bRestarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSTN_OnReactionEnded, FName, RuleName);

/**
 * Plays the victim reaction animations from data (UGSTN_ReactionAnimSet): every frame it reads the owner's gameplay tags (the combat
 * states the combat tick projects), picks the winning rule and plays its animation on the character's montage slot.
 *
 * Presentation only, on every machine that draws the character (nothing on a dedicated server): it never writes a tag, an attribute or
 * the simulation, so a rollback only changes which rule wins on the next frame. An ability that plays its own timeline montage (the
 * ground bounce, the get-ups) wins over it (bYieldToOtherMontages), so the frame-synced montages of the simulation keep their timing.
 *
 * Add it to the character Blueprint with Tools/GAS/setup_reaction_anims.py (never by hand), or at run time with AddReactionComponent.
 *
 * Cosmetic hooks (CP4): On Reaction Started / Ended follow the rule the owner's tags select (the state, not whether its animation could
 * play), for a Blueprint that adds dust, a hit flash or a sound to a reaction; and the set's HitstopShake trembles the mesh while the owner is
 * frozen by hitstop. Both come from the same per-frame tag read, so a rollback only changes what the next frame shows.
 *
 * Directional hit additives (hit-react-directional, owner option B 2026-11-16): on Combat.Event.Hit.Received it picks the side the hit came
 * from (dot product of the push, attacker to victim, with the victim's facing) and the weight from the hitstun the event carries, and hands
 * them to the anim node FGSTN_AnimNode_DirectionalHitAdditive through IGSTN_AdditiveReactionSource. Visual only, like the rest.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GSTNCOMBATGAS_API UGSTN_ReactionAnimComponent : public UActorComponent, public IGSTN_AdditiveReactionSource
{
	GENERATED_BODY()

public:
	UGSTN_ReactionAnimComponent(const FObjectInitializer& ObjectInitializer);

	/** The reactions. None = the component does nothing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TSoftObjectPtr<UGSTN_ReactionAnimSet> ReactionSet;

	/** The skeletal mesh whose anim instance plays the reactions; None = the first one on the owner that has an anim instance and no leader */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	FName MeshComponentName;

	/** Replaces the set at run time (a kit or a skin with its own reactions); the playing reaction blends out */
	UFUNCTION(BlueprintCallable, Category = "Combat|Reaction")
	void SetReactionSet(UGSTN_ReactionAnimSet* NewSet);

	/** The rule playing now ("" when none), for the debugger and the harness */
	UFUNCTION(BlueprintPure, Category = "Combat|Reaction")
	FName GetActiveRuleName() const;

	/** The rule the owner's tags select changed to RuleName (or the same rule restarted by its RestartEvent). Cosmetic only */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Cosmetics")
	FGSTN_OnReactionStarted OnReactionStarted;

	/** The rule RuleName no longer wins (another rule does, or none) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Cosmetics")
	FGSTN_OnReactionEnded OnReactionEnded;

	/** The offset the hitstop shake puts on the mesh now (relative space), for the harness */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	FVector GetHitstopShakeOffset() const { return ShakeOffset; }

	/** Adds the component to Actor (or returns the one it has) with Set; for scripts and tests */
	UFUNCTION(BlueprintCallable, Category = "Combat|Reaction")
	static UGSTN_ReactionAnimComponent* AddReactionComponent(AActor* Actor, UGSTN_ReactionAnimSet* Set);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// IGSTN_AdditiveReactionSource
	virtual void GetAdditiveReaction(FGSTN_AdditiveReactionRequest& Out) const override;

	/** The side of the running directional additive (Front when none), for the debugger and the harness */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	EGSTN_HitSide GetDirectionalHitSide() const;

	/** The weight of the running directional additive (0 when none), for the harness */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	float GetDirectionalHitWeight() const { return HitLayers[0].Weight; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Loads the set and its animations (presentation: a synchronous load at begin play or on a swap) and binds the restart events */
	void ApplySet(UGSTN_ReactionAnimSet* NewSet);
	void UnbindEvents();
	void OnRestartEvent(const FGameplayEventData* Payload, FGameplayTag Tag);
	void OnHitEvent(const FGameplayEventData* Payload);

	/** Advances the directional additives: side settle, time, fades, the end of a layer */
	void UpdateDirectionalHits(float DeltaTime, const FGameplayTagContainer& Tags);

	/** One running directional additive */
	struct FHitLayer
	{
		FGSTN_AdditiveReactionLayer Draw;
		FVector Push = FVector::ZeroVector;   // world push of the hit as it arrived (attacker to victim)
		uint8 Side = 0;                        // EGSTN_HitSide
		bool bAir = false;
		float Weight = 0.f;                    // the hit's intensity
		float Fade = 1.f;                      // 1, falls to 0 when replaced or blocked
		float Length = 0.f;
		bool bFading = false;
	};
	void ResolveLayer(FHitLayer& Layer) const;

	UAnimInstance* ResolveAnimInstance();
	UAbilitySystemComponent* ResolveAbilitySystem();

	/** Starts the stage (0 = Animation, 1 = ThenAnimation) of the rule; false when it has no animation there */
	bool PlayStage(UAnimInstance& Anim, const FGSTN_ReactionAnimRule& Rule, int32 Stage, float BlendSeconds);

	/** Stops the montage this component plays, holding its last shown pose through the blend; Profile spreads the blend per bone */
	void StopActive(UAnimInstance* Anim, float BlendSeconds, UBlendProfile* Profile = nullptr);

	/** The rule's loaded blend out profile, when it has one and the blend goes back to the character's own pose */
	UBlendProfile* ResolveBlendOutProfile(int32 RuleIndex) const;

	/** A montage not started by this component is playing and not blending out */
	bool IsOtherMontagePlaying(const UAnimInstance& Anim) const;

	/** Starts the rule's entry drop when it applies (the rule shown before is in EntryDropAfterRules) */
	void BeginEntryDrop(const FGSTN_ReactionAnimRule& Rule, FName PreviousRule);

	/** Writes this frame's height of the entry drop and the hitstop shake on the mesh (0 at their end), as a difference over whatever Mover put there */
	void UpdateEntryDrop(float DeltaTime);

	/** The hitstop shake of this frame in the mesh's parent space (zero outside hitstop): screen right, from the first local camera */
	FVector ComputeHitstopShake(float DeltaTime, const FGameplayTagContainer& Tags);

	/** Raises On Reaction Started / Ended when the selected rule changed or restarted */
	void AnnounceReaction(int32 Winner, bool bRestart);

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_ReactionAnimSet> LoadedSet;

	/** Keeps the rules' animations in memory while the set is applied */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequenceBase>> LoadedAnimations;

	/** The rules' blend out profiles, by rule index (null where a rule has none) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBlendProfile>> LoadedBlendOutProfiles;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TArray<TPair<FGameplayTag, FDelegateHandle>> EventHandles;

	/** The rule playing (INDEX_NONE: none) and its stage */
	int32 ActiveRule = INDEX_NONE;
	int32 ActiveStage = 0;

	/** The rule that already ran to its end or was interrupted: not started again until another rule (or none) wins */
	int32 ConsumedRule = INDEX_NONE;

	/** A restart event arrived for this rule since the last tick */
	int32 PendingRestartRule = INDEX_NONE;

	/** The rule whose animation was shown last (kept after it stops, for the next rule's entry drop) */
	FName LastShownRule;

	/** The entry drop running: its height, its length and the time spent (seconds) */
	float DropCm = 0.f;
	float DropSeconds = 0.f;
	float DropElapsed = 0.f;

	/** The offset written on the mesh last (drop + shake) and the relative location it gave: if the location changed since, Mover reset it and the offset is gone */
	FVector AppliedOffset = FVector::ZeroVector;

	/** Seconds into the current hitstop, -1 outside one; the shake of this frame */
	float ShakeElapsed = -1.f;
	FVector ShakeOffset = FVector::ZeroVector;

	/** The rule the events last announced (INDEX_NONE: none) and its name */
	int32 AnnouncedRule = INDEX_NONE;
	FName AnnouncedRuleName;
	FVector LastWrittenLocation = FVector::ZeroVector;

	/** Directional additives: [0] the newest hit, [1] the one it replaced while it fades out */
	FHitLayer HitLayers[2];
	FDelegateHandle HitEventHandle;
	bool bHitEventPending = false;
	FHitLayer PendingHit;
};
