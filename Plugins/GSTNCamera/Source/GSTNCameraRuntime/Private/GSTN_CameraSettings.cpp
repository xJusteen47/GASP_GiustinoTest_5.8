#include "GSTN_CameraSettings.h"

#include "GSTN_CameraTags.h"

UGSTN_CameraSettings::UGSTN_CameraSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("GSTN Camera");

	ShoulderOcclusion.Policy = EGSTN_CameraOcclusionPolicy::PullIn;

	FollowBlend.BlendSeconds = 0.6f;
	ShoulderBlend.BlendSeconds = 0.25f;
	ShoulderBlend.Function = EGSTN_CameraBlendFunction::EaseOut;
	DuelBlend.BlendSeconds = 0.5f;
}
