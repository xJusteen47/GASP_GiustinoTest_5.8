// GAS + NPP + Mover network test harness.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/NpGameplayAbility.h"
#include "GSTN_TestAbilities.generated.h"

/** Base for the placeholder test abilities: instant activation from an input action, apply one effect, end. */
UCLASS(Abstract)
class GASP_GIUSTINO2_API UGSTN_TestAbilityBase : public UNpGameplayAbility
{
	GENERATED_BODY()

public:
	UGSTN_TestAbilityBase(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	void SetActivationInput(const TCHAR* InputActionPath);

	/** Effect applied to the owner when the ability activates. */
	UPROPERTY(EditDefaultsOnly, Category = "Test")
	TSubclassOf<UGameplayEffect> EffectToApply;
};

/** Costs 25 Stamina, 1.5s cooldown, dashes forward through a Mover layered move. Input: IA_GASNPP_Dash (key 1). */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestDash : public UGSTN_TestAbilityBase
{
	GENERATED_BODY()
public:
	UGSTN_GA_TestDash(const FObjectInitializer& ObjectInitializer);
};

/** 4s cooldown, applies a 3s periodic heal + Test.State.Buffed. Input: IA_GASNPP_Buff (key 2). */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestBuff : public UGSTN_TestAbilityBase
{
	GENERATED_BODY()
public:
	UGSTN_GA_TestBuff(const FObjectInitializer& ObjectInitializer);
};
