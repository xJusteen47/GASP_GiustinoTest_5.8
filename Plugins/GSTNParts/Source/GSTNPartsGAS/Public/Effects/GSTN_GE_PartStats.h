// Destructible parts, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Attributes/GSTN_KitStats.h"
#include "GameplayEffect.h"
#include "GSTN_GE_PartStats.generated.h"

class UAbilitySystemComponent;

/**
 * Base class of the stats effect of a broken part (FGSTN_PartBreakEffect_Stats): infinite, so a lost leg slows the entity until the
 * effect is removed, which only an explicit restore of the part does (parts never heal by themselves). A Blueprint child adds the
 * modifiers on the combat attribute set; their magnitudes are set-by-caller values under Combat.SetByCaller.Stat, the keys of the
 * Stats effect, exactly like the weapon stats effect.
 *
 * One class serves every part of an entity, so the effect is found again by its class AND the part it belongs to. The part index
 * rides in the spec's level (index + 1): the level is plain data of the spec, needs no tag and no handle kept anywhere, so the
 * effect can be found, applied once and removed from the part mask alone, which is what a resimulated frame and a restore have.
 * The level is never read by the modifiers (every magnitude is set-by-caller), so a Blueprint child must not use it as a scalable
 * float input. Every modifier of the child reads a set-by-caller key, and the engine logs an error for a key the spec never set,
 * so the stat keys of the Stats effects of a part set must cover the keys the child's modifiers read (a stat nothing reads is harmless).
 */
UCLASS(Blueprintable)
class GSTNPARTSGAS_API UGSTN_GE_PartStats : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_PartStats();

	/** The spec level that stands for a part index */
	static float LevelForPart(int32 PartIndex) { return static_cast<float>(PartIndex + 1); }

	/**
	 * Applies EffectClass to ASC, sourced by ASC itself, for the part PartIndex, with Values as set-by-caller magnitudes. Returns an
	 * invalid handle when the class is null, the index is negative or the effect was not applied. When the effect of that part is
	 * already active it does nothing and returns the live handle, so a repeated break request (a resimulated frame) never stacks it.
	 */
	static FActiveGameplayEffectHandle Apply(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_PartStats> EffectClass, int32 PartIndex,
		TConstArrayView<FGSTN_StatValue> Values);

	/** Removes the active instance of EffectClass that belongs to PartIndex. Returns how many were removed */
	static int32 Remove(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_PartStats> EffectClass, int32 PartIndex);

	/** True while the effect of EffectClass for PartIndex is active on ASC */
	static bool IsApplied(const UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_PartStats> EffectClass, int32 PartIndex);
};
