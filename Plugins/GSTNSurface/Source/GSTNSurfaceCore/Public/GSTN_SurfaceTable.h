// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_SurfaceTypes.h"

class UPhysicalMaterial;

/**
 * Lookup tables the resolution reads, baked once outside the simulation (traversal-surfaces.md §2.2 step 3, M4 H11):
 *
 * - physical material -> surface behaviour, keyed by the loaded material so a lookup inside the simulation never builds a path
 *   or loads an asset;
 * - Surface.Type.* tag <-> uint8 index, the only form of a surface type a sync state may hold (modularity-and-tags 4.7).
 *   Index 0 is "no type"; the other indices follow the tag names in lexical order, so every machine with the same tag set
 *   (already required by Network Prediction) gets the same indices.
 *
 * Plain C++ so tests can bake their own table; the running game uses the one UGSTN_SurfaceSettings owns.
 */
class GSTNSURFACECORE_API FGSTN_SurfaceTable
{
public:
	/** Highest index a surface type can have; types past it map to 0 and the bake logs an error */
	static constexpr int32 MaxTypeIndex = 255;

	/** Replaces the material entries. Materials must stay loaded while the table is used (the settings keep them referenced) */
	void BakeMaterials(const TMap<const UPhysicalMaterial*, FGSTN_SurfaceBehaviour>& Entries);

	/** Rebuilds the type indices from the Surface.Type.* tags registered in the tag manager */
	void BakeTypesFromTagManager();

	/** Rebuilds the type indices from an explicit list (sorted here, duplicates and invalid tags dropped) */
	void BakeTypes(TArray<FGameplayTag> Types);

	/** The entry of a physical material, or null */
	const FGSTN_SurfaceBehaviour* FindMaterial(const UPhysicalMaterial* Material) const;

	/** Index of a surface type tag (exact match), 0 for an empty or unknown tag */
	uint8 GetTypeIndex(const FGameplayTag& Type) const;

	/** Tag of a surface type index, empty for 0 or an unknown index */
	FGameplayTag GetTypeByIndex(uint8 Index) const;

	/** Number of known surface types, not counting index 0 */
	int32 NumTypes() const { return FMath::Max(0, Types.Num() - 1); }

	bool AreMaterialsBaked() const { return bMaterialsBaked; }
	bool AreTypesBaked() const { return bTypesBaked; }

	/** Forgets everything, so the next use bakes again */
	void Reset();

private:
	TMap<const UPhysicalMaterial*, FGSTN_SurfaceBehaviour> Materials;

	/** Index -> tag; element 0 is the empty tag */
	TArray<FGameplayTag> Types;

	TMap<FGameplayTag, uint8> TypeIndices;

	bool bMaterialsBaked = false;
	bool bTypesBaked = false;
};
