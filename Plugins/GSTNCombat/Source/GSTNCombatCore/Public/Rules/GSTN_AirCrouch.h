// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Rules/GSTN_KitExtension.h"
#include "GSTN_AirCrouch.generated.h"

namespace GSTNCombatTags
{
	/**
	 * The combatant is crouched in the air (owner 2026-11-16: some aerials are done crouching, a crouched air state reads clearer). Projected
	 * by the combat tick from the kit's FGSTN_AirCrouchKitExtension, never granted. Presentation reads it (the reaction anim set's air crouch
	 * rule plays the air crouch animations), a binding may require it as data.
	 */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_AirCrouch);
}

/**
 * A kit's air crouch (owner 2026-11-16): while airborne with the hold button down, the combatant is in the air crouch state and carries
 * Combat.State.AirCrouch. Data per kit, nothing in the project settings; a kit without this extension never air-crouches.
 *
 * The state is derived every frame, never stored: airborne (Mover's ground as of the end of the previous frame, the tracker rolled back with
 * Mover) and the hold button in this frame's held buttons, both part of what a rollback restores. So it costs no sync state and a resimulation
 * projects the same tag. Today the state changes only the tag (and so the animation): fall speed, hurtbox and pushbox stay as they are
 * (defaults applied by the thread, listed in the handover air-crouch-1.md for the owner).
 *
 * Copied into the loadout's table set when it is built (UGSTN_CombatTableSet::KitAirCrouch), like the movement hurtbox: the simulation reads the
 * table of the revision it runs, never the kit asset.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Air Crouch"))
struct GSTNCOMBATCORE_API FGSTN_AirCrouchKitExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	/** Off = this kit never air-crouches (the same as no extension) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Crouch")
	bool bEnabled = true;

	/**
	 * The button held for the air crouch. None = the crouch button of the standard set (Input.Button.Crouch, whatever the player's layout maps
	 * to it: Xbox LT in the example layout). A button tag, never a key: the layout stays the player's
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Crouch", meta = (Categories = "Input.Button"))
	FGameplayTag HoldButton;

	/**
	 * Off (default) = the state ends while a move plays and comes back after it if the button is still held: the move's own animation owns the
	 * pose, and an aerial that ends crouched shows the air crouch again at once. On = the tag stays through moves (a binding or a cosmetic may read it)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Crouch")
	bool bKeepDuringMoves = false;

	/**
	 * No air crouch while the owner carries any of these (a grab, riding, a plugin's state). The hit reactions (hitstun, tumble, splat) and a KO
	 * always block it, without being listed here
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air Crouch")
	FGameplayTagContainer BlockedTags;

	/** HoldButton, or the standard crouch button when None */
	FGameplayTag ResolveHoldButton() const;

	/**
	 * Does the air crouch hold this frame? Pure, so the same facts give the same answer on every machine and in every resimulation.
	 * bAirborne: not on the ground (wall modes count as airborne for the caller to decide); bHeld: HoldButton is down in this frame's input;
	 * bInMove: a combat timeline runs; bActionLocked: a hit reaction or a KO holds the combatant; OwnerTags: the owner's tags as the projection
	 * sees them (BlockedTags are tested against them)
	 */
	bool Holds(bool bAirborne, bool bHeld, bool bInMove, bool bActionLocked, const FGameplayTagContainer& OwnerTags) const;
};
