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

	/**
	 * Blend from whatever was showing when the rule starts (60 fps frames). Owner rule (2026-11-14): the first frame of a new state must read
	 * as a clear change from the pose before, for feel, so keep it at 0-1; a soft blend in hides the hit
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 BlendInFrames = 1;

	/** Cross-fade from Animation to ThenAnimation (60 fps frames): only hides the join, the second clip is part of the same reaction */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ChainBlendFrames = 1;

	/**
	 * Blend back to the character's own animation when the rule stops holding, or at the end with EndBehaviour BlendOut (60 fps frames).
	 * The pose held during this blend is the last one shown (the animation stops advancing), so nothing past the reaction is drawn
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 BlendOutFrames = 6;

	/**
	 * The mesh starts this much higher than the capsule puts it and falls into place over EntryDropFrames (presentation only, the capsule never
	 * moves). For a clip whose first pose is lower than the one before it: the ground hit after a tumble played at pelvis height (react-hook-1
	 * review: an 85 cm drop in one frame). The first frame still shows the new pose (owner rule): only its height eases. 0 = none
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction|Entry", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float EntryDropCm = 0.f;

	/** Frames (60 fps) of the fall into place: accelerating, as a body falls */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction|Entry", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 EntryDropFrames = 5;

	/** The drop applies only when the rule shown just before is one of these (Tumble); empty = whatever was shown before */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction|Entry")
	TArray<FName> EntryDropAfterRules;

	/** Optional: a gameplay event on the owner (Combat.Event.Hit.Received) restarts the rule from its first animation while it holds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	FGameplayTag RestartEvent;

	/** True when the owner's tags satisfy this rule */
	bool Matches(const FGameplayTagContainer& OwnerTags) const;
};

/**
 * The tremble of a character frozen by hitstop (CP4, VFX checklist approved by the owner 2026-11-15): the victim shakes more than the attacker,
 * side to side RELATIVE TO THE SCREEN (with a free camera a shake along a world axis can point into the camera and vanish), with an amplitude
 * that fades out and grows with the camera distance. Only the mesh moves, as an offset on top of what Mover writes: hurtboxes and the capsule
 * stay where the simulation put them. Presentation only.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_HitstopShake
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Shake")
	bool bEnabled = true;

	/** Amplitude of the hit character (Combat.State.Hitstop with Hitstun or Tumble) at the reference camera distance. 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Shake", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float VictimAmplitudeCm = 3.f;

	/** Amplitude of the attacker (in hitstop without hitstun). 0 = none (the attacker's weight reads from the hitstop anim rate instead) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Shake", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float AttackerAmplitudeCm = 0.f;

	/** Side swaps per second over two (30 Hz at 60 fps = one side per drawn frame, the classic fighting game shake) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Shake", meta = (ClampMin = "1.0", ForceUnits = "Hz"))
	float FrequencyHz = 30.f;

	/** The amplitude falls linearly to zero over this time from the start of the hitstop (it ends anyway when the hitstop ends) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Shake", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float DecaySeconds = 0.2f;

	/** The camera distance the amplitudes are authored at; farther cameras scale them up (a small shake far away reads as nothing) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Shake", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float ReferenceCameraDistanceCm = 400.f;

	/** Cap of the distance scale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop Shake", meta = (ClampMin = "1.0"))
	float MaxDistanceScale = 3.f;

	/** Signed offset (cm, along screen right) Elapsed seconds into the hitstop. Pure: starts at +Amplitude (the first frame visibly moves) */
	static float Sample(float AmplitudeCm, float FrequencyHz, float DecaySeconds, float ElapsedSeconds);

	/** The amplitude scale at a camera distance: 1 up to the reference distance, then linear, capped */
	float DistanceScale(float CameraDistanceCm) const;
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

	/** The tremble while frozen by hitstop (victim and attacker), see FGSTN_HitstopShake */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitstop")
	FGSTN_HitstopShake HitstopShake;

	/** The index of the rule that wins for these tags, INDEX_NONE when none holds. Pure: highest Priority, then the first listed */
	UFUNCTION(BlueprintPure, Category = "Combat|Reaction")
	int32 SelectRule(const FGameplayTagContainer& OwnerTags) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
