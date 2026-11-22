// Defence, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GSTN_GE_ShieldBreak.generated.h"

/**
 * The cosmetic effect of a shield break (m6 spec 4): instant, empty in C++, applied by the NP module's Shield step when the break
 * resolves. It carries no state: the break itself is the combat sync state (ActionState ShieldBroken, BreakEndFrame) and the stun
 * tag Combat.State.ShieldBroken is projected from it, never granted. A Blueprint child adds the cue (GameplayCue.Combat.Shield.Break,
 * an ini leaf the code never names) and any presentation tag; the rules asset names the child by a soft class.
 */
UCLASS(Blueprintable)
class GSTNDEFENCEGAS_API UGSTN_GE_ShieldBreak : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_ShieldBreak();
};
