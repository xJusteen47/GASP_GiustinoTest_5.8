// Spin and drill, ability-system half.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_GA_DrillDive.generated.h"

class UGameplayEffect;
struct FGSTN_DrillFragment;

/**
 * The drill's dive (plan D8, spin-drill.md 3, 7.4): crouch pressed during a spin (the spin's cancel window into Command.Movement.Drill, a binding
 * that requires Combat.State.Spinning). A timeline ability whose timeline carries a FGSTN_DiveTrack and the drill tip; the drill tick extension
 * turns the track into the Diving phase, applies the dive gravity (none) while it lasts, and decides what the frame means: a hit of the tip is a
 * bounce (D9 latches the drillable ones first), the ground is the landing lag.
 *
 * The ability only executes: PlungeEffect when its timeline starts (PlungeSpeed along gravity), BounceEffect on the bounce frame (the fragment's
 * Bounced flag) and then it ends, and it ends GroundLagFrames after the fragment entered the lag. Releasing crouch ends it (bEndOnTriggerRelease):
 * the character falls with its gravity back. Never sets the character's state: Mover applies the queued moves. Every read is the fragment of the
 * frame, so a re-simulation decides the same.
 */
UCLASS(Blueprintable)
class GSTNSPINDRILLGAS_API UGSTN_GA_DrillDive : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_DrillDive(const FObjectInitializer& ObjectInitializer);

	/** Applied when the timeline starts. It must move the character (UApplyMoveGameplayEffectComponent) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drill")
	TSubclassOf<UGameplayEffect> PlungeEffect;

	/** Applied on the bounce off a hittable target that is not drillable */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Drill")
	TSubclassOf<UGameplayEffect> BounceEffect;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;

	/** What the dive does with the fragment of a frame: 0 nothing, 1 bounce and end, 2 end (the lag is over). Pure, for tests */
	static int32 DecideFromFragment(const FGSTN_DrillFragment* Fragment, int32 Frame, int32 GroundLagFrames);

protected:
	virtual void OnTimelineBegan(int32 StateStartFrame) override;
};
