// Spin and drill, ability-system half.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Data/GSTN_SpinDrillTracks.h"
#include "GSTN_GA_Spin.generated.h"

class UGameplayEffect;

/**
 * The spin attack (spin-drill.md 3, 7.3; owner: 3D World on the ground, Sunshine in the air). A timeline ability: the frame data, the hitbox
 * ring, the cancel windows and the spin track are its timeline, and the ability itself holds almost nothing. The drill tick extension sees the
 * spin track on the timeline that started and writes the Spinning phase into the drill fragment (sense, air spin used, low gravity window), so
 * the spin's state is sync state the combat tick owns, never the ability's.
 *
 * In the air the timeline's spin track says bAirSpin: the ability then applies HopEffect when the timeline starts (the small hop; the ability
 * never sets the character's state, Mover applies the queued move), and the tick extension lowers gravity for the track's frames through the
 * low gravity effect. Once per airtime: the air spin classes are blocked by Combat.State.AirSpinSpent, which the extension projects from the
 * fragment; landing and trampolines give it back, a hit does not (owner 2026-11-01).
 *
 * Kit bindings (data): Command.Movement.Spin.Clockwise to UGSTN_GA_AirSpinClockwise with an airborne requirement and UGSTN_GA_SpinClockwise
 * without one, the same for CounterClockwise. One class per sense because the fragment is written inside the combat tick, before the ability
 * system knows which command fired; the sense is therefore data of the timeline (FGSTN_SpinTrack::Sense).
 */
UCLASS(Abstract, Blueprintable)
class GSTNSPINDRILLGAS_API UGSTN_GA_Spin : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_Spin(const FObjectInitializer& ObjectInitializer);

	/** Applied to the owner when an air spin's timeline starts. It must move the character (UApplyMoveGameplayEffectComponent) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spin")
	TSubclassOf<UGameplayEffect> HopEffect;

	/** The spin track of the timeline this class runs, null when the timeline has none (a misauthored asset: the spin is then a plain attack) */
	const FGSTN_SpinTrack* FindSpinTrack() const;

	/** The tags that block the activation (the ability's data: the air spin's Combat.State.AirSpinSpent) */
	const FGameplayTagContainer& GetBlockedTags() const { return ActivationBlockedTags; }

protected:
	virtual void OnTimelineBegan(int32 StateStartFrame) override;

	/** Sets the default timeline of a ground or air spin of one sense, and for the air spin the block on Combat.State.AirSpinSpent */
	void SetDefaultSpin(EGSTN_SpinSense Sense, bool bAirSpin);
};

/** The default ground spin, clockwise. Bind to Command.Movement.Spin.Clockwise */
UCLASS(Blueprintable)
class GSTNSPINDRILLGAS_API UGSTN_GA_SpinClockwise : public UGSTN_GA_Spin
{
	GENERATED_BODY()

public:
	UGSTN_GA_SpinClockwise(const FObjectInitializer& ObjectInitializer);
};

/** The default ground spin, counter-clockwise. Bind to Command.Movement.Spin.CounterClockwise */
UCLASS(Blueprintable)
class GSTNSPINDRILLGAS_API UGSTN_GA_SpinCounterClockwise : public UGSTN_GA_Spin
{
	GENERATED_BODY()

public:
	UGSTN_GA_SpinCounterClockwise(const FObjectInitializer& ObjectInitializer);
};

/** The default air spin, clockwise: hop, low gravity, once per airtime. Bind to Command.Movement.Spin.Clockwise with an airborne requirement */
UCLASS(Blueprintable)
class GSTNSPINDRILLGAS_API UGSTN_GA_AirSpinClockwise : public UGSTN_GA_Spin
{
	GENERATED_BODY()

public:
	UGSTN_GA_AirSpinClockwise(const FObjectInitializer& ObjectInitializer);
};

/** The default air spin, counter-clockwise. Bind to Command.Movement.Spin.CounterClockwise with an airborne requirement */
UCLASS(Blueprintable)
class GSTNSPINDRILLGAS_API UGSTN_GA_AirSpinCounterClockwise : public UGSTN_GA_Spin
{
	GENERATED_BODY()

public:
	UGSTN_GA_AirSpinCounterClockwise(const FObjectInitializer& ObjectInitializer);
};
