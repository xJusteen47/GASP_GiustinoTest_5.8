// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"

class APlayerController;

/**
 * Forced skin for captures and proportion checks (owner 2026-11-23, the Lyra cosmetic cheat idea rewritten): puts a skin on pawns
 * whatever their loadout says, on this machine only, through UGSTN_SkinComponent::SetForcedSkin. Presentation only: the loadout, the
 * kit, hitboxes and the simulation are untouched, so a forced skin never changes what a capture measures in the game.
 *
 *  - gstn.Debug.Skin <SkinName>       forces that skin on the first local player's pawn (asset name, e.g. SKIN_Test_Big)
 *  - gstn.Debug.Skin base             forces the pawn's original look (the base mannequin), to compare
 *  - gstn.Debug.Skin off              back to the loadout's skin
 *  - add "all" as second argument     every pawn of the play world (targets, dummies), to compare body sizes side by side
 * The Debug Options window has the same as a Skin row.
 */
class FGSTN_DebugSkinPicker
{
public:
	/** Every skin the Asset Manager knows, by asset name, sorted */
	static TArray<FString> GetSkinNames();

	/** SkinName, "base" or "off" on Controller's pawn, or on every pawn of its world. False (and Error) when nothing was forced */
	static bool ForceSkin(APlayerController& Controller, const FString& SkinName, bool bAllPawns, FString& Error);
};
