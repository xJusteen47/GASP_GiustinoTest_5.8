// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_PartBreakEffects.h"

#include "Math/GSTN_PartsProjection.h"
#include "Math/GSTN_PartsState.h"

void FGSTN_PartBreakEffect_Tags::Project(const FGSTN_PartsState& State, int32 PartIndex, FGSTN_PartsProjection& Out) const
{
	Out.Granted.AppendTags(Granted);
	Out.Removed.AppendTags(Removed);
}

void FGSTN_PartBreakEffect_DisableHurtbox::Project(const FGSTN_PartsState& State, int32 PartIndex, FGSTN_PartsProjection& Out) const
{
	for (const FName& Hurtbox : Hurtboxes)
	{
		Out.DisabledHurtboxes.AddUnique(Hurtbox);
	}
}

void FGSTN_PartBreakEffect_DisableHitGroups::Project(const FGSTN_PartsState& State, int32 PartIndex, FGSTN_PartsProjection& Out) const
{
	Out.HitGroupMask |= GroupMask;
}
