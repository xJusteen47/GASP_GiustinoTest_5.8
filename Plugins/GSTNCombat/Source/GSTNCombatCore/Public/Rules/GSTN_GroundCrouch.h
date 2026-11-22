// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Hits/GSTN_HurtboxPose.h"
#include "Rules/GSTN_KitExtension.h"
#include "Timeline/GSTN_BodyCapsule.h"
#include "GSTN_GroundCrouch.generated.h"

namespace GSTNCombatTags
{
	/**
	 * The combatant is crouched on the ground (owner 2026-11-16: holding crouch on the ground did nothing in any kit once GASP's IA_Crouch left the
	 * input mapping). Projected by the combat tick from the kit's FGSTN_GroundCrouchKitExtension, never granted. A binding may require it as data
	 * (the crouch attacks keep their Hold(Crouch) shape, which already works); presentation may read it.
	 */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Crouch);
}

/**
 * A kit's ground crouch: on the ground with the hold button down, the combatant crouches. EVERY kit crouches: a kit without this extension uses
 * these defaults, so the extension is only added to tune or switch off (bEnabled). Data per kit, nothing in the project settings.
 *
 * What a crouch does (all rollback-safe, no new sync state in the combat model):
 *  - Combat.State.Crouch on the owner (projected every frame like Combat.State.AirCrouch);
 *  - the movement capsule shrinks to the crouch capsule through the body capsule modifier (the per-move capsule mechanism; a Body Capsule track
 *    of a running move still wins). The modifier lives in Mover's sync state, so a rollback restores it, and it reports Mover.IsCrouching, which
 *    GASP's animation and walking mode read: the crouch locomotion of the sample plays, as it did with IA_Crouch;
 *  - the pushbox follows the movement capsule on its own (a pushbox with no authored height takes the capsule's);
 *  - the movement hurtbox becomes CrouchHurtboxShapes while crouched outside moves (Simple detail level).
 *
 * The stance is decided from facts every machine has for the frame: the hold button in the frame's held buttons, Mover's ground and the crouch
 * of the previous frame (both in Mover's rolled back state), the combat state. Copied into the loadout's table set (UGSTN_CombatTableSet::KitGroundCrouch).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Ground Crouch"))
struct GSTNCOMBATCORE_API FGSTN_GroundCrouchKitExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	FGSTN_GroundCrouchKitExtension();

	/** Off = this kit never crouches on the ground */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Crouch")
	bool bEnabled = true;

	/**
	 * The button held to crouch. None = the crouch button of the standard set (Input.Button.Crouch: Xbox LT in the example layout). A button tag,
	 * never a key: the layout stays the player's
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Crouch", meta = (Categories = "Input.Button"))
	FGameplayTag HoldButton;

	/**
	 * On (default) = crouch walk, as the sample does (GASP crouch locomotion, Mover's speed while crouched). Off = Smash-like: the stick does not
	 * walk while crouched (the facing holds), it only shapes the crouch attacks. Both shipped as a setting (owner rule: two plausible behaviours)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Crouch")
	bool bCrouchWalk = true;

	/**
	 * On (default) = a move started while crouched keeps the crouch capsule (a crouch attack does not pop the body up for its length), unless the
	 * move has its own Body Capsule track, which always wins. A move never STARTS the crouch: pressing crouch during a jab waits for its end
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Crouch")
	bool bKeepStanceDuringMoves = true;

	/** Off (default) = Combat.State.Crouch drops while a move plays (the move's clip owns the pose). On = the tag stays through moves */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Crouch")
	bool bKeepTagDuringMoves = false;

	/** Crouch capsule half height (Mover's sample default is 55). Unreal raises it to the radius when smaller */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Crouch", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float CapsuleHalfHeight = 55.f;

	/** Crouch capsule radius, 0 = the pawn's own */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Crouch", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float CapsuleRadius = 0.f;

	/**
	 * The body volume(s) while crouched outside moves, at the Simple detail level (root-relative like FGSTN_MovementHurtboxExtension: the root is the
	 * crouched capsule's centre). Default: one capsule from the feet to about one metre. Empty = the standing movement hurtbox stays
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Crouch")
	TArray<FGSTN_MovementHurtboxShape> CrouchHurtboxShapes;

	/** No ground crouch while the owner carries any of these. Hit reactions and a KO always block it, without being listed here */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Crouch")
	FGameplayTagContainer BlockedTags;

	/** HoldButton, or the standard crouch button when None */
	FGameplayTag ResolveHoldButton() const;

	/**
	 * Is the combatant crouched (capsule, Mover.IsCrouching) this frame? Pure: the same facts give the same answer on every machine and in every
	 * resimulation. bGrounded: Mover's ground (end of the previous frame); bHeld: HoldButton in this frame's held buttons; bInMove: a combat
	 * timeline runs; bActionLocked: a hit reaction or KO holds the combatant; bWasCrouched: the stance of the previous frame (Mover state)
	 */
	bool HoldsStance(bool bGrounded, bool bHeld, bool bInMove, bool bActionLocked, bool bWasCrouched, const FGameplayTagContainer& OwnerTags) const;

	/** Combat.State.Crouch this frame: the stance, minus moves unless bKeepTagDuringMoves */
	bool HoldsTag(bool bGrounded, bool bHeld, bool bInMove, bool bActionLocked, bool bWasCrouched, const FGameplayTagContainer& OwnerTags) const;

	/** The crouch movement capsule for a pawn whose own capsule is Standing: bottom on the mesh root (between the feet) */
	FGSTN_BodyCapsuleShape MakeCapsule(const FGSTN_BodyCapsuleShape& Standing) const { return MakeCapsule(CapsuleHalfHeight, CapsuleRadius, Standing); }

	/** The same from the two capsule values (the body capsule modifier gets them from the frame's decision, not the struct) */
	static FGSTN_BodyCapsuleShape MakeCapsule(float HalfHeight, float Radius, const FGSTN_BodyCapsuleShape& Standing);

	/** The movement hurtbox while crouched (the shapes in the movement hurtbox form FGSTN_HurtboxPose reads), Detail from the kit's own */
	FGSTN_MovementHurtboxExtension MakeHurtbox(const FGSTN_MovementHurtboxExtension* KitMovement) const;

	/** The default crouched body: a capsule from the feet to ~1 m, root-relative to the crouched capsule centre (55 above the feet) */
	static FGSTN_HitShape MakeDefaultCrouchBody();
};
