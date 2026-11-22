// Pure combat logic (no GAS, no Network Prediction).

#include "Loadout/GSTN_LoadoutInputFeel.h"

const FName FGSTN_LoadoutInputFeelExt::WireId(TEXT("GSTN.InputFeel"));

FGSTN_StickThresholds FGSTN_LoadoutInputFeelExt::Resolve(const TArray<FInstancedStruct>& Extensions)
{
	const FGSTN_LoadoutInputFeelExt* Entry = GSTNLoadoutExtension::Find<FGSTN_LoadoutInputFeelExt>(Extensions);
	return Entry ? Entry->StickThresholds.Sanitized() : FGSTN_StickThresholds();
}
