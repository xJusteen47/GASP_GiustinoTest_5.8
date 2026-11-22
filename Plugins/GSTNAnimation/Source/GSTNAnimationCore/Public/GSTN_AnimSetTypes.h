// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSTN_AnimSetTypes.generated.h"

class UAnimInstance;
class UAnimMontage;

/**
 * A presentation replacement of one montage. The simulation (timelines, hit and hurt windows, baked sockets, Mover queued moves)
 * only ever names Reference; the mesh plays Visual instead. So a variant may look different but must keep the same timing:
 * same section names, section lengths within GSTNAnimSet::MaxSectionLengthDelta, same rate scale, and no root motion when the
 * reference has none (UGSTN_AnimSetUtils::ValidateMontageOverride, run by the data validation of the set)
 */
USTRUCT(BlueprintType)
struct GSTNANIMATIONCORE_API FGSTN_MontageOverride
{
	GENERATED_BODY()

	/** The montage the feature's simulation data names */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> Reference;

	/** What the mesh plays instead */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> Visual;
};

/**
 * The animation of one feature (a wall mode, a spin, a drill): the linked layer class that draws it and the montage replacements.
 * A variant (male / female, heavy / slim) is another set with the same SetTag, whose LayerClass is usually a child anim Blueprint of
 * the base layer that only swaps clips. Chosen by FGSTN_AnimSetRule in collections; presentation only. Primary asset type GSTN_AnimSet.
 */
UCLASS(BlueprintType)
class GSTNANIMATIONCORE_API UGSTN_AnimSetDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Feature slot this set fills, a leaf under Anim.Set (declared by the feature's owner) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (Categories = "Anim.Set"))
	FGameplayTag SetTag;

	/**
	 * Linked anim layer class (implements the feature's layer interface) linked on the mesh while this set wins its slot. Empty = the set
	 * only overrides montages and leaves the layer of a lower source (or the graph's default) in place
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftClassPtr<UAnimInstance> LayerClass;

	/** Presentation replacements of the feature's montages */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TArray<FGSTN_MontageOverride> MontageOverrides;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

/** One conditional choice: use Set when the cosmetic tags match When */
USTRUCT(BlueprintType)
struct GSTNANIMATIONCORE_API FGSTN_AnimSetRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UGSTN_AnimSetDefinition> Set;

	/** Against the owner's cosmetic tags (skin tags and Anim.Trait.* leaves). Empty = always */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FGameplayTagQuery When;

	/** Within one source, the higher priority wins its slot; on a tie the rule listed first wins */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	int32 Priority = 0;
};

/**
 * The anim set rules one source brings (the character's defaults, a kit, a weapon, a skin). Rules for different slots live side by side;
 * e.g. a skin collection: "Anim.Set.Wall heavy variant when Anim.Trait.Build.Heavy", "Anim.Set.Spin female variant when Anim.Trait.Body.Female"
 */
UCLASS(BlueprintType)
class GSTNANIMATIONCORE_API UGSTN_AnimSetCollection : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TArray<FGSTN_AnimSetRule> Rules;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

/** A collection registered on UGSTN_AnimSetComponent under a source name, with the source's priority */
USTRUCT(BlueprintType)
struct GSTNANIMATIONCORE_API FGSTN_AnimSetSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FName SourceName = NAME_None;

	/** Higher sources win over lower ones whatever the rule priorities. Defaults in GSTNAnimSet (character < kit < weapon < skin) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	int32 SourcePriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UGSTN_AnimSetCollection> Collection;
};

namespace GSTNAnimSet
{
	/** Primary asset types */
	GSTNANIMATIONCORE_API extern const FPrimaryAssetType AnimSetType;
	GSTNANIMATIONCORE_API extern const FPrimaryAssetType AnimSetCollectionType;

	/** Default source priorities: the usual layering, a game may use any numbers */
	constexpr int32 PriorityCharacter = 0;
	constexpr int32 PriorityKit = 100;
	constexpr int32 PriorityWeapon = 200;
	constexpr int32 PrioritySkin = 300;

	/** Largest section length difference a montage override may have (s): one frame at 30 fps */
	constexpr float MaxSectionLengthDelta = 1.f / 30.f;
}
