// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Data/GSTN_PartDefinition.h"
#include "Data/GSTN_PartsBakedCurve.h"
#include "Data/GSTN_PartsLimits.h"
#include "Engine/DataAsset.h"
#include "GSTN_PartSetDefinition.generated.h"

class UGSTN_PenetrationTable;
class USkeletalMesh;

/**
 * The parts of one kind of entity (combat-parts.md §3.1): a player fighter, an enemy, a boss. One asset per entity type,
 * referenced by the kit (FGSTN_PartsKitExtension) or by a minion definition. A player's set is a single part whose break is
 * death; an enemy's has several. Up to GSTN_PartsLimits::MaxParts parts.
 *
 * The set maps parts to hurtbox names of the entity's hurtbox set and leaves that set unchanged, so combat without parts is
 * untouched. Everything the simulation reads (the parts, the table, the baked weight curve) is resolved before the pawn's first
 * simulation tick and must be identical on every machine; presentation lives in a separate asset (added with the presentation
 * module, which Core cannot name) and is never read by the simulation.
 */
UCLASS(BlueprintType)
class GSTNPARTSCORE_API UGSTN_PartSetDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UGSTN_PartSetDefinition();

	/** Index in this array is the part index used everywhere (state, events, masks). Reordering parts is a data migration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	TArray<FGSTN_PartDefinition> Parts;

	/** The penetration rule for this entity. Empty = the rules' default table, then the settings', then the built-in three bands */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	TSoftObjectPtr<UGSTN_PenetrationTable> Table;

	/** The hitstun condition, reaction cap and juggle of a hit on the body (no part), and the parts' default */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	FGSTN_PartReaction BodyReaction;

	/** Which broken parts make the entity grabbable. Empty = never locked */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	FGSTN_GrabGate GrabGate;

	/** Use WeightCurve instead of the default (linear from 1 to the settings' scale at the cap) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weight")
	bool bOverrideWeightCurve = false;

	/**
	 * Weight scale over Progress (damage taken over the cap, 0..1), values 0..1. Baked at load into integers, so every machine
	 * reads the same number. A flat curve makes an entity that never gets lighter
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weight", meta = (EditCondition = "bOverrideWeightCurve"))
	FRuntimeFloatCurve WeightCurve;

#if WITH_EDITORONLY_DATA
	/** Mesh the bone names are checked against by validation. Editor only, never read at run time */
	UPROPERTY(EditAnywhere, Category = "Parts|Editor")
	TSoftObjectPtr<USkeletalMesh> PreviewMesh;
#endif

	/** The parts the math may address: the first GSTN_PartsLimits::MaxParts (validation rejects more) */
	TConstArrayView<FGSTN_PartDefinition> GetParts() const { return MakeArrayView(Parts.GetData(), FMath::Min(Parts.Num(), GSTN_PartsLimits::MaxParts)); }

	/** Index of the part with this name, or INDEX_NONE */
	int32 FindPartByName(FName PartName) const;

	/** Index of the part that owns this hurtbox name, or INDEX_NONE (the hurtbox belongs to the body) */
	int32 FindPartByHurtbox(FName HurtboxName) const;

	/** Bit i set for every part with a cap above zero (the parts that can break) */
	uint16 GetBreakableMask() const;

	/** The baked weight curve: the set's own when bOverrideWeightCurve, else the default shape from the settings */
	const FGSTN_PartsBakedCurve& GetWeightCurve() const { return BakedWeightCurve; }

	/**
	 * Rebuilds the baked weight curve from WeightCurve or the settings. Runs on load and after an editor edit; call it after
	 * changing the curve or the settings at run time, outside the simulation
	 */
	void RebakeWeightCurve();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** Not serialised: derived from the curve and the settings on load */
	FGSTN_PartsBakedCurve BakedWeightCurve;
};
