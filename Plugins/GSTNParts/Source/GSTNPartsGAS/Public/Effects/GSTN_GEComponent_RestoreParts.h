// Destructible parts, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Data/GSTN_PartRestoreRequest.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponent.h"
#include "GameplayTagContainer.h"
#include "GSTN_GEComponent_RestoreParts.generated.h"

class AActor;
class UAbilitySystemComponent;

/**
 * Marks a gameplay effect as a restore request for the broken parts of its target (combat-parts.md §4.5): a healer enemy's ability applies
 * such an effect to a wounded one, or a script applies it through UGSTN_PartsLibrary::RequestRestore. The effect is INFINITE on purpose: the
 * request is the effect itself. The ability system is simulation state that is rolled back with the rest, so an effect still on the target is
 * a request not yet consumed, on every machine and after every rollback. The parts tick extension of the target consumes it at the victim's
 * next combat tick (restores the fragment, publishes Combat.Event.Part.Restored) and removes the effect, and a re-simulated frame finds the
 * effect again and does the same.
 *
 * Why the effect and not a field of the parts fragment: the combat model ticks before the ability system and nothing else writes into its
 * state during a frame, so a component that runs in the ability system's tick cannot write the fragment. The cost is no wire bytes at all.
 *
 * The fields are the defaults of the effect asset. A request made in code (RequestRestore) carries its own mask in the spec's level (mask + 1,
 * so the default level 1 means "the asset's mask") and its own fraction as a set-by-caller value.
 */
UCLASS(DisplayName = "Restore Parts", meta = (BlueprintThreadSafe))
class GSTNPARTSGAS_API UGSTN_GEComponent_RestoreParts : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	/** Parts to restore, bit i = part i; 0 = every part. The broken ancestors of the masked parts are restored with them */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parts|Restore", meta = (ClampMin = 0, ClampMax = 65535))
	int32 PartMask = 0;

	/** Damage a restored part keeps as a fraction of its cap: 0 = full repair. A restore never makes a part worse than it was */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parts|Restore", meta = (ClampMin = 0, ClampMax = 1))
	float DamageFraction = 0.f;

	/** Set-by-caller name of the per-request fraction */
	static FName GetFractionKey();

	/** The spec level that carries a mask: mask + 1, so level 1 (the default of any effect) keeps the asset's mask */
	static float LevelForMask(uint16 Mask) { return static_cast<float>(Mask) + 1.f; }

	/** The request this component makes of a spec of its effect: the mask from the level (above 1) or the asset, the fraction from the set-by-caller value or the asset */
	FGSTN_PartRestoreRequest MakeRequest(const FGameplayEffectSpec& Spec) const;
};

/**
 * The native default of a restore effect: infinite, carrying the restore component with its defaults (every part, full repair). A content
 * effect can be a Blueprint child of it, or any effect class with the component added.
 */
UCLASS(Blueprintable)
class GSTNPARTSGAS_API UGSTN_GE_PartRestore : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_PartRestore(const FObjectInitializer& ObjectInitializer);
};

/** A restore request found on an ability system, with the effect that carries it */
struct GSTNPARTSGAS_API FGSTN_PendingPartRestore
{
	FActiveGameplayEffectHandle Handle;
	FGSTN_PartRestoreRequest Request;

	/** The actor that applied the effect (the healer), null when unknown; the instigator of the event */
	TWeakObjectPtr<AActor> Instigator;
};

/**
 * The restore requests of an ability system: finding them, making one, consuming them. Static and stateless; the NP half's tick extension
 * calls Collect and Consume, the authority code calls Apply. Nothing here reads a component other than the ability system it is handed, and
 * nothing branches on the network role.
 */
struct GSTNPARTSGAS_API FGSTN_PartRestoreQueue
{
	/** Every active effect that carries the restore component, in the container's order (the same on every machine), with its request. Out is reset first */
	static void Collect(const UAbilitySystemComponent& ASC, TArray<FGSTN_PendingPartRestore>& Out);

	/** Removes the effects of the requests that were handled. Returns how many were removed */
	static int32 Consume(UAbilitySystemComponent& ASC, TConstArrayView<FGSTN_PendingPartRestore> Handled);

	/**
	 * Applies a restore effect to ASC. EffectClass null = UGSTN_GE_PartRestore. Returns whether the effect was applied. For authority code
	 * outside the simulation (a script, a console command); an ability applies its restore effect through the normal GAS path instead
	 */
	static bool Apply(UAbilitySystemComponent& ASC, TSubclassOf<UGameplayEffect> EffectClass, const FGSTN_PartRestoreRequest& Request, AActor* Instigator);
};
