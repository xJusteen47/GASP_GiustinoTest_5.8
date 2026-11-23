// Fixtures used only by the Automation tests of the defence GAS half. Private: nothing outside the tests includes this.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Misc/AutomationTest.h"
#include "GSTN_DefenceGASTestTypes.generated.h"

class UAbilitySystemComponent;
class UWorld;

/** Distinct ability classes to tell the kit extension's entries apart */
UCLASS(Hidden)
class UGSTN_TestDefenceShieldAbility : public UGameplayAbility
{
	GENERATED_BODY()
};

UCLASS(Hidden)
class UGSTN_TestDefenceTechA : public UGameplayAbility
{
	GENERATED_BODY()
};

UCLASS(Hidden)
class UGSTN_TestDefenceTechB : public UGameplayAbility
{
	GENERATED_BODY()
};

UCLASS(Hidden)
class UGSTN_TestDefenceGetupNeutral : public UGameplayAbility
{
	GENERATED_BODY()
};

/** Not an ability: what a kit extension must not be allowed to name */
UCLASS(Hidden)
class UGSTN_TestDefenceNotAnAbility : public UObject
{
	GENERATED_BODY()
};

namespace GSTNDefenceGASTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/**
	 * A transient game world with one actor owning a stock ability system component and NO attribute set: the tests add the defence
	 * set through the library, as the loadout grant does. Effects need a world for their start time; no Network Prediction or Mover
	 * is involved, so the tests run on stock GAS.
	 */
	struct FRig
	{
		FRig();
		~FRig();
		UWorld* World = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
	};
}
