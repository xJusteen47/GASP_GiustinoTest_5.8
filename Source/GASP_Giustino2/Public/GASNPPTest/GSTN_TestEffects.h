// GAS + NPP + Mover network test harness.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GSTN_TestEffects.generated.h"

namespace GSTNTestTags
{
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Dash);
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Buff);
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dashing);
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Buffed);
}

/** Creates a forward linear-velocity layered move for the duration of the owning effect. */
UCLASS(DisplayName = "GSTN Dash Move Creator")
class GASP_GIUSTINO2_API UGSTN_DashMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	UGSTN_DashMoveCreator(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashSpeed = 1400.f;

	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: -25 Stamina. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_DashCost : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_DashCost();
};

/** 1.5s: grants Test.Cooldown.Dash. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_DashCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_DashCooldown();
};

/** 0.3s: grants Test.State.Dashing and drives Mover through a layered move. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_Dash : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_Dash();
};

/** 4s: grants Test.Cooldown.Buff. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_BuffCooldown : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_BuffCooldown();
};

/** 3s, period 0.5s: +5 Health per tick, grants Test.State.Buffed. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_Buff : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_Buff();
};

/** Infinite, period 0.25s: +2 Stamina per tick. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_StaminaRegen : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_StaminaRegen();
};
