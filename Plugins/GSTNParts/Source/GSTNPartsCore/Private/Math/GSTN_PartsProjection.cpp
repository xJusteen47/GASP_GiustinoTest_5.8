// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "Math/GSTN_PartsProjection.h"

#include "GSTN_PartsTags.h"

void FGSTN_PartsProjection::Reset()
{
	Granted.Reset();
	Removed.Reset();
	DisabledHurtboxes.Reset();
	HitGroupMask = 0;
	bAnyBroken = false;
	bAllBroken = false;
	bGrabLocked = false;
}

void FGSTN_PartsProjection::AppendNativeTags(FGameplayTagContainer& OutTags) const
{
	if (bAnyBroken)
	{
		OutTags.AddTag(GSTN_PartsTags::Part_Broken);
	}
	if (bGrabLocked)
	{
		OutTags.AddTag(GSTN_PartsTags::Part_GrabLocked);
	}
	if (bAllBroken)
	{
		OutTags.AddTag(GSTN_PartsTags::Part_AllBroken);
	}
}
