// GAS + NPP + Mover network test harness.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_Base.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_TestCombatAbilities.generated.h"

/**
 * Placeholder ability bound to the four Command.Attack.* commands (grounded and airborne) by the combat test binding set.
 *
 * It is a combat timeline ability (UGSTN_GA_BaseCombat) whose only effect is to own Test.State.Attacking while the timeline
 * runs, so a command that started it (or one that was locked and did not) shows up in the ability system's tags, the harness
 * snapshots and the Test.* log. It never touches the character: the harness checks input, lock and timeline behaviour here,
 * not combat moves. The frame data is set in the constructor (no asset): 20 frames, an 8 frame startup override window (a
 * higher tier command replaces it), a cancel window [8, 14) into the special commands, free from frame 14.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestAttack : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestAttack(const FObjectInitializer& ObjectInitializer);
};

/**
 * A timeline ability no command starts: the owned tag Test.Probe.TagTrigger (granted by the tag raiser probe, which a press of the
 * buff button activates from inside the simulation) triggers it, and the combat tick adopts it (combat-m3 7.4, scenario_triggered_timeline).
 * It owns Test.State.Triggered while the timeline runs. 12 frames, no startup override window, free from frame 12. The test
 * component adds it to the combat component's AdoptableTimelines.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestTriggeredTimeline : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestTriggeredTimeline(const FObjectInitializer& ObjectInitializer);
};

/**
 * Placeholder stance, bound to the Command.Special.* commands (the Special button is the stance button of
 * scenario_combat_lock). While active it owns Test.State.Stance and the attack command lock (Input.Lock.Command.Attack, via
 * ActivationOwnedTags), so the attack commands cannot start while the stance is held but a command of another root (the
 * dash) still does.
 *
 * It is a UGSTN_GA_Base with bEndOnTriggerRelease: the base captures the trigger button and ends the stance when the button is
 * released (a start without a button never ends on release). It logs the camera aim it saw on activation (GASNPP_AIM, read by
 * scenario_camera_aim).
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestStance : public UGSTN_GA_Base
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestStance(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/**
 * A weapon switch ability (scenario_weapon_switch): triggered like UGSTN_GA_TestTriggeredTimeline by Test.Probe.TagTrigger and adopted by
 * the combat tick, with a weapon switch track (next weapon) at frame 3 of a 12 frame timeline. Grant it instead of the triggered
 * timeline (both would wait for the same adoption). The test component adds it to the combat component's AdoptableTimelines.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestWeaponSwitch : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestWeaponSwitch(const FObjectInitializer& ObjectInitializer);
};
