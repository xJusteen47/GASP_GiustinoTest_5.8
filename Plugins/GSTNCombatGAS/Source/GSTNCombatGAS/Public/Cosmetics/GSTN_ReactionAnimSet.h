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

/** The side of the victim a hit came from (hit-react-directional) */
UENUM(BlueprintType)
enum class EGSTN_HitSide : uint8
{
	Front,
	Back,
	Left,
	Right,
};

/**
 * The 4 directional hit additives of one base reaction (ground or air), owner option B 2026-11-16 (handoffs/reazioni-ricerca.md): an
 * additive clip on the torso per side, weighted by the hit's intensity, drawn on top of whatever the base shows by the anim node
 * FGSTN_AnimNode_DirectionalHitAdditive. A side with no clip draws a PLACEHOLDER spine bend (PlaceholderAngleDeg), so the system can be
 * tested before the poses exist; it is not a pose and never goes to the owner as one. Presentation only.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_DirectionalHitAdditives
{
	GENERATED_BODY()

	/** Hit from the front (pushed backward). Additive clips (local space or mesh-space rotation offset) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional")
	TSoftObjectPtr<UAnimSequenceBase> Front;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional")
	TSoftObjectPtr<UAnimSequenceBase> Back;

	/** Hit on the victim's left side (pushed to its right) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional")
	TSoftObjectPtr<UAnimSequenceBase> Left;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional")
	TSoftObjectPtr<UAnimSequenceBase> Right;

	/** PLACEHOLDER bend of a side with no clip, at weight 1 (degrees over the whole spine chain). 0 = a side with no clip draws nothing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional|Placeholder", meta = (ClampMin = "0.0", ForceUnits = "deg"))
	float PlaceholderAngleDeg = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional|Placeholder", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float PlaceholderPeakSeconds = 0.07f;

	/** Length of the placeholder; a clip lasts its own length */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional|Placeholder", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float PlaceholderLengthSeconds = 0.4f;

	const TSoftObjectPtr<UAnimSequenceBase>& ForSide(EGSTN_HitSide Side) const;
};

/** Pure math of the directional hit additives, for the component and the tests */
struct GSTNCOMBATGAS_API FGSTN_DirectionalHitMath
{
	/**
	 * The side a hit pushing the victim along PushDirection came from, by dot product with the victim's forward and right (horizontal
	 * parts only). Ties go to Front/Back. A zero push = Front
	 */
	static EGSTN_HitSide SelectSide(const FVector& PushDirection, const FVector& VictimForward, const FVector& VictimRight);

	/** The world push of a side, as the cardinal direction it stands for (front hit = pushed backward) */
	static FVector SidePush(EGSTN_HitSide Side, const FVector& VictimForward, const FVector& VictimRight);

	/** The weight of a hit: Unstaggered for a hit that did not stagger, else Min..1 linear in hitstun frames up to FullFrames */
	static float IntensityWeight(bool bStaggered, int32 HitstunFrames, int32 FullFrames, float MinWeight, float UnstaggeredWeight);
};

/** Directional hit additives of a reaction set: which base (ground, air) they ride on and how strong a hit is */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_DirectionalHitSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional")
	bool bEnabled = true;

	/** On the ground base reaction (HitStand) and on hits that do not stagger */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional")
	FGSTN_DirectionalHitAdditives Ground;

	/** On the air base reaction (Tumble) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional")
	FGSTN_DirectionalHitAdditives Air;

	/** The owner carrying ANY of these uses the Air set (Combat.State.Tumble; set by Tools/GAS/setup_reaction_anims.py) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional")
	FGameplayTagContainer AirTags;

	/** No additive while the owner carries ANY of these (Combat.State.Knockdown, Combat.State.Splat: lying on the floor, splatted: the base pose owns the body); a running one fades out */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional")
	FGameplayTagContainer BlockedTags;

	/** Hitstun frames (simulation) that give weight 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional|Intensity", meta = (ClampMin = "1"))
	int32 FullWeightHitstunFrames = 40;

	/** Weight of the weakest staggering hit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional|Intensity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinWeight = 0.5f;

	/** Weight of a hit that only froze the victim (no hitstun): a flinch on top of whatever it was doing. 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional|Intensity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnstaggeredWeight = 0.3f;

	/**
	 * The side is re-read against the victim's facing for this long after the hit, then held: the back-hit turn (PASS) rotates the victim
	 * on the first reaction frame, after the hit event, and the side must match the body as drawn
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float SideSettleSeconds = 0.05f;

	/** While the victim is frozen by the hitstop the additive advances to this time and holds there (the hit pose), then plays out */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float HitstopHoldSeconds = 0.07f;

	/** A newer hit replaces the running additive; the old one fades out over this (the new one starts at full weight, owner rule 2026-11-14) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ReplaceFadeSeconds = 0.05f;

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

	/** Directional hit additives on the torso (front/back/left/right, ground and air), see FGSTN_DirectionalHitAdditives */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Directional")
	FGSTN_DirectionalHitSettings DirectionalHits;

	/** The index of the rule that wins for these tags, INDEX_NONE when none holds. Pure: highest Priority, then the first listed */
	UFUNCTION(BlueprintPure, Category = "Combat|Reaction")
	int32 SelectRule(const FGameplayTagContainer& OwnerTags) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
