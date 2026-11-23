// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native roots of the Anim tags (anim-layers.md section 4, owner GSTNAnimation).
 *
 * Anim.Set.* names a feature's animation slot (Anim.Set.Wall, Anim.Set.Spin...). The code that owns a feature declares its leaf
 * natively in its own module (the wall layer's Anim.Set.Wall lives in the project), so this plugin never names a feature.
 *
 * Anim.Trait.* are cosmetic traits that change some animations (body, build). Leaves are designer content in
 * Config/Tags/GSTNAnimation.ini and the game's own ini; code never names one, rules match them with tag queries.
 * No Anim tag ever reaches a sync state: all of this is presentation.
 */
namespace GSTNAnimTags
{
	/** Root of every animation tag */
	GSTNANIMATIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Anim);

	/** Root of the feature animation slots: UGSTN_AnimSetDefinition::SetTag is a leaf of it */
	GSTNANIMATIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Set);

	/** Root of the cosmetic traits matched by FGSTN_AnimSetRule::When */
	GSTNANIMATIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Trait);
}
