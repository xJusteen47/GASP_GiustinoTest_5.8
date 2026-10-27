// Defence, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_ShieldAbilityData.generated.h"

struct FGSTN_ShieldRules;

/**
 * Why the shield command cannot execute (UGSTN_DefenceLibrary::CanExecuteShield). For logs, tests and UI; never on the wire. The
 * model refuses through the ability's CanExecute with these rules; the lock tag Combat.State.Tech.Lockout is NOT reused (spec 5.3).
 */
UENUM(BlueprintType)
enum class EGSTN_ShieldRefusal : uint8
{
	None,
	/** The mode disables the shield or the kit has none: the command is not even built (safeguard only) */
	Unavailable,
	/** The shield value is at or below zero (decayed or broken and not yet recovered) */
	Empty,
	/** The shield is breaking (ShieldFlags.Breaking) */
	Breaking,
	/** The entity is not grounded: no wall or air shield, as in Smash */
	NotGrounded
};

/**
 * One out-of-shield option (spec 5.3): a cancel window inside the hold window into the commands the query matches. They are
 * M3 cancel windows as data, so no code is needed per option; the shield ability lists them and BuildTimeline places each over
 * the hold window. The model refuses them during ShieldStun by itself (the stun closes the candidates, not the window).
 */
USTRUCT(BlueprintType)
struct GSTNDEFENCEGAS_API FGSTN_OutOfShieldOption
{
	GENERATED_BODY()

	/** The commands the shield may end into: jump, the kit's up special and up strong, Command.Link.Grab, the kit's dodge, the platform drop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield")
	FGameplayTagQuery IntoCommands;
};

/**
 * The timeline of the shield ability, as data (spec 4, 5.3): startup frames (the bubble exists from the press, blocks from the
 * end of startup), ONE hold window on the shield button that stalls the timeline while the button is held and ends by itself when
 * the value decays to zero (bEndOnZeroShield: a forced drop, never a break), then the release frames the bubble outlasts the
 * release by. The frame counts come from the rules asset, so a kit with its own rules gets its own timeline from the same ability
 * class. The shield ability (GSTNDefenceNP, where the combat base ability lives) builds its timeline from this at grant, outside
 * the simulation; nothing here runs in a tick.
 */
USTRUCT(BlueprintType)
struct GSTNDEFENCEGAS_API FGSTN_ShieldAbilitySpec
{
	GENERATED_BODY()

	FGSTN_ShieldAbilitySpec();

	/** The button the hold window reads; Input.Button.Shield by default, the slot the shield command is anchored on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield", meta = (Categories = "Input.Button"))
	FGameplayTag HoldButton;

	/**
	 * Length of the hold window in timeline frames. The window stalls the timeline while the button is held, so its length only
	 * decides how many frames of the montage play per stall; 1 is enough and the default
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 HoldFrames = 1;

	/** The out-of-shield options, one cancel window each over the hold window */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield")
	TArray<FGSTN_OutOfShieldOption> OutOfShieldOptions;

	/** The shield montage, soft; empty = none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield")
	TSoftObjectPtr<UAnimMontage> Montage;

	/**
	 * The timeline for Rules: [0, StartupFrames) startup, [StartupFrames, StartupFrames + HoldFrames) the hold window with the
	 * out-of-shield cancel windows over it, then ReleaseFrames; FirstActionableFrame is the end. Tracks: the hold window first,
	 * then one cancel window per option, in list order (the same on every machine, since the table hashes the timeline).
	 */
	FGSTN_CombatTimelineData BuildTimeline(const FGSTN_ShieldRules& Rules) const;

	/** First frame of the hold window for Rules (the end of startup) */
	static int32 HoldStartFrame(const FGSTN_ShieldRules& Rules);
};
