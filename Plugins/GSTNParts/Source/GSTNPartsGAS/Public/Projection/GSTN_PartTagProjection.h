// Destructible parts, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartDefinition.h"
#include "GameplayTagContainer.h"
#include "Math/GSTN_PartsProjection.h"

class UAbilitySystemComponent;

/**
 * Turns the derived projection of the part math (FGSTN_PartsProjection: flags, granted and removed tags) into the tags on the
 * ability system, for the tick extension of the NP half (combat-parts.md §3.3, §7). Static and memberless, a pure function of the
 * projection, so a resimulated frame ends with the same tags and nothing has to be undone by a rollback.
 *
 * The tags are SET, never added: the loose count of every tag this extension may own is written to 0 or 1 each frame. The set of
 * tags it may own (the universe) is derived from the data (the three native tags, every tag a Tags break effect of the part set
 * can grant, and the base tags the caller passes), so a tag that stopped being projected (a restored part) is cleared without any
 * memory of the previous frame. A tag in the universe must therefore not be granted by anyone else: the Combat.Part.* tags are the
 * plugin's own, the Combat.Part.Effect.* leaves are granted by the Tags break effects of the content, and the base tags are the
 * ones the caller hands this extension to own.
 */
struct GSTNPARTSGAS_API FGSTN_PartTagProjection
{
	/**
	 * The tags the entity shows for a projection: BaseTags minus the Removed tags (a removed parent suppresses its children), plus
	 * the Granted tags, plus the native Combat.Part.Broken, GrabLocked and AllBroken the flags stand for. Out is reset first
	 */
	static void BuildTags(const FGSTN_PartsProjection& Projection, const FGameplayTagContainer& BaseTags, FGameplayTagContainer& Out);

	/** Every tag the projection of this part set can ever show, whatever the mask: the native three, all Granted tags of Tags break effects, and BaseTags */
	static void GetUniverse(TConstArrayView<FGSTN_PartDefinition> Parts, const FGameplayTagContainer& BaseTags, FGameplayTagContainer& Out);

	/** Sets every tag of the universe on ASC to 1 when BuildTags shows it and 0 when not. Returns the number of tags that changed count */
	static int32 Apply(UAbilitySystemComponent& ASC, TConstArrayView<FGSTN_PartDefinition> Parts, const FGSTN_PartsProjection& Projection, const FGameplayTagContainer& BaseTags);
};
