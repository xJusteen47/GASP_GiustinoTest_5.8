// Spin and drill, ability-system half.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_GA_DrillBore.generated.h"

class UGameplayEffect;
struct FGSTN_DrillFragment;

/**
 * The drill's bore (plan D9 part 2, spin-drill.md 3, 7.4). Never bound to a command: the drill tick extension starts its timeline (forced) on the
 * seat's first frame, so the kit must list it among its triggered abilities, which the spin and drill kit extension does by itself
 * (FGSTN_SpinDrillKitExtension::BoreAbility). Its timeline carries a FGSTN_BoreTrack: the drill's track hook holds it on the seat frame while
 * Seating and on the bore frame while latched, where the tip re-hits the target every RehitIntervalFrames (seam S1) as ordinary hits.
 *
 * The phase machine decides how the latch ends (UGSTN_DrillMath::ResolveEnd); the ability only executes, on the frame of the decision:
 * LaunchEffect when the fragment enters Launching, PopEffect on the Popped flag. Nothing on a hitstun detach (the hitstun replaces the
 * timeline). After either the timeline plays its recovery and the ability ends when the combat state no longer names it. Never sets the
 * character's state: Mover applies the queued moves. Every read is the fragment of the frame, so a re-simulation decides the same.
 */
UCLASS(Blueprintable)
class GSTNSPINDRILLGAS_API UGSTN_GA_DrillBore : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_DrillBore(const FObjectInitializer& ObjectInitializer);

	/** Applied when the bore ends in a launch. It must move the character (UApplyMoveGameplayEffectComponent) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drill")
	TSubclassOf<UGameplayEffect> LaunchEffect;

	/** Applied when the bore ends in a detach pop (crouch released early) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drill")
	TSubclassOf<UGameplayEffect> PopEffect;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;

	/** What the bore does with the fragment of a frame: 0 nothing, 1 launch, 2 pop. Pure, for tests */
	static int32 DecideFromFragment(const FGSTN_DrillFragment* Fragment, int32 Frame);
};
