// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Settings/GSTN_CombatEditorSettings.h"

class USkeleton;

/**
 * Left/right mirroring of hurtboxes (combat-m5-editor 8, owner answer 2A of 2026-10-23). Pure functions over names and shapes.
 *
 * Mirroring exists only where the skeleton has recognisable pairs: bone names that match under the name pairs in force (the settings' list, or
 * the list of that skeleton) and whose counterpart exists. A skeleton with none (a spider, a turret, a boss with a single arm) gets no mirror at all,
 * and a skeleton can be switched off in the settings even when some names happen to look like a pair. A name that carries a side marker but whose counterpart
 * is missing is reported, never guessed.
 *
 * The mirror plane contains the character's forward axis (a setting) and the up axis; it is the root's YZ plane when the character faces +X.
 */
namespace GSTNHurtboxMirror
{
	enum class ESide : uint8
	{
		None,
		Left,
		Right
	};

	/**
	 * The name on the other side. A fragment of the name must match one side of a pair on a word boundary (the end of the name, a non-letter, or a capital after
	 * a lower case letter), so "_l" pairs "arm_l" and "arm_l_1" but not "arm_lower". The case of the fragment is kept ("_L" gives "_R") unless the pair is
	 * case-sensitive. The first pair of the list that matches wins. False when no fragment matches: the name has no side.
	 */
	bool FindCounterpart(const FString& Name, TConstArrayView<FGSTN_MirrorNamePair> Pairs, FString& OutCounterpart, ESide* OutSide = nullptr);

	/** What the names of a skeleton (or of a set of hurtboxes) say about mirroring */
	struct FNameAnalysis
	{
		/** Name -> its counterpart, both directions, only where the counterpart is one of the names */
		TMap<FName, FName> Pairs;

		/** Names with a side marker whose counterpart is not among the names */
		TArray<FName> Unpaired;

		/** Names with no side marker at all (the spine, the head) */
		TArray<FName> Centre;

		bool IsMirrorable() const { return !Pairs.IsEmpty(); }
	};

	FNameAnalysis AnalyzeNames(TConstArrayView<FName> Names, TConstArrayView<FGSTN_MirrorNamePair> Pairs);

	/** Is mirroring on for this skeleton, which pairs apply, and why not when it is off */
	struct FResolvedConfig
	{
		bool bEnabled = false;
		TArray<FGSTN_MirrorNamePair> Pairs;
		FNameAnalysis Bones;

		/** The bone names were known (a mesh is shown), so a counterpart bone must exist; false = judged on the hurtbox names alone */
		bool bBonesKnown = false;

		/** Why it is off, or a sentence on what it found when it is on */
		FText Reason;
	};

	/**
	 * Mode and pairs of the skeleton's rule, the global pairs, the bone names (empty when no mesh is shown: the hurtbox names are then the only evidence,
	 * pass them as FallbackNames). Off by rule, or Auto with no pair among the names, is disabled
	 */
	FResolvedConfig ResolveConfig(EGSTN_SkeletonMirrorMode Mode, TConstArrayView<FGSTN_MirrorNamePair> SkeletonPairs, TConstArrayView<FGSTN_MirrorNamePair> GlobalPairs,
		TConstArrayView<FName> BoneNames, TConstArrayView<FName> FallbackNames);

	/** The same, reading the skeleton's rule from the settings (a null skeleton uses Auto and the global pairs) */
	FResolvedConfig ResolveConfig(const UGSTN_CombatEditorSettings& Settings, const USkeleton* Skeleton, TConstArrayView<FName> BoneNames, TConstArrayView<FName> FallbackNames);

	/** The plane's normal: the horizontal axis that is not the forward axis (Y when the character faces +X or -X) */
	EAxis::Type GetLateralAxis(EGSTN_MirrorForwardAxis Forward);

	/**
	 * The transform seen in a mirror: M T M where M flips the lateral axis. A point of the mirrored shape is the mirror of the point of the original, and
	 * the rotation stays a rotation (it is not a negative scale). Scale is kept
	 */
	FTransform MirrorTransform(const FTransform& Transform, EGSTN_MirrorForwardAxis Forward);

	FGSTN_HitShape MirrorShape(const FGSTN_HitShape& Shape, EGSTN_MirrorForwardAxis Forward);

	/** One hurtbox the mirror would write */
	struct FPlanEntry
	{
		int32 SourceIndex = INDEX_NONE;
		FGSTN_Hurtbox Mirrored;

		/** The hurtbox it replaces, INDEX_NONE when it is a new one */
		int32 ExistingIndex = INDEX_NONE;
	};

	struct FPlan
	{
		TArray<FPlanEntry> Entries;

		/** Selected hurtboxes that have a side marker but no counterpart (name or bone): reported, not mirrored */
		TArray<FName> Unpaired;

		/** Selected hurtboxes with no side marker: not mirrored, they are the middle */
		TArray<FName> Centre;

		/** Selected hurtboxes skipped because their counterpart is selected too and comes first (the first of two selected is the source) */
		TArray<FName> SkippedAsTarget;

		/** The names the plan overwrites, for the confirmation */
		TArray<FName> GetOverwrittenNames(const TArray<FGSTN_Hurtbox>& Set) const;
		bool NeedsConfirmation() const;
	};

	/** What mirroring the selected hurtboxes would do to the set. Writes nothing: the caller asks for confirmation when the plan overwrites */
	FPlan MakePlan(const TArray<FGSTN_Hurtbox>& Set, TConstArrayView<int32> Selected, const FResolvedConfig& Config, EGSTN_MirrorForwardAxis Forward);

	/** The set with the plan applied: overwritten hurtboxes replaced in place, new ones appended */
	TArray<FGSTN_Hurtbox> Apply(const TArray<FGSTN_Hurtbox>& Set, const FPlan& Plan);
}
