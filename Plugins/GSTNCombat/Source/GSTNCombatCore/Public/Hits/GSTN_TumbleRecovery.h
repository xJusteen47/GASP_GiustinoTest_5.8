// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Rules/GSTN_KitExtension.h"
#include "GSTN_TumbleRecovery.generated.h"

/** When a tumbling victim may act again in the air (owner 2026-11-16, option B) */
UENUM(BlueprintType)
enum class EGSTN_TumbleRecovery : uint8
{
	/** Locked until it lands (the behaviour before 2026-11-16): a launch upward can be followed by another hit before it ever acts */
	UntilGround,
	/** Able to act once the hitstun ends, still in the air (Smash-like): the victim keeps tumbling until it acts or lands */
	AfterHitstun,
};

/** A combatant's own choice over the rules' (players vs AI and training dummies) */
UENUM(BlueprintType)
enum class EGSTN_TumbleRecoveryOverride : uint8
{
	UseRules,
	UntilGround,
	AfterHitstun,
};

/**
 * Tumble air recovery, the rules' part (UGSTN_CombatRules::TumbleRecovery). A victim launched hard enough to tumble is locked for the
 * hitstun the knockback model gave; with AfterHitstun it may then act in the air (the anti infinite-combo rule: a victim launched upward is
 * not helpless until it lands). It keeps the tumble state, animation and landing rules until it acts: landing without acting is still a
 * tumble landing (tech or knockdown). Any command it starts ends the tumble.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_TumbleRecoveryRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble Recovery")
	EGSTN_TumbleRecovery Mode = EGSTN_TumbleRecovery::AfterHitstun;

	/**
	 * The commands a recovering victim may start (hierarchical: Command.Attack admits every attack). Empty = every command. A kit with an
	 * FGSTN_TumbleRecoveryExtension replaces this list with its own
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble Recovery", meta = (Categories = "Command", EditCondition = "Mode == EGSTN_TumbleRecovery::AfterHitstun"))
	FGameplayTagContainer AllowedCommands;
};

/**
 * The kit's part (kit Extensions): which commands this kit's fighter may use to get out of a tumble in the air, e.g. Command.Movement.Jump,
 * Command.Attack, Command.Defence.Dodge, Command.Special. Data only
 */
USTRUCT(BlueprintType, DisplayName = "Tumble Recovery")
struct GSTNCOMBATCORE_API FGSTN_TumbleRecoveryExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	/** Empty = every command */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble Recovery", meta = (Categories = "Command"))
	FGameplayTagContainer AllowedCommands;
};

/** Rules, kit and combatant resolved once (outside the frame loop): what the simulation reads */
struct GSTNCOMBATCORE_API FGSTN_TumbleRecoveryResolved
{
	bool bAfterHitstun = false;
	FGameplayTagContainer AllowedCommands;

	static FGSTN_TumbleRecoveryResolved Resolve(const FGSTN_TumbleRecoveryRules* Rules, const FGSTN_TumbleRecoveryExtension* Kit, EGSTN_TumbleRecoveryOverride Override);

	/** The mode alone (no copy of the command list): the per-frame question of the victim step */
	static bool IsAfterHitstun(const FGSTN_TumbleRecoveryRules* Rules, EGSTN_TumbleRecoveryOverride Override);

	/** The command list that applies, without copying it (an empty static one when neither has one) */
	static const FGameplayTagContainer& GetAllowedCommands(const FGSTN_TumbleRecoveryRules* Rules, const FGSTN_TumbleRecoveryExtension* Kit);

	/** Command may start from a recovering tumble: Allowed is empty or Command matches one of its tags */
	static bool IsCommandAllowed(const FGameplayTagContainer& Allowed, const FGameplayTag& Command);
	bool IsCommandAllowed(const FGameplayTag& Command) const { return IsCommandAllowed(AllowedCommands, Command); }
};
