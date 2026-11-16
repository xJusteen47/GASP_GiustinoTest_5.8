// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameRules/GSTN_GameRules.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"
#include "GSTN_LoadoutModeRules.generated.h"

/** Whose stats a combatant has while a support weapon is active (combat-loadout-mods 6, owner answer M11.6: WeaponOwn) */
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

/** Where the support weapon goes when a player takes a whole kit from the ground (owner answer M11.2: StaysWithPlayer) */
UENUM(BlueprintType)
enum class EGSTN_KitSwapSupportWeapon : uint8
{
	/**
	 * The player's support weapon (their last additional weapon) comes with them, when the new kit's rules accept it; if the ground
	 * kit carried one, the two trade places. When the new kit cannot carry it, it stays on the ground with the old kit
	 */
	StaysWithPlayer,
	/** The support weapon stays on the ground with the old kit; the player gets the ground kit exactly as it lies */
	StaysWithKit
};

/** The Ex and Super meters when a player takes a whole kit from the ground (owner question M11.3: decided by playtest) */
UENUM(BlueprintType)
enum class EGSTN_KitSwapMeters : uint8
{
	/** The meters keep their value, capped at the new kit's maximum */
	Keep,
	/** The meters start empty with the new kit */
	Reset
};

/** Whose mods apply to a kit taken from the ground (owner answer M11.4: GroundKit) */
UENUM(BlueprintType)
enum class EGSTN_KitSwapMods : uint8
{
	/** The mods that lie with the kit on the ground (its previous carrier's setup); the player's own setups stay on the ground with their old kit */
	GroundKit,
	/** The player's own saved setup for the new kit; the ground kit's setups stay on the ground with the old kit */
	PlayerSetup
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
	 * Both answers to M11.6 ("may a heavy support weapon slow you?") kept as data. Applies to every weapon after the kit's primary one
	 * (the loadout's AdditionalWeapons). Default WeaponOwn: the owner's answer (2026-11-12, a heavy weapon may slow you)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Support weapons")
	EGSTN_SupportWeaponStats SupportWeaponStats = EGSTN_SupportWeaponStats::WeaponOwn;

	/** Kit swap from the ground: where the support weapon goes (M11.2). Default = the owner's answer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit swap")
	EGSTN_KitSwapSupportWeapon KitSwapSupportWeapon = EGSTN_KitSwapSupportWeapon::StaysWithPlayer;

	/** Kit swap from the ground: the Ex and Super meters (M11.3, to be settled by playtest). TEST VALUE: Keep */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit swap")
	EGSTN_KitSwapMeters KitSwapMeters = EGSTN_KitSwapMeters::Keep;

	/** Kit swap from the ground: whose mods apply to the new kit (M11.4). Default = the owner's answer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit swap")
	EGSTN_KitSwapMods KitSwapMods = EGSTN_KitSwapMods::GroundKit;

	/** The loadout part of the kit swap rules, for the pure swap (FGSTN_KitPickupPayload) */
	FGSTN_KitSwapCarryOver MakeKitSwapCarryOver() const
	{
		FGSTN_KitSwapCarryOver CarryOver;
		CarryOver.bSupportWeaponStaysWithPlayer = KitSwapSupportWeapon == EGSTN_KitSwapSupportWeapon::StaysWithPlayer;
		CarryOver.bModsStayWithPlayer = KitSwapMods == EGSTN_KitSwapMods::PlayerSetup;
		return CarryOver;
	}

	/**
	 * Whether a player may change a special among the weapon's allowed alternatives during a match (combat-loadout-mods 7, question
	 * M11.9), through UGSTN_LoadoutComponent::ServerRequestSpecialSwap. The loadout screen (ServerRequestLoadout) is never gated by it.
	 * False = specials change in the loadout only, the spec's default; a mode with supply points or the hub turns it on
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Specials")
	bool bAllowSpecialSwapInMatch = false;
};
