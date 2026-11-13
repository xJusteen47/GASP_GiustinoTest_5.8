// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"

/** Which weapon a weapon switch track selects, as a function of the active weapon and the number of weapons (combat-m3 7.7) */
struct GSTNCOMBATCORE_API FGSTN_WeaponSwitchRules
{
	/**
	 * The index of the weapon after the switch. Next and Previous wrap around; Index selects that weapon and, when the loadout has no
	 * such weapon, changes nothing. A combatant with fewer than two weapons never switches. Active outside [0, Count) counts as 0.
	 */
	static int32 Resolve(EGSTN_SwitchTarget Target, int32 Index, int32 Active, int32 Count);
};
