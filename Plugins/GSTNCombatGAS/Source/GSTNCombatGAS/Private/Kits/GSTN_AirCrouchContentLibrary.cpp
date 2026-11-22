// Ability-system layer of the combat framework.

#include "Kits/GSTN_AirCrouchContentLibrary.h"

#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Rules/GSTN_AirCrouch.h"

bool UGSTN_AirCrouchContentLibrary::EnsureKitAirCrouch(UGSTN_PrimaryKitDefinition* Kit)
{
	if (!Kit || Kit->Extensions.Find<FGSTN_AirCrouchKitExtension>())
	{
		return false;
	}
	Kit->Extensions.ExtensionData.AddDefaulted_GetRef().InitializeAs<FGSTN_AirCrouchKitExtension>();
	Kit->MarkPackageDirty();
	return true;
}

bool UGSTN_AirCrouchContentLibrary::HasKitAirCrouch(const UGSTN_PrimaryKitDefinition* Kit)
{
	const FGSTN_AirCrouchKitExtension* AirCrouch = Kit ? Kit->Extensions.Find<FGSTN_AirCrouchKitExtension>() : nullptr;
	return AirCrouch && AirCrouch->bEnabled;
}
