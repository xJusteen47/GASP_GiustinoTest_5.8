// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "GSTN_GroundContactSettings.generated.h"

class UAnimMontage;

/** How a contact bone is kept above the floor */
UENUM(BlueprintType)
enum class EGSTN_GroundContactRole : uint8
{
	/** Body bone (pelvis, spine, head, knee, elbow): its penetration lifts the whole body (the lift bone moves along the floor normal) */
	LiftBody,
	/** End of a two-bone chain (hand, foot): after the lift, a two-bone IK puts it back on the floor if it still sinks, or keeps it
	 *  on the floor if it was touching before the lift */
	LimbEnd
};

/** One bone the correction watches: a sphere of Radius around its origin must stay above the floor */
USTRUCT(BlueprintType)
struct GSTNANIMATIONCORE_API FGSTN_GroundContactBone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Contact")
	FName Bone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Contact")
	EGSTN_GroundContactRole Role = EGSTN_GroundContactRole::LiftBody;

	/** Distance (cm) from the bone origin to the skin on the floor side: the origin is kept at least this high above the floor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Contact", meta = (ClampMin = "0", ForceUnits = "cm"))
	float Radius = 8.f;
};

/**
 * When the correction is on: a rule matches while the owner has ANY of AnyTags (owned gameplay tags of the actor or of a component that
 * implements IGameplayTagAssetInterface, e.g. the ability system component) OR one of Montages is playing. A rule with neither never matches.
 * The weight is the largest matching Weight; it blends in and out over the matching rule's times.
 */
USTRUCT(BlueprintType)
struct GSTNANIMATIONCORE_API FGSTN_GroundContactRule
{
	GENERATED_BODY()

	/** Label for the editor list only */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Contact")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Contact")
	FGameplayTagContainer AnyTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Contact")
	TArray<TObjectPtr<UAnimMontage>> Montages;

	/** 0..1: share of the correction (1 = no penetration left) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Contact", meta = (ClampMin = "0", ClampMax = "1"))
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Contact", meta = (ClampMin = "0", ForceUnits = "s"))
	float BlendInTime = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Contact", meta = (ClampMin = "0", ForceUnits = "s"))
	float BlendOutTime = 0.15f;
};

/**
 * Tuning of the ground contact correction (FGSTN_AnimNode_GroundContactFix): a post-process at the end of the main anim graph that only
 * removes floor penetration from whatever pose the graph made (reaction montages, knockdown, ground bounce). It never replaces the authored
 * animation: a pose that does not sink passes through unchanged, and a pose that sinks is moved only by the depth it sinks.
 * Presentation only: never read by the simulation, never replicated.
 */
UCLASS(BlueprintType)
class GSTNANIMATIONCORE_API UGSTN_GroundContactSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	UGSTN_GroundContactSettings();

	/** States (tags or montages) where the correction is on. Everywhere else the node is a pass-through and does no trace */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "When", meta = (TitleProperty = "Name"))
	TArray<FGSTN_GroundContactRule> Rules;

	/** Optional anim curve: a montage that writes it (0..1) turns the correction on for those frames, on top of the rules. None = unused */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "When")
	FName CurveName = TEXT("GSTN_GroundContact");

	/** Bones kept above the floor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bones", meta = (TitleProperty = "Bone"))
	TArray<FGSTN_GroundContactBone> Bones;

	/** Bone moved along the floor normal to lift the body (its children follow) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bones")
	FName LiftBone = TEXT("pelvis");

	/** Largest body lift (cm): deeper sinks are game bugs (capsule), not something to hide */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lift", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxLift = 35.f;

	/** The lift goes up at once (no frame shows the body in the floor) and settles back down with this half-life */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lift", meta = (ClampMin = "0", ForceUnits = "s"))
	float LiftReleaseHalfLife = 0.05f;

	/** A limb end that touched the floor before the lift (within ContactBand) stays on the floor after it (no floating hand or foot) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limbs")
	bool bKeepLimbContacts = true;

	/** Height above its radius (cm) under which a limb end counts as touching the floor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limbs", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ContactBand = 3.f;

	/** Largest move of a limb end by the IK (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Limbs", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxLimbCorrection = 30.f;

	/** Floor trace: from TraceStartBone (last frame) TraceUp above to TraceDown below, along the owner's down. One line trace a frame, only
	 *  while the correction is on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor")
	FName TraceStartBone = TEXT("pelvis");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor", meta = (ClampMin = "0", ForceUnits = "cm"))
	float TraceUp = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor", meta = (ClampMin = "0", ForceUnits = "cm"))
	float TraceDown = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Floors steeper than this (degrees from the owner's up) are ignored: walls are not floors */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Floor", meta = (ClampMin = "0", ClampMax = "89"))
	float MaxFloorSlopeDeg = 50.f;

	/** For setup scripts: adds the rule called Name, or replaces it (idempotent). Unknown tag names are skipped and returned in OutUnknownTags */
	UFUNCTION(BlueprintCallable, Category = "Ground Contact")
	void SetRule(const FString& Name, const TArray<FName>& TagNames, const TArray<UAnimMontage*>& InMontages, float Weight, TArray<FName>& OutUnknownTags);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
