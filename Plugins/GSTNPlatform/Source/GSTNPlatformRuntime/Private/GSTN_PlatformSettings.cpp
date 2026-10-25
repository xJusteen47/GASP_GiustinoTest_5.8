// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#include "GSTN_PlatformSettings.h"

#include "Engine/CollisionProfile.h"
#include "GSTNPlatformCoreModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PlatformSettings)

namespace GSTNPlatformSettingsPrivate
{
	bool FindChannel(FName Name, ECollisionChannel& OutChannel)
	{
		const UCollisionProfile* Profile = UCollisionProfile::Get();
		if (!Profile || Name.IsNone())
		{
			return false;
		}
		for (int32 Index = 0; Index < ECC_MAX; ++Index)
		{
			if (Profile->ReturnChannelNameFromContainerIndex(Index) == Name)
			{
				OutChannel = static_cast<ECollisionChannel>(Index);
				return true;
			}
		}
		return false;
	}
}

ECollisionChannel UGSTN_PlatformSettings::GetPlatformObjectChannel() const
{
	ECollisionChannel Channel = ECC_WorldDynamic;
	if (!GSTNPlatformSettingsPrivate::FindChannel(PlatformObjectChannelName, Channel))
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogGSTNPlatform, Warning, TEXT("Collision channel '%s' is not defined: platform colliders use WorldDynamic. Declare it in the collision settings (the plugin's Config/DefaultEngine.ini does)"),
				*PlatformObjectChannelName.ToString());
		}
		return ECC_WorldDynamic;
	}
	return Channel;
}

bool UGSTN_PlatformSettings::HasPlatformObjectChannel() const
{
	ECollisionChannel Channel;
	return GSTNPlatformSettingsPrivate::FindChannel(PlatformObjectChannelName, Channel);
}
