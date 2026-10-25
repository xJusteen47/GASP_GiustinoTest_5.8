// GAS + NPP + Mover network test harness.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_Base.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Abilities/GSTN_GA_Dash.h"
#include "Abilities/GSTN_GA_PlatformJump.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "Effects/GSTN_GE_MeterCost.h"
#include "GameplayEffect.h"
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
 * The test move of the attacker side (combat-m4-hits 6, M4 H8): bound to Command.Attack.Neutral by WPN_Test_Hits / KIT_Test_Hits. A 32 frame
 * timeline with a hitbox track of each space, every hit with an authored hitstop (8 frames for the victim, 6 for the attacker), and a cancel
 * window into the special commands that opens only on a hit. The geometry is relative to an attacker at the origin facing +X:
 *   frames  4..7   Root           sphere r 50, group 0, sweeps from 80 to 170 cm ahead (keys over the track), swept between frames
 *   frames 10..13  BakedSocket    sphere r 45, group 1, socket hand_r baked to run from (100, 260) to (100, 140) cm
 *   frames 16..19  WorldAtMoveStart sphere r 60, group 2, at (200, -160) cm of where the move started
 *   frames 20..23  Root           sphere r 140 around the attacker, group 3, with an Ally variant (damage 3)
 * Owns Test.State.HitMove while the timeline runs.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestHitMove : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestHitMove(const FObjectInitializer& ObjectInitializer);
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

/** Launch layered move (FLayeredMove_Launch, overriding the velocity) along Mover's up direction at LaunchSpeed: what a trampoline asks the GE path for */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_TestLaunchMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Launch", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float LaunchSpeed = 900.f;

	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant effect that queues the test launch */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_TestLaunch : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_TestLaunch();
};

/**
 * A launch no jump asked for (scenario_external_launch_jump): triggered like the triggered timeline by Test.Probe.TagTrigger (a press of the
 * buff button with the tag raiser probe granted), it applies UGSTN_GE_TestLaunch from inside the simulation, so every machine launches in the same frame.
 * Granted as the probe "Launcher".
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestLaunch : public UGSTN_GA_Base
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestLaunch(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** Placeholder of the interact tap (combat-m3 C14): owns Test.State.Grab for a few combat frames, so the harness sees which interact command fired */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestGrab : public UGSTN_GA_Base
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestGrab(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual void OnCombatFramesElapsed() override;
};

/** Placeholder of the interact hold: owns Test.State.PickUp for a few combat frames */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestPickUp : public UGSTN_GA_TestGrab
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestPickUp(const FObjectInitializer& ObjectInitializer);
};

/**
 * A kit's special escape: the default ground dodge with the hitstun and hitstop blocks dropped, as data. The test binding set binds it to
 * Command.Defence.Dodge for a grounded pawn that owns Test.State.Escape (the default dodge ignores that tag), so granting the tag swaps one for the other
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestEscapeDodge : public UGSTN_GA_GroundDodge
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestEscapeDodge(const FObjectInitializer& ObjectInitializer);
};

/** Infinite: grants Combat.State.Hitstun to the owner. Stands in for the hit reaction of M4, which projects the tag from the combat tick */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_TestHitstun : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_TestHitstun();
};

/** Infinite: grants Combat.Lock.Movement to the owner: it cannot walk, the stick still feeds DI. Test hook of the DI scenario */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_TestMovementLock : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_TestMovementLock();
};

/** Infinite: grants Test.State.Escape, the requirement of the escape dodge binding (the ignore tag of the default dodge) */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_TestEscapeKit : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_TestEscapeKit();
};

// ---- C18 loadout table swap scenarios (command_set, super_code, ex_meter, wait_frames). Test kits bind these; the meters come from
// UGSTN_MeterAttributeSet, which the test component puts on the pawn (the shared Blueprints are not edited).

/** Instant: spends 50 ExMeter. The cost of the EX special; a short meter fails CanActivateAbility, which is what lets the EX slot fall through */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_TestExCost : public UGSTN_GE_MeterCost
{
	GENERATED_BODY()
public:
	UGSTN_GE_TestExCost();
};

/** Instant: spends 100 SuperMeter. The cost of both test supers */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_TestSuperCost : public UGSTN_GE_MeterCost
{
	GENERATED_BODY()
public:
	UGSTN_GE_TestSuperCost();
};

/** Instant: +60 ExMeter. Applied by the harness on the authority (an out-of-band write, like a grant) */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_TestExMeterGrant : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_TestExMeterGrant();
};

/** Instant: +100 SuperMeter, applied like the EX grant */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_TestSuperMeterGrant : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_TestSuperMeterGrant();
};

/** Has a duration of half a second and grants Test.State.Strike: the strike of UGSTN_GA_TestWaitStrike, seen in the tag history */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_TestStrike : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UGSTN_GE_TestStrike();
};

/** Added command of KIT_Test_Gamma (the replacement of the removed dash): owns Test.State.GammaMove for a few combat frames */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestGammaMove : public UGSTN_GA_TestGrab
{
	GENERATED_BODY()
public:
	UGSTN_GA_TestGammaMove(const FObjectInitializer& ObjectInitializer);
};

/** The grab placeholder that pays its cost: commits the ability when it starts (a command activation does not commit by itself), so a meter cost is spent in the simulation on every machine */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestCostGrab : public UGSTN_GA_TestGrab
{
	GENERATED_BODY()
public:
	UGSTN_GA_TestCostGrab(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** The plain special of KIT_Test_Delta: owns Test.State.Special for a few combat frames */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestSpecial : public UGSTN_GA_TestGrab
{
	GENERATED_BODY()
public:
	UGSTN_GA_TestSpecial(const FObjectInitializer& ObjectInitializer);
};

/** The EX special: Test.State.SpecialEx, costs UGSTN_GE_TestExCost. Its slot falls through to the plain special when this cannot start */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestSpecialEx : public UGSTN_GA_TestCostGrab
{
	GENERATED_BODY()
public:
	UGSTN_GA_TestSpecialEx(const FObjectInitializer& ObjectInitializer);
};

/** Super A of KIT_Test_Sigma: Test.State.SuperA, costs UGSTN_GE_TestSuperCost */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestSuperA : public UGSTN_GA_TestCostGrab
{
	GENERATED_BODY()
public:
	UGSTN_GA_TestSuperA(const FObjectInitializer& ObjectInitializer);
};

/** Super B: Test.State.SuperB, same cost */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestSuperB : public UGSTN_GA_TestCostGrab
{
	GENERATED_BODY()
public:
	UGSTN_GA_TestSuperB(const FObjectInitializer& ObjectInitializer);
};

/**
 * A kit-bound timeline ability that waits WaitFrames combat frames and then strikes (scenario_wait_frames): it owns Test.State.Windup while
 * it runs, and at the end of the wait it applies UGSTN_GE_TestStrike (Test.State.Strike). The wait counts combat frames, so a hitstop opened
 * during it delays the strike by exactly the window. The timeline is longer than the wait (60 frames), so the wait ends inside it.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestWaitStrike : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()
public:
	UGSTN_GA_TestWaitStrike(const FObjectInitializer& ObjectInitializer);

	/** Combat frames between the start and the strike */
	static constexpr int32 WaitFrames = 20;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual void OnCombatFramesElapsed() override;
};

/** The platform jump with a 6 frame squat (KIT_Test_Squat): owns Test.State.JumpSquat from the request to the take-off, which is how the harness measures the squat */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestJumpSquat : public UGSTN_GA_PlatformJump
{
	GENERATED_BODY()
public:
	UGSTN_GA_TestJumpSquat(const FObjectInitializer& ObjectInitializer);
};
