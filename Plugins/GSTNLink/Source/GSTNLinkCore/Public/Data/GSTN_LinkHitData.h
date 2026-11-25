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

	/**
	 * A hit on the held partner that also wears the grab down (grab spin, owner 2026-11-25: the held enemy frees itself after a while): the partner's
	 * escape frame moves this many frames closer, never before the frame the hit lands (UGSTN_LinkMath::MashProgress's floor). Only on the live Body
	 * partner of the same link; 0 = no effect. Smash keeps the grab time apart from pummels; a spin is a different deal: each turn costs grab time, so
	 * the partner cannot be swung for ever
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 GrabTimeCostFrames = 0;
};
