// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "GSTN_TumbleLanding.generated.h"

class UAbilitySystemComponent;
struct FGSTN_CombatTickContext;
struct FGSTN_DefenceEntityData;
struct FGSTN_DefenceFragment;
struct FGSTN_InputHistory;
struct FGSTN_TechRules;
struct FGSTN_TumbleContactSyncState;
enum class EGSTN_ImpactOutcome : uint8;
enum class EGSTN_TumbleContactKind : uint8;

/**
 * Landing from a tumble, the downed state, the get-ups and the optional tech (m6 spec 6 with the owner's revision of 2026-10-23, M6 F8). Rules of
 * the combat tick over the M4 7b contact record (the knockback probe's record of the end of the previous frame), run by the defence tick extension
 * for an entity that has defence data:
 *
 *  - Tech (only when the kit's FGSTN_TechSet is not empty): ConsumeContact runs before the core resolves the surface impact of a new contact. The
 *    tech-button presses come from the input history (Input.Button.Shield, frames up to the end of the previous frame) through
 *    UGSTN_DefenceMath::EvaluateTech: the anti-mash lockout, the window before the contact. Success cancels the impact entirely (the contact is
 *    consumed): the reaction ends, the tech kind comes from the stick and the contact (PickTechType), the kit's ability of that kind starts Forced
 *    (through the model, at AfterClocks of the same frame) with its movement (in place / wall: held at rest; roll: a slide; wall jump: a launch off
 *    the wall), TechEndFrame is stamped, Combat.Event.Teched goes out. Failure returns the contact to the core: the impact is resolved FIRST, and
 *    only a floor contact with no impact outcome falls to the landing below. The hit's bNoTech skips the evaluation. Presses between contacts are
 *    registered every frame of the tumble (AfterClocks), so the lockout and Combat.State.Tech.Lockout show while airborne. A kit without a tech
 *    set registers nothing: the shield button in tumble does nothing and starts no lockout.
 *  - Landing (OnContactResolved, floor contact, impact outcome None): the reaction ends, the mini ground bounce goes out (GroundBounceVelocity
 *    with the frame's stick, as a knockback move), the kit's ground bounce ability starts Forced, and the entity is downed: KnockdownEndFrame =
 *    the contact plus MaxDownedFrames (0 = never: MAX_int32), or after a failed tech of a kit with a tech set the spec's KnockdownFrames +
 *    NoTechKnockdownBonusFrames. Combat.Event.KnockedDown goes out.
 *  - Downed: the movement lock (UGSTN_GE_DownedLock, Combat.Lock.Movement) holds while IsKnockedDown; the gate admits only the bindings that
 *    require Combat.State.Knockdown (the neutral get-up on the jump slot, the kit's get-up attacks); once the bounce is over, holding the stick
 *    starts the kit's get-up roll Forced. Any get-up that starts (Forced roll or a command) ends the downed state on its frame.
 *
 * Simulation code: deterministic, no net mode, no resimulation branch. State only in the fragment (tech stamps, knockdown end, the pending start
 * of the frame) and in the core state the model owns (the reaction is ended through FGSTN_CombatVictim::EndReaction). The contact, the presses
 * and the stick are inputs of the frame; the kit's classes are content. Every effect goes through the entity's ability system.
 */
struct GSTNDEFENCENP_API FGSTN_TumbleLanding
{
	static constexpr float FrameSeconds = 0.016f;

	/** KnockdownEndFrame of a downed state with no automatic get-up */
	static constexpr int32 NeverEnds = MAX_int32;

	/** Stick magnitude that counts as holding a direction (the get-up roll, a rolling tech) */
	static constexpr double HoldStickMagnitude = 0.5;

	/** The presses of Button in the history after the frame After, ascending */
	static void CollectPresses(const FGSTN_InputHistory& History, int32 Button, int32 After, TArray<int32, TInlineAllocator<8>>& OutFrames);

	/**
	 * The tech kind (EGSTN_TechType) for a contact: a wall gives WallTech, or WallJumpTech with the stick pushed away from the wall (none where the
	 * rules forbid wall techs); a floor gives InPlace with a neutral stick, else RollRight / RollLeft by the side the physical stick is pushed
	 * (x of the command's stick). A kind the kit has no ability for falls back to InPlace (floor) or WallTech (wall); None when the kit has neither
	 */
	static uint8 PickTechType(EGSTN_TumbleContactKind Kind, const FVector& Normal, const FVector2D& Stick, const FVector& StickOnPlane, const FGSTN_TechRules& Rules, const FGSTN_DefenceEntityData& Data);

	/** Frames of a tech kind's move (InPlaceFrames, RollFrames, WallTechFrames) */
	static int32 TechFrames(uint8 Type, const FGSTN_TechRules& Rules);

	/** The extension's ConsumeTumbleContact: true for a successful tech */
	static bool ConsumeContact(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data, const FGSTN_TumbleContactSyncState& Contact);

	/** The extension's OnTumbleContactResolved: the landing */
	static void OnContactResolved(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data, const FGSTN_TumbleContactSyncState& Contact, EGSTN_ImpactOutcome Outcome);

	/** AfterClocks: the pending start of the frame, the tech presses of a tumble, the get-up roll from the stick */
	static void AfterClocks(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data);

	/** AfterExecute: a get-up command that started this frame ends the downed state */
	static void AfterExecute(FGSTN_CombatTickContext& Context);

	/** BeforeTagProjection: the downed movement lock follows IsKnockedDown */
	static void SyncDownedLock(FGSTN_CombatTickContext& Context);

	/** Combat.State.Knockdown while the entity is downed at the frame, else none */
	static FGameplayTag AdmitsOnly(const FGSTN_CombatTickContext& Context);

	/** Whether the entity's avatar has a Mover to queue a layered move on (the move effects need one) */
	static bool CanQueueMoves(const UAbilitySystemComponent* AbilitySystem);

	/** Registers the tech-button presses (lockout) for an entity in tumble whose kit has a tech set; false when nothing applies */
	static bool RegisterPresses(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data, int32 ContactFrame, bool& bOutSuccess, int32& OutPressFrame);
};

/** Infinite: the movement lock of the downed state (grants Combat.Lock.Movement), removed when the entity gets up */
UCLASS()
class GSTNDEFENCENP_API UGSTN_GE_DownedLock : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_DownedLock();
};
