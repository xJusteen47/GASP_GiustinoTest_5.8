// GAS + NPP + Mover network test harness.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Effects/GSTN_GE_WeaponStats.h"
#include "NativeGameplayTags.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GSTN_TestEffects.generated.h"

namespace GSTNTestTags
{
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Dash);
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Buff);
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dashing);
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Buffed);

	// C15 test kits: the stat keys of the kit init effect (leaves under the plugin's stat set-by-caller root)
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_WalkSpeed);
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_JumpVelocity);

	// M3 spike probes (see GSTN_TestProbeAbilities.h)
	/** GameplayEvent that the event raiser sends and the event target ability is triggered by */
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Probe_Event);
	/** Owned tag that the tag-grant effect adds and the tag target ability is triggered by (OwnedTagAdded) */
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Probe_TagTrigger);
	/** Owned while the infinite bulk effect of the grant probe is applied */
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Probe_Bulk);
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

/** 1s: grants Test.Probe.TagTrigger (the OwnedTagAdded trigger of the tag target probe ability). */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_ProbeTagGrant : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_ProbeTagGrant();
};

/** Instant: +1 ProbeEventCount. Applied by the event target probe ability, once per activation. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_ProbeCountEvent : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_ProbeCountEvent();
};

/** Instant: +1 ProbeTagCount. Applied by the tag target probe ability, once per activation. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_ProbeCountTag : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_ProbeCountTag();
};

/** Infinite: grants Test.Probe.Bulk. Applied from outside the simulation by the grant probe (no modifiers, no period). */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_ProbeBulk : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_ProbeBulk();
};

/** Infinite: speed scale 2.0 and gravity scale 0.5 (additive on a base of 1). Stats probe preset 1. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_ProbeStatsFast : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_ProbeStatsFast();
};

/** Infinite: speed scale 0.5 and gravity scale 2.0 (additive on a base of 1). Stats probe preset 2. */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_ProbeStatsSlow : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_ProbeStatsSlow();
};

/**
 * Weapon stats effects of the weapon switch scenario (scenario_weapon_switch): the primary adds 300 to WalkSpeed (x1.5 of the
 * reference 600), the support removes 300 (x0.5). Mechanism tests only: the real support weapons inherit the kit's stats and ship
 * without a stats effect by default.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_TestPrimaryWeaponStats : public UGSTN_GE_WeaponStats
{
	GENERATED_BODY()
public:
	UGSTN_GE_TestPrimaryWeaponStats();
};

UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_TestSupportWeaponStats : public UGSTN_GE_WeaponStats
{
	GENERATED_BODY()
public:
	UGSTN_GE_TestSupportWeaponStats();
};

/**
 * Instant kit init effect of the C15 test kits (KIT_Test_*): overrides WalkSpeed and JumpVelocity with the numbers of the kit's
 * stats, read as set-by-caller magnitudes (the kit data holds the numbers, this class only names the attributes).
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_TestKitInit : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_TestKitInit();
};
