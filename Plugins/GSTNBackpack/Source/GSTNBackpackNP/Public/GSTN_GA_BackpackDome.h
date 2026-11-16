// Backpacks, Network Prediction half (M11 L7b).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_GA_BackpackDome.generated.h"

/**
 * The shield-dome backpack's raise (owner 2026-11-12: an area dome, on the dodge button held with the stick neutral, in PvE too). A timeline
 * ability: its timeline carries a FGSTN_BackpackDomeTrack (radius, lifetime) and a FGSTN_BackpackChargeTrack (its memory slot), so the
 * combat tick raises the dome and spends the charge in the frame the timeline starts; the ability holds nothing. It ends when the button goes
 * up (bEndOnTriggerRelease), which with the WhileHeld lifetime lowers the dome; the timeline's length caps the hold.
 *
 * Defaults are a first guess for the playtest, tuned in a Blueprint child: 400 cm radius, WhileHeld, at most 10 s, 2 charges, one back every
 * 20 s (slot 0). What the dome does to hits is not in this slice: the dome is sync state and a tag (Backpack.State.Dome) only.
 */
UCLASS(Blueprintable)
class GSTNBACKPACKNP_API UGSTN_GA_BackpackDome : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_BackpackDome(const FObjectInitializer& ObjectInitializer);

	/** The default timeline: TotalFrames long, a charge track on Slot and a dome track. Public so the tests and tools build the same one */
	static FGSTN_CombatTimelineData MakeDefaultTimeline(int32 TotalFrames = 625, int32 Slot = 0);
};
