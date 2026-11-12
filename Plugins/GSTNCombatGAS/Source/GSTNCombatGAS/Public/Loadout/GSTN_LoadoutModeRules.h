// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameRules/GSTN_GameRules.h"
#include "GSTN_LoadoutModeRules.generated.h"

/** Whose stats a combatant has while a support weapon is active (combat-loadout-mods 6, owner question M11.6 still open) */
UENUM(BlueprintType)
enum class EGSTN_SupportWeaponStats : uint8
{
	/**
	 * The support weapon's own stats effect, when it authors one (M3 7.7 as built): a heavy weapon may slow its carrier. A support
	 * weapon without one leaves the kit's stats, as in KitOnly
	 */
	WeaponOwn,
	/** Support weapons never change stats: their stats effect is ignored and the carrier keeps the kit's stats (the spec's rule 6) */
	KitOnly
};

/**
 * The combat loadout's piece of the game rules (M11 L3; owner rule 2026-11-11: no gameplay factor in Project Settings). One per GameRules
 * asset, chosen by the server per mode or level; the class defaults are the rules when no asset is active and keep today's behaviour.
 * Read with UGSTN_LoadoutModeRules::Get() outside the simulation only (loadout apply, server requests): a change of the active rules
 * re-applies every loadout (UGSTN_LoadoutComponent::HandleGameRulesChanged), so what the simulation derives from it is rebuilt at a
 * revision boundary and never read mid-tick.
 */
UCLASS(meta = (DisplayName = "Combat loadout"))
class GSTNCOMBATGAS_API UGSTN_LoadoutModeRules : public UGSTN_GameRulesPiece
{
	GENERATED_BODY()

public:
	/** The active rules' piece, else the defaults */
	static const UGSTN_LoadoutModeRules& Get() { return FGSTN_ActiveGameRules::GetPiece<UGSTN_LoadoutModeRules>(); }

	/**
	 * Both answers to M11.6 ("may a heavy support weapon slow you?") kept as data until the owner picks one. Applies to every weapon
	 * after the kit's primary one (the loadout's AdditionalWeapons). TEST VALUE: WeaponOwn, the behaviour before M11
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Support weapons")
	EGSTN_SupportWeaponStats SupportWeaponStats = EGSTN_SupportWeaponStats::WeaponOwn;

	/**
	 * Whether a player may change a special among the weapon's allowed alternatives during a match (combat-loadout-mods 7, question
	 * M11.9), through UGSTN_LoadoutComponent::ServerRequestSpecialSwap. The loadout screen (ServerRequestLoadout) is never gated by it.
	 * False = specials change in the loadout only, the spec's default; a mode with supply points or the hub turns it on
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Specials")
	bool bAllowSpecialSwapInMatch = false;
};
