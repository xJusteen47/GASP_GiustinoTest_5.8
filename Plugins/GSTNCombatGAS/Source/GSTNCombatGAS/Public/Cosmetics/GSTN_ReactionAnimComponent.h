// Ability-system layer of the combat framework.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GSTN_ReactionAnimComponent.generated.h"

class UAbilitySystemComponent;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class USkeletalMeshComponent;
class UGSTN_ReactionAnimSet;
struct FGameplayEventData;
struct FGSTN_ReactionAnimRule;

/**
 * Plays the victim reaction animations from data (UGSTN_ReactionAnimSet): every frame it reads the owner's gameplay tags (the combat
 * states the combat tick projects), picks the winning rule and plays its animation on the character's montage slot.
 *
 * Presentation only, on every machine that draws the character (nothing on a dedicated server): it never writes a tag, an attribute or
 * the simulation, so a rollback only changes which rule wins on the next frame. An ability that plays its own timeline montage (the
 * ground bounce, the get-ups) wins over it (bYieldToOtherMontages), so the frame-synced montages of the simulation keep their timing.
 *
 * Add it to the character Blueprint with Tools/GAS/setup_reaction_anims.py (never by hand), or at run time with AddReactionComponent.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GSTNCOMBATGAS_API UGSTN_ReactionAnimComponent : public UActorComponent
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

	/** Adds the component to Actor (or returns the one it has) with Set; for scripts and tests */
	UFUNCTION(BlueprintCallable, Category = "Combat|Reaction")
	static UGSTN_ReactionAnimComponent* AddReactionComponent(AActor* Actor, UGSTN_ReactionAnimSet* Set);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Loads the set and its animations (presentation: a synchronous load at begin play or on a swap) and binds the restart events */
	void ApplySet(UGSTN_ReactionAnimSet* NewSet);
	void UnbindEvents();
	void OnRestartEvent(const FGameplayEventData* Payload, FGameplayTag Tag);

	UAnimInstance* ResolveAnimInstance();
	UAbilitySystemComponent* ResolveAbilitySystem();

	/** Starts the stage (0 = Animation, 1 = ThenAnimation) of the rule; false when it has no animation there */
	bool PlayStage(UAnimInstance& Anim, const FGSTN_ReactionAnimRule& Rule, int32 Stage, float BlendSeconds);

	/** Stops the montage this component plays, holding its last shown pose through the blend */
	void StopActive(UAnimInstance* Anim, float BlendSeconds);

	/** A montage not started by this component is playing and not blending out */
	bool IsOtherMontagePlaying(const UAnimInstance& Anim) const;

	/** Starts the rule's entry drop when it applies (the rule shown before is in EntryDropAfterRules) */
	void BeginEntryDrop(const FGSTN_ReactionAnimRule& Rule, FName PreviousRule);

	/** Writes this frame's height of the entry drop on the mesh (0 at its end), as a difference over whatever Mover put there */
	void UpdateEntryDrop(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_ReactionAnimSet> LoadedSet;

	/** Keeps the rules' animations in memory while the set is applied */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequenceBase>> LoadedAnimations;

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

	/** The offset written on the mesh last and the relative location it gave: if the location changed since, Mover reset it and the offset is gone */
	float AppliedDropCm = 0.f;
	FVector LastWrittenLocation = FVector::ZeroVector;
};
