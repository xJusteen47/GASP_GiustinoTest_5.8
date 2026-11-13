// Destructible parts, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_PartsLibrary.generated.h"

class AActor;

/** Entry points of the parts plugin for scripts and Blueprints that act OUTSIDE the simulation (never call these from a simulated ability) */
UCLASS()
class GSTNPARTSGAS_API UGSTN_PartsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Asks for the broken parts of Target to be restored (a script, a console command, a healer that is not a simulated ability). Authority only:
	 * it applies a restore effect (EffectClass, null = UGSTN_GE_PartRestore) to the target's ability system, and the parts tick extension of the
	 * target consumes it at its next combat tick, so every machine restores on the same frame (prediction and reconcile follow the effect). A simulated
	 * ability applies a restore effect through the normal GAS path instead. PartMask 0 = the asset's mask (every part for the default class);
	 * DamageFraction 0 = full repair. Returns whether the request was made
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Parts", meta = (DefaultToSelf = "Instigator"))
	static bool RequestRestore(AActor* Target, int32 PartMask, float DamageFraction, TSubclassOf<UGameplayEffect> EffectClass, AActor* Instigator);
};
