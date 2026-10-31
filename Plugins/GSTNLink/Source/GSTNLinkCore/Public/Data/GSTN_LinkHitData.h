// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitExtensions.h"
#include "GSTN_LinkHitData.generated.h"

/**
 * The link part of a hit's data (m6 spec 8, pummel): an entry of FGSTN_HitData::ExtensionData, so the combat core's hit data grows no link
 * field. A hit of type Combat.HitType.Pummel behaves as if it carried this with bNoReaction set; another hit on the linked partner (a latch
 * attack that must not knock the ridden enemy away) opts in here.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Link Hit Data"))
struct GSTNLINKCORE_API FGSTN_LinkHitData : public FGSTN_HitExtension
{
	GENERATED_BODY()

	/**
	 * The hit applies its damage, meter and events through the pipeline but no reaction: no hitstun, no knockback, no flinch (the LinkRequest
	 * step scales them to zero), so the partner stays in the link
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	bool bNoReaction = false;
};
