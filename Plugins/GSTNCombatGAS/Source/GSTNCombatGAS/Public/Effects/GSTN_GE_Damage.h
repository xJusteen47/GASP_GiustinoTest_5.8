// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GSTN_GE_Damage.generated.h"

/**
 * The instant effect a hit applies to its victim (combat-m4-hits 4). Its only content is UGSTN_DamageExecution; the damage is a
 * set-by-caller magnitude (Combat.SetByCaller.Damage) and the damage mode comes from the hit effect context, so one class serves
 * every match and the rules asset only names it (a soft class, resolved outside the simulation). Make a Blueprint child to add
 * a gameplay cue or to grant tags.
 */
UCLASS(Blueprintable)
class GSTNCOMBATGAS_API UGSTN_GE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_Damage();
};
