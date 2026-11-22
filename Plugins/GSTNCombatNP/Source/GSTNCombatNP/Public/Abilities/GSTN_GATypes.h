// Combat abilities (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_GATypes.generated.h"

/**
 * What an ability that was not started by a command does when the combatant is busy running a timeline (combat-m3 6.1). A command
 * never reads it: the combat tick already decided, with the cancel and override windows, that the command may start.
 */
UENUM(BlueprintType)
enum class EGSTN_InterruptPolicy : uint8
{
	/** Do not activate while a timeline runs (the default: a trigger or an effect waits its turn) */
	SkipIfBusy,

	/** Activate and cancel the abilities named by UGSTN_GA_Base::InterruptTags */
	InterruptCurrent
};

/**
 * The decisions of UGSTN_GA_Base that depend on nothing but their arguments, so the rules are tested without an ability system
 * and the ability only gathers the arguments from the simulation.
 */
struct FGSTN_GARules
{
	/** CanActivateAbility says no: SkipIfBusy, a timeline running, and the activation is not a command */
	static bool BlocksActivation(EGSTN_InterruptPolicy Policy, bool bInTimeline, bool bCommandActivation)
	{
		return Policy == EGSTN_InterruptPolicy::SkipIfBusy && bInTimeline && !bCommandActivation;
	}

	/** ActivateAbility cancels the running timeline's abilities first: InterruptCurrent, a timeline running, not a command */
	static bool ShouldInterrupt(EGSTN_InterruptPolicy Policy, bool bInTimeline, bool bCommandActivation)
	{
		return Policy == EGSTN_InterruptPolicy::InterruptCurrent && bInTimeline && !bCommandActivation;
	}

	/** The ability ends: it ends on release, it has a trigger button (a forced command or a trigger has none), and that button is up */
	static bool EndsOnRelease(bool bEndOnTriggerRelease, int32 TriggerButton, bool bTriggerHeld)
	{
		return bEndOnTriggerRelease && TriggerButton != INDEX_NONE && !bTriggerHeld;
	}
};
