// Effects and a minimal world used only by the Automation tests of the attribute layer.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "Effects/GSTN_GE_MeterCost.h"
#include "Effects/GSTN_GE_WeaponStats.h"
#include "GSTN_GASTestTypes.generated.h"

class UAbilitySystemComponent;
class UWorld;

namespace GSTNCombatGASTest
{
	/** Test leaves under the stat set-by-caller root; production code never names a leaf. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_TestSpeed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_TestMeterMax);
}

/** Instant: overrides the EX meter maximum, then the walk speed, from set-by-caller stats. */
UCLASS(Hidden)
class UGSTN_TestKitInitEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_TestKitInitEffect();
};

/** Meter cost on the EX meter. */
UCLASS(Hidden)
class UGSTN_TestExMeterCost : public UGSTN_GE_MeterCost
{
	GENERATED_BODY()

public:
	UGSTN_TestExMeterCost();
};

/** Infinite: adds the test speed stat to the walk speed. */
UCLASS(Hidden)
class UGSTN_TestWeaponStatsA : public UGSTN_GE_WeaponStats
{
	GENERATED_BODY()

public:
	UGSTN_TestWeaponStatsA();
};

/** Same modifier as A, a different class: the two must not remove each other. */
UCLASS(Hidden)
class UGSTN_TestWeaponStatsB : public UGSTN_GE_WeaponStats
{
	GENERATED_BODY()

public:
	UGSTN_TestWeaponStatsB();
};

namespace GSTNCombatGASTest
{
	/**
	 * A transient game world with one actor owning a stock ability system component, both attribute sets added. Effects
	 * need a world for their start time; no Network Prediction or Mover is involved, so the test runs on stock GAS.
	 */
	struct FRig
	{
		FRig();
		~FRig();

		UWorld* World = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
	};
}
