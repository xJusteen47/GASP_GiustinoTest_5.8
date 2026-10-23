// Native tags of the camera (camera.md §9). Root Camera.* is owned by GSTNCamera. None is ever granted by the simulation or read by it.
// Designer leaves (Camera.Mode.<Name>, Camera.Request.Scope.<Name>, Camera.Section.<Name>) are ini content of the project that adds them.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace GSTNCameraTags
{
	GSTNCAMERACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Camera);

	GSTNCAMERACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mode_Follow);
	GSTNCAMERACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mode_Free);
	GSTNCAMERACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mode_Shoulder);
	GSTNCAMERACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mode_Section);
	GSTNCAMERACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mode_Duel);

	GSTNCAMERACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Request_Aim);
	GSTNCAMERACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Request_Scope);

	GSTNCAMERACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Section);

	GSTNCAMERACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_InSection);
	GSTNCAMERACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ShoulderBlocked);
}
