// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_GrabHoldTypes.h"

FGSTN_GrabEscapeHit::FGSTN_GrabEscapeHit()
{
	// TEST VALUES (owner tunes per enemy): a light shove away from the enemy. Not parryable: the grabber is holding it, there is no guard to parry with
	Hit.Damage = 5.f;
	Hit.BaseKnockback = 600.f;
	Hit.AngleDeg = 40.f;
	Hit.bParryable = false;
}
