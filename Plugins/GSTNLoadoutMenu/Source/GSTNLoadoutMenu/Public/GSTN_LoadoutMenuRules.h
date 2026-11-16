// Pre-mission loadout screen: its piece of the game rules.

#pragma once

#include "CoreMinimal.h"
#include "GameRules/GSTN_GameRules.h"
#include "GSTN_LoadoutMenuRules.generated.h"

class UWorld;

/** A loadout debug pad (AGSTN_LoadoutMenuPad) put in a level at Play, so a test level gets one without being saved with it */
USTRUCT(BlueprintType)
struct GSTNLOADOUTMENU_API FGSTN_LoadoutMenuPadPlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pad")
	TSoftObjectPtr<UWorld> Level;

	/** Where the pad lies (its centre on the floor) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pad", meta = (ForceUnits = "cm"))
	FVector Location = FVector::ZeroVector;

	/** The way its label faces */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pad", meta = (ForceUnits = "deg"))
	float Yaw = 0.f;

	/** The bare debug list (true) or the normal loadout screen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pad")
	bool bDebug = true;
};

/**
 * When the loadout screen opens by itself, per mode or level (a GameRules piece, owner rule: no gameplay factor in Project Settings). The
 * class defaults come from the plugin's DefaultGSTNLoadoutMenu.ini (a config class, not in Project Settings): the playtest hub opens it,
 * the harness levels do not. Mods and specials change before the match only (combat-loadout-mods 7, bAllowSpecialSwapInMatch stays off):
 * the screen opens when a player's pawn first appears in a level, and again only through the console (GSTN.LoadoutMenu) or a game flow
 * calling UGSTN_LoadoutMenuSubsystem::OpenMenu.
 */
UCLASS(config = GSTNLoadoutMenu, meta = (DisplayName = "Loadout screen"))
class GSTNLOADOUTMENU_API UGSTN_LoadoutMenuRules : public UGSTN_GameRulesPiece
{
	GENERATED_BODY()

public:
	/** The active rules' piece, else the defaults */
	static const UGSTN_LoadoutMenuRules& Get() { return FGSTN_ActiveGameRules::GetPiece<UGSTN_LoadoutMenuRules>(); }

	/** Whether the screen opens by itself in World: every level, or one listed in AutoOpenLevels */
	bool ShouldAutoOpenIn(const UWorld& World) const;

	/** Opens the screen in every level when a player first spawns (a mode with a briefing) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Opening")
	bool bAutoOpenEverywhere = false;

	/** The levels where the screen opens by itself when a player first spawns (the playtest hub) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Opening")
	TArray<TSoftObjectPtr<UWorld>> AutoOpenLevels;

	/**
	 * Loadout debug pads added to a level when Play starts (UGSTN_LoadoutMenuPadSubsystem): stepping on one opens the screen for the player
	 * standing on it. A level can also hold AGSTN_LoadoutMenuPad actors of its own
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Opening")
	TArray<FGSTN_LoadoutMenuPadPlacement> Pads;

	/** The player's saved loadout (UGSTN_LoadoutSubsystem preset) the screen starts from and writes on confirm */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Saving")
	FString LastLoadoutPreset = TEXT("Last");

	/** Stick deflection that counts as a direction */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ClampMin = "0.1", ClampMax = "0.95"))
	float StickThreshold = 0.5f;

	/** Holding a direction repeats it after this long... */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float RepeatDelay = 0.35f;

	/** ...then every this long */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ClampMin = "0.03", ForceUnits = "s"))
	float RepeatInterval = 0.12f;
};
