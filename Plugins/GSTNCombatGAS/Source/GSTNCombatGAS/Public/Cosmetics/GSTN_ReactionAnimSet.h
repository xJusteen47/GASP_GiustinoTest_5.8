// Ability-system layer of the combat framework.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSTN_ReactionAnimSet.generated.h"

class UAnimSequenceBase;

/** What the last animation of a reaction rule does when it reaches its end while the rule still holds */
UENUM(BlueprintType)
enum class EGSTN_ReactionAnimEnd : uint8
{
	/** Holds the last frame until the rule stops holding (lying on the floor, pinned to a wall) */
	HoldLastPose,
	/** Plays again from its start until the rule stops holding (a tumble spin) */
	Loop,
	/** Blends back to the character's own animation over BlendOutFrames, even if the rule still holds */
	BlendOut,
};

/**
 * One victim reaction as data: "while the owner's tags match, play this animation". The tags are the projected combat states
 * (Combat.State.Hitstun, Combat.State.Tumble, Combat.State.Splat, Combat.State.Knockdown, ...), so the rule never names a state in code.
 * Presentation only: nothing here is read by the simulation, and a missing animation only means no reaction pose.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_ReactionAnimRule
{
	GENERATED_BODY()

	/** For logs and the debugger only */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	FName Name;

	/** The owner must carry ALL of these. Empty never matches (a rule for "always" would hide the character's own animation) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	FGameplayTagContainer RequiredTags;

	/** The rule does not hold while the owner carries ANY of these */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	FGameplayTagContainer BlockedTags;

	/** Of the rules that hold, the highest wins (Splat above Hitstun, say); equal priorities: the first listed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	int32 Priority = 0;

	/** An animation sequence (played as a dynamic montage on the set's slot) or a montage asset */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TSoftObjectPtr<UAnimSequenceBase> Animation;

	/**
	 * Optional: played right after Animation while the rule still holds, cross-faded over ChainBlendFrames and started before Animation ends,
	 * so the character's own pose never shows between the two (the ground hit, then lying on the floor). EndBehaviour applies to it
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TSoftObjectPtr<UAnimSequenceBase> ThenAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	EGSTN_ReactionAnimEnd EndBehaviour = EGSTN_ReactionAnimEnd::HoldLastPose;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = "0.01"))
	float PlayRate = 1.f;

	/** Blend from whatever was showing when the rule starts (60 fps frames) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 BlendInFrames = 4;

	/** Cross-fade from Animation to ThenAnimation (60 fps frames) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ChainBlendFrames = 2;

	/**
	 * Blend back to the character's own animation when the rule stops holding, or at the end with EndBehaviour BlendOut (60 fps frames).
	 * The pose held during this blend is the last one shown (the animation stops advancing), so nothing past the reaction is drawn
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 BlendOutFrames = 6;

	/** Optional: a gameplay event on the owner (Combat.Event.Hit.Received) restarts the rule from its first animation while it holds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	FGameplayTag RestartEvent;

	/** True when the owner's tags satisfy this rule */
	bool Matches(const FGameplayTagContainer& OwnerTags) const;
};

/**
 * The victim reactions of a character, a kit or a skin (owner 2026-11-14: "state -> montage via data, not hardcoded"). Read by
 * UGSTN_ReactionAnimComponent. Swapping the asset swaps every reaction; the simulation never sees it.
 */
UCLASS(BlueprintType)
class GSTNCOMBATGAS_API UGSTN_ReactionAnimSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The montage slot the sequences play on (the one the character's anim graph blends full body) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	FName SlotName = TEXT("DefaultSlot");

	/**
	 * A montage this set did not start (an ability's timeline montage: the ground bounce, the get-ups) wins: no reaction starts while one
	 * plays, and a reaction it interrupts is not started again
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	bool bYieldToOtherMontages = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TArray<FGSTN_ReactionAnimRule> Rules;

	/** The index of the rule that wins for these tags, INDEX_NONE when none holds. Pure: highest Priority, then the first listed */
	UFUNCTION(BlueprintPure, Category = "Combat|Reaction")
	int32 SelectRule(const FGameplayTagContainer& OwnerTags) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
