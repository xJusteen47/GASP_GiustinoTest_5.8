#include "GSTN_ExperienceActionSet.h"
#include "GameFeatureAction.h"

FPrimaryAssetId UGSTN_ExperienceActionSet::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GetAssetType(), GetFName());
}
