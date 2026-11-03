// Pure combat logic (no GAS, no Network Prediction).

#include "Timeline/GSTN_WeaponSwitchRules.h"

int32 FGSTN_WeaponSwitchRules::Resolve(EGSTN_SwitchTarget Target, int32 Index, int32 Active, int32 Count)
{
	if (Count < 2)
	{
		return FMath::Max(Active, 0);
	}
	if (Active < 0 || Active >= Count)
	{
		Active = 0;
	}

	switch (Target)
	{
	case EGSTN_SwitchTarget::Next:
		return (Active + 1) % Count;
	case EGSTN_SwitchTarget::Previous:
		return (Active + Count - 1) % Count;
	case EGSTN_SwitchTarget::Index:
	default:
		return Index >= 0 && Index < Count ? Index : Active;
	}
}
