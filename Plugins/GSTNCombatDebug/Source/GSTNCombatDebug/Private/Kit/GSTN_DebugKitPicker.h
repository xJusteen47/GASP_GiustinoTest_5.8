// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UWorld;

/**
 * Kit choice for play sessions (owner 2026-11-16: no loadout screen at the start of Play; kits are picked from the GSTN Debug Options
 * window). Goes through the same server path as the loadout screen (ServerRequestLoadout on the loadout component, validated by the rules)
 * and saves the choice as the "Last" preset, so nothing here bypasses the loadout rules.
 *
 *  - gstn.Debug.Kit <KitName>      gives the first local player of the play session that kit (asset name, e.g. KIT_Test_Playtest)
 *  - gstn.Debug.KitAtStart <Name>  the kit every local player gets once when its pawn first appears in a play session ("" = off).
 *                                  The hub's default is set in Config/DefaultEngine.ini [ConsoleVariables] (debug tooling, not a rule)
 * The Debug Options window lists every kit the Asset Manager knows (GetKitNames) with an Apply button.
 */
class FGSTN_DebugKitPicker
{
public:
	static void Startup();
	static void Shutdown();

	/** Every kit the Asset Manager knows, by asset name, sorted */
	static TArray<FString> GetKitNames();

	/** Asks the server for KitName on Controller's loadout component. False (and Error) when it cannot be asked */
	static bool RequestKit(APlayerController& Controller, const FString& KitName, FString& Error);

	/** The first local player controller of a play session (PIE or game), null outside one */
	static APlayerController* FindPlayController();
};
