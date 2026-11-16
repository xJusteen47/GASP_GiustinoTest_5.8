// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Settings/GSTN_CombatEditorSettings.h"

class UPhysicsAsset;
class USkeletalMesh;
struct FReferenceSkeleton;

/** What the generator may be told (combat-m5-editor 8). Everything is a plain value, so a test needs no settings object and no editor */
struct FGSTN_HurtboxGenOptions
{
	/**
	 * Leave out the bodies whose Physics Type is Kinematic. Off by default: a body the designer drew in the Physics Asset is a body, and "one hurtbox
	 * per primitive" is the count a designer can check by eye. Turn it on when the asset has kinematic bodies that only exist for ragdoll tuning
	 */
	bool bSkipKinematic = false;

	/** Multiplies every extent (a margin: 1.1 makes every hurtbox 10 % bigger). Not the transform scale, which stays 1 */
	float ScaleMargin = 1.f;

	/**
	 * Hurtboxes whose largest extent is below this many cm are merged into the hurtbox of the nearest ancestor bone that has a big one (the ancestor's
	 * shape grows to contain them). 0 = off. A hand of twenty finger bodies becomes the hand
	 */
	float MergeBelowCm = 0.f;

	/** Where the mesh sits in root space (the preview scene's placement). The generated shapes are root-relative, in the baker's frame */
	FTransform MeshToRoot = FTransform::Identity;

	/** Bone name fragments to zone tags, first match wins. FromSettings copies the settings' list */
	TArray<FGSTN_HurtboxZoneRule> ZoneRules;

	static FGSTN_HurtboxGenOptions FromSettings(const UGSTN_CombatEditorSettings& Settings);
};

/** What a generation produced and what it had to say about it */
struct FGSTN_HurtboxGenResult
{
	TArray<FGSTN_Hurtbox> Hurtboxes;

	/** Things the designer should look at (a convex body replaced by a capsule, a body whose bone the mesh lacks) */
	TArray<FText> Warnings;

	/** Bodies of the Physics Asset, and the primitives they hold */
	int32 BodyCount = 0;
	int32 PrimitiveCount = 0;
	int32 SkippedKinematic = 0;
	int32 SkippedNoBone = 0;
	int32 MergedSmall = 0;

	/** Convex and tapered primitives replaced by a bounding capsule */
	int32 BoundingFallbacks = 0;
};

/** How a regeneration treats the hurtboxes the set already has */
enum class EGSTN_RegenerateMode : uint8
{
	/** A hurtbox whose name the generation also makes stays exactly as it is; the new names are added. Hand edits are never lost */
	KeepEdits,

	/** A hurtbox whose name the generation also makes takes the new shape and bone but keeps its tags, priority and reaction scale */
	UpdateShapes,

	/** The set becomes the generation. Everything hand-made is gone (the designer chose Replace all) */
	ReplaceAll
};

/** What a regeneration did to the set */
struct FGSTN_RegenerateSummary
{
	int32 Kept = 0;
	int32 Added = 0;
	int32 Updated = 0;
	int32 Removed = 0;

	/** Hurtboxes of the set that the generation does not make: they stay (they may be hand-made), and the designer is told by name */
	TArray<FName> NotInGeneration;
};

/**
 * Hurtbox generation from a Physics Asset (combat-m5-editor 8). Pure: it reads a Physics Asset and a reference skeleton and returns hurtboxes,
 * touching no asset, so it runs in a test with an asset made in memory.
 *
 * Per primitive of every body setup: a sphyl becomes a Capsule, a sphere a Sphere, a box a Box; a convex body or a tapered capsule becomes the capsule
 * that bounds it (along the longest axis for a convex, along its own axis for a tapered one) with a warning. The shape's transform is the primitive's local
 * transform, then the bone's reference pose in component space, then the mesh placement: root-relative, in the same frame the socket baker uses, so a
 * hurtbox costs no bone transform at run time. The name is the bone's (bone_N for the second and later primitives of one bone), the zone tag the
 * settings' mapping suggests, empty when nothing matches. No bone name is assumed anywhere: a spider or a turret generates like a person.
 */
namespace GSTNHurtboxGenerator
{
	FGSTN_HurtboxGenResult FromPhysicsAsset(const UPhysicsAsset& PhysicsAsset, const FReferenceSkeleton& RefSkeleton, const FGSTN_HurtboxGenOptions& Options);

	/** The same over a skeletal mesh's reference skeleton */
	FGSTN_HurtboxGenResult FromPhysicsAsset(const UPhysicsAsset& PhysicsAsset, const USkeletalMesh& Mesh, const FGSTN_HurtboxGenOptions& Options);

	/** The zone tag the rules suggest for a bone name: the first rule whose fragment the name contains (case-insensitive). Invalid when none does */
	FGameplayTag SuggestZone(TConstArrayView<FGSTN_HurtboxZoneRule> Rules, FName BoneName);

	/**
	 * Grows a shape (in its own local space) until it contains a sphere: Center is where the sphere is in the shape's local space, Radius its radius.
	 * A sphere grows its radius; a capsule its radius and half height (axis Z); a box its half extents. The shape only ever grows
	 */
	void ExpandShapeToInclude(FGSTN_HitShape& Shape, const FVector& CenterInShapeSpace, double Radius);

	/** The radius of the sphere around the shape's centre that contains it, in the shape's own space (extents, not scale) */
	double GetBoundingRadius(const FGSTN_HitShape& Shape);

	/** The largest single extent, the number "small" is judged by */
	double GetLargestExtent(const FGSTN_HitShape& Shape);

	/**
	 * The set after a regeneration, by name (combat-m5-editor 8, plan Q10): see EGSTN_RegenerateMode. Order is kept: the existing hurtboxes stay where
	 * they are (a HurtboxIndex a hit carries is a position in this array) and new ones are appended
	 */
	TArray<FGSTN_Hurtbox> Merge(const TArray<FGSTN_Hurtbox>& Existing, const TArray<FGSTN_Hurtbox>& Generated, EGSTN_RegenerateMode Mode, FGSTN_RegenerateSummary* OutSummary = nullptr);
}
