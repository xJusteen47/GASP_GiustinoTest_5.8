// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Hits/GSTN_HitShape.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "GSTN_HitShapeUtils.generated.h"

/**
 * Stateless shape queries of the hit system (combat-m4-hits 3). Everything is plain double math over the inputs, with no
 * time, no random and no global state, so the same inputs give the same bits on every machine and in a resimulation. A
 * shape is placed by Shape.Local followed by the world transform of its carrier; shapes that only touch overlap.
 */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_HitShapeUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Most sub-steps a swept test takes between two frames. Past this a very fast hitbox can still tunnel; fast ones are authored with longer shapes */
	static constexpr int32 MaxSweepSubsteps = 4;

	/** Do the two shapes overlap, in every pair of sphere, capsule and box (box against box by separating axes)? */
	UFUNCTION(BlueprintPure, Category = "GSTN|Hits")
	static bool Overlap(const FGSTN_HitShape& A, const FTransform& AWorld, const FGSTN_HitShape& B, const FTransform& BWorld);

	/** N = ceil(distance the shape's centre travels / smallest extent of the shape), in [1, MaxSweepSubsteps] */
	UFUNCTION(BlueprintPure, Category = "GSTN|Hits")
	static int32 GetSweepSubsteps(const FGSTN_HitShape& Shape, const FTransform& PreviousWorld, const FTransform& CurrentWorld);

	/**
	 * Overlap tested at the N + 1 carrier transforms from PreviousWorld to CurrentWorld (location and scale linear, rotation
	 * by slerp) against a victim shape that stays where it is, so a fast hitbox does not jump over a thin hurtbox.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Hits")
	static bool SweptOverlap(const FGSTN_HitShape& A, const FTransform& APreviousWorld, const FTransform& ACurrentWorld, const FGSTN_HitShape& B, const FTransform& BWorld);

	/** The shape with its extent multiplied (the ShapeScale of a relationship variant) */
	UFUNCTION(BlueprintPure, Category = "GSTN|Hits")
	static FGSTN_HitShape ScaleShape(const FGSTN_HitShape& Shape, float Scale);

	/**
	 * The transform of a hitbox's path at Frame (frames since the start of its track): linear between the two keys around it,
	 * slerp for the rotation, the nearest key outside the keyed range. Keys must be sorted by Frame. No keys = identity.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Hits")
	static FTransform EvaluateKeys(const TArray<FGSTN_HitboxKey>& Keys, int32 Frame);

	/**
	 * Which hurtbox a hit lands on among the overlapped ones (indices into the set): the highest Priority, ties by the lowest
	 * index. INDEX_NONE if nothing overlapped. The caller removes intangible hurtboxes first.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Hits")
	static int32 PickHurtbox(const TArray<int32>& Overlapped, const UGSTN_HurtboxSetDefinition* Set);

	/** PickHurtbox over plain arrays, for code that holds no set asset. Indices from GSTNHurtboxIndex::ExtraIndexBase are not set hurtboxes (the set holds at most 128) */
	static int32 PickHurtboxIndex(TConstArrayView<int32> Overlapped, TConstArrayView<FGSTN_Hurtbox> Hurtboxes);

	/**
	 * PickHurtboxIndex over the set and the extras of the entity's snapshot. An overlapped index from GSTNHurtboxIndex::ExtraIndexBase is
	 * an extra (position = index - base). The highest Priority wins over both kinds; a tie goes to an extra over the set, then to the lower
	 * index. An extra that ignores HitType is skipped as if it had not overlapped. With no extras this is the overload above, bit for bit.
	 */
	static int32 PickHurtboxIndex(TConstArrayView<int32> Overlapped, TConstArrayView<FGSTN_Hurtbox> Hurtboxes, TConstArrayView<FGSTN_ExtraHurtbox> Extras, const FGameplayTag& HitType);
};
