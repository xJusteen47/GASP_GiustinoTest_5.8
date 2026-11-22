// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GSTN_StanceTypes.generated.h"

class UAnimSequence;

/** When the upper body starts going back to the chosen stance after a move (owner 2026-11-16: both behaviours, as a setting) */
UENUM(BlueprintType)
enum class EGSTN_StanceReturnTiming : uint8
{
	/** With the feet: once the montage has fully blended out and no foot is still planted by the blend-out foot lock (the steps start) */
	WithFeet,
	/** Slightly before the feet: as soon as the montage has fully blended out, while the planted feet are still waiting for the game pose to settle */
	BeforeFeet
};

/** How much of the stance one bone (and, by default, its children) carries, and how slowly it gets there */
USTRUCT(BlueprintType)
struct GSTNANIMATIONCORE_API FGSTN_StanceBoneWeight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance")
	FName Bone = NAME_None;

	/** Share of the stance offset this bone receives (0..1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance", meta = (ClampMin = "0", ClampMax = "1"))
	float Weight = 1.f;

	/** Multiplies FadeInTime for this bone: above 1 the bone arrives later (torso slower than arms) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance", meta = (ClampMin = "0.1", ClampMax = "4"))
	float FadeTimeScale = 1.f;

	/** Bones below it that are not listed take the same weight and time scale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance")
	bool bIncludeChildren = true;
};

/**
 * A chosen idle stance for the upper body (torso, neck, arms), applied as a local-space additive on top of the game's idle: the offset is
 * StancePose minus ReferencePose (the idle the moves were authored for, normally GASP's M_Neutral_Stand_Idle_Loop), each sampled at one time.
 * The feet are not touched (the blend-out foot lock handles them); bones that are not listed (root, pelvis, legs, IK bones) get no offset.
 *
 * Like FFXIV's chosen idle poses, the stance lives only at rest: it is off while a montage drives the pose and while the capsule moves, and
 * it comes back after a move with ReturnTiming, ReturnDelay and FadeInTime. So GASP locomotion never sees it. One asset per kit (or skin),
 * selected with UGSTN_CharacterAnimInstance::SetStance. Both poses must be plain (non-additive) sequences of the same skeleton.
 */
UCLASS(BlueprintType)
class GSTNANIMATIONCORE_API UGSTN_StanceDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UGSTN_StanceDefinition();

	/** The chosen idle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance|Poses")
	TObjectPtr<UAnimSequence> StancePose;

	/** The idle the moves were made for: the offset is measured from it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance|Poses")
	TObjectPtr<UAnimSequence> ReferencePose;

	/** Time sampled in StancePose (one static pose: the game idle keeps its own breathing under it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance|Poses", meta = (ClampMin = "0", ForceUnits = "s"))
	float StancePoseTime = 0.f;

	/** Time sampled in ReferencePose */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance|Poses", meta = (ClampMin = "0", ForceUnits = "s"))
	float ReferencePoseTime = 0.f;

	/** Which bones carry the offset. Default: rising along the spine (0.3 to 1.0), full on neck and arms, torso 1.3x slower than arms */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance|Bones")
	TArray<FGSTN_StanceBoneWeight> BoneWeights;

	/** When the upper body starts going back after a move */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance|Return")
	EGSTN_StanceReturnTiming ReturnTiming = EGSTN_StanceReturnTiming::WithFeet;

	/** Extra wait before the stance returns (FFXIV-style neutral time after a move); 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance|Return", meta = (ClampMin = "0", ForceUnits = "s"))
	float ReturnDelay = 0.f;

	/** Time of the return for a bone with FadeTimeScale 1 (smoothstep: no pop at either end) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance|Return", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float FadeInTime = 0.25f;

	/** Time to drop the stance when the capsule starts moving */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance|Return", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float FadeOutTime = 0.15f;

	/** Blend from the last output pose when the stance asset is switched while it shows */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stance|Return", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float SwitchBlendTime = 0.2f;

	/** Both poses set, plain sequences of the same skeleton */
	bool HasCompatiblePoses(FText* OutReason = nullptr) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
