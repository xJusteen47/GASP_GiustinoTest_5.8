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
 *   (already required by Network Prediction) gets the same indices. A tag added later (a Game Feature plugin, another ini)
 *   would shift the indices after it, so the running game freezes the table once the engine has started, and GetTypesChecksum
 *   lets two machines check that they agree.
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

	/** Rebuilds the type indices from the Surface.Type.* tags registered in the tag manager. Returns false when the table is frozen and the tags no longer match it */
	bool BakeTypesFromTagManager();

	/**
	 * Rebuilds the type indices from an explicit list (sorted here, duplicates and invalid tags dropped). A frozen table keeps its
	 * indices: a list that would give a different table is refused with an error in the log and false is returned, a list that gives
	 * the same table is accepted without change
	 */
	bool BakeTypes(TArray<FGameplayTag> Types);

	/**
	 * Locks the type indices for the rest of the session (the running game does it once the engine has started). The indices are
	 * what a sync state stores, so they must not move while a match runs; Reset is the only way back. The editor never freezes:
	 * a designer adding a Surface.Type tag has to see its index at once.
	 *
	 * An empty table is never frozen (returns false): a bake that ran before the tag tree was complete would otherwise lock "no
	 * types" for the whole session and refuse every Surface.Type leaf that registers afterwards. Freeze again after a bake that
	 * found at least one type
	 */
	bool Freeze()
	{
		if (!bTypesBaked || NumTypes() == 0)
		{
			return false;
		}
		bTypesFrozen = true;
		return true;
	}
	bool IsFrozen() const { return bTypesFrozen; }

	/**
	 * A 32-bit checksum of the ordered type list (the tag names in index order, as UTF-8). Two machines with the same checksum
	 * agree on every index. Compare it at connect or log it to find a client whose Surface.Type tags differ from the server
	 */
	uint32 GetTypesChecksum() const { return TypesChecksum; }

	/** The checksum a list of types would have once baked (sorted, deduplicated), without baking it */
	static uint32 ComputeTypesChecksum(TArray<FGameplayTag> Types);

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

	/** Forgets the material entries only (the settings changed): the type indices, and their freeze, stay */
	void ResetMaterials();

	/** Forgets everything, the freeze included, so the next use bakes again */
	void Reset();

private:
	TMap<const UPhysicalMaterial*, FGSTN_SurfaceBehaviour> Materials;

	/** Index -> tag; element 0 is the empty tag */
	TArray<FGameplayTag> Types;

	TMap<FGameplayTag, uint8> TypeIndices;

	uint32 TypesChecksum = 0;

	bool bMaterialsBaked = false;
	bool bTypesBaked = false;
	bool bTypesFrozen = false;
};
