#include "GSTN_DepthMarkerSettings.h"

UGSTN_DepthMarkerSettings::UGSTN_DepthMarkerSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("GSTN Depth Marker");
	// Pawns never are a target (a character below a jumping one is not the ground); a project adds its projectile channel
	ExcludedObjectTypes.Add(ECC_Pawn);
}
