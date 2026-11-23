// Pure combat logic (no GAS, no Network Prediction).

#include "Rules/GSTN_AirCrouch.h"
#include "Input/GSTN_CombatButtonTags.h"

namespace GSTNCombatTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_AirCrouch, "Combat.State.AirCrouch", "Crouched in the air: airborne with the kit's air crouch button held (FGSTN_AirCrouchKitExtension). Projected by the combat tick, never granted");
}

FGameplayTag FGSTN_AirCrouchKitExtension::ResolveHoldButton() const
{
	return HoldButton.IsValid() ? HoldButton : GSTNCombatButtonTags::Button_Crouch.GetTag();
}

bool FGSTN_AirCrouchKitExtension::Holds(bool bAirborne, bool bHeld, bool bInMove, bool bActionLocked, const FGameplayTagContainer& OwnerTags) const
{
	if (!bEnabled || !bAirborne || !bHeld || bActionLocked)
	{
		return false;
	}
	if (bInMove && !bKeepDuringMoves)
	{
		return false;
	}
	return BlockedTags.IsEmpty() || !OwnerTags.HasAny(BlockedTags);
}
