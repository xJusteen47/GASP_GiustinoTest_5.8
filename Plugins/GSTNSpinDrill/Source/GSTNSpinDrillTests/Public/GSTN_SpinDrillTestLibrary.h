// Spin and drill, test module: harness helpers (never in Shipping).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_SpinDrillTestLibrary.generated.h"

/**
 * Infinite effect granting Combat.Immunity.Drill: how the harness marks a hittable target that is not drillable (the tag stands in for
 * the penetration against hardness of plan D12). Applied on the authority like the other harness effects, so the tag reaches every copy
 * through the ability system state, never as a loose tag.
 */
UCLASS(NotBlueprintable)
class GSTNSPINDRILLTESTS_API UGSTN_GE_TestDrillImmunity : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_TestDrillImmunity();
};

/** Harness calls of the spin and drill scenarios (Tools/GASNPPTest/spin_drill_scenarios.py) */
UCLASS()
class GSTNSPINDRILLTESTS_API UGSTN_SpinDrillTestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Gives (true) or takes away (false) Combat.Immunity.Drill on Target through UGSTN_GE_TestDrillImmunity. Authority only: returns false
	 * on a copy without authority or without an ability system. Applying twice keeps one effect
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|SpinDrill|Test")
	static bool ServerSetDrillImmunity(AActor* Target, bool bImmune);
};
