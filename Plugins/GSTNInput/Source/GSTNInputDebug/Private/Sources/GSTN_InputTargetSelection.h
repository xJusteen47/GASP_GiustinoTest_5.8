// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"

/** One local player of a game or PIE world, as far as the choice of what to show is concerned. A plain value: the selection logic reads nothing else */
struct FGSTN_InputTargetCandidate
{
	/** Stable across play sessions: "<PIE instance>/<local player index>", so a choice made in one session finds its player in the next */
	FString Key;

	/** For the picker: "Client 1 (PIE 2): BP_Fighter" */
	FText Label;

	/** The window of this player has the keyboard focus right now */
	bool bFocused = false;

	/** The pawn of this player has something the display can read */
	bool bHasSource = false;
};

/**
 * Which local player the input display shows. With several PIE windows (a listen-server host and its clients) there are several local
 * players, one per window, and the display must follow the one the developer is looking at: the window that has the focus, or, once the
 * focus moved to the display itself (typing in its script box, clicking the picker), the window that had it last. A player picked in the
 * combo is shown whatever has the focus. Pure and without a world: GSTNInput.Debug.InputDisplay.TargetSelection runs it on lists.
 */
class FGSTN_InputTargetSelection
{
public:
	/** Follow the focused window (the default) */
	void SetFollowFocus() { bFollowFocus = true; }

	/** Show one player whatever has the focus. The key of a player that is gone falls back to following the focus until it comes back */
	void SetFixed(const FString& Key)
	{
		bFollowFocus = false;
		FixedKey = Key;
	}

	bool IsFollowingFocus() const { return bFollowFocus; }
	const FString& GetFixedKey() const { return FixedKey; }

	/** The key of the player to show among the candidates in order, empty when there are none */
	FString Choose(TConstArrayView<FGSTN_InputTargetCandidate> Candidates);

private:
	bool bFollowFocus = true;
	FString FixedKey;

	/** The window that had the focus last, remembered while the focus is on the display or the editor */
	FString LastFocusedKey;
};
