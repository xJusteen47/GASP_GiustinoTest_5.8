// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Input/GSTN_BufferResetRules.h"
#include "Curves/CurveFloat.h"
#include "StructUtils/InstancedStruct.h"
#include "Movement/GSTN_MomentumSlide.h"
#include "GSTN_TimelineTypes.generated.h"

class UAnimMontage;
class UCurveVector;

/**
 * Frame data of a combat ability (combat.md 11.3, combat-m3 6.3). Everything here is plain data in frames of the fixed
 * simulation tick; the evaluation lives in FGSTN_TimelinePlayer and the checks in FGSTN_TimelineValidation. Assets and
 * montages are soft references only, so a timeline never drags content into memory and a game can replace it by data.
 *
 * Frame ranges are half open: a track is active for StartFrame <= frame < EndFrame.
 */

/** How the character turns when the timeline starts (combat.md 8.4) */
/**
 * Where a move lives (owner 2026-11-16). Any: today's behaviour, the move plays on wherever the character is. Ground: a ground move; if the
 * character is in the air while it runs (walked off a ledge, pushed, teleported) the move is CANCELLED, except from the take-off window on
 * (FGSTN_CombatTimelineData::TakeoffFromFrame: a ground move that sends the character up, the Smash 4 wizard's dropkick) or when it was
 * started in coyote time. Air: an aerial; what a landing does is the Landing track's (FGSTN_TimelineTrack_Landing), none = plays on
 */
UENUM(BlueprintType)
enum class EGSTN_MoveGroundRule : uint8
{
	Any,
	Ground,
	Air
};

/**
 * Which way an attack faces when it starts (owner 2026-11-16: the stick has priority over the facing, "stick right while facing left, the
 * move goes right"). Applied by FGSTN_StartupSteer::ResolveOriginAim on the move's first frame (frames 0 and 1, so a move the ability graph
 * starts one frame late still turns); the turn is instant and rewrites the move origin, so the steer cone, the reverse window and the
 * WorldAtMoveStart hitboxes all start from the new direction. The stick is the one of the frame the move comes out (execution), not the one
 * of a buffered press: the latest thing the player holds wins, and the reverse window covers a late flick.
 */
UENUM(BlueprintType)
enum class EGSTN_AimPolicy : uint8
{
	/** The move starts where the character looks. For moves whose meaning is relative to the facing (a back air, a back throw) */
	KeepFacing,
	/** Snap to the move stick when it is out of the dead zone */
	SnapToStick,
	/** Snap to the aim stick while the reticle is on and it is pushed, else to the move stick */
	SnapIfAimOrTilt,
	/** Snap to the camera yaw, and keep following it until TurnUntilFrame (Focus mode style) */
	Camera,
	/**
	 * Default of every move (owner 2026-11-16): SnapIfAimOrTilt for an attack (a move with an active hitbox that reads the move stick),
	 * KeepFacing for the rest (rolls, dodges, get-ups with bIgnoreMoveInput). Added last so the stored values of the others stay the same
	 */
	Auto
};

/** What a reverse does to the character's momentum (FGSTN_ReverseWindow) */
UENUM(BlueprintType)
enum class EGSTN_ReverseMomentum : uint8
{
	/** The velocity stays as it is: only the move turns */
	Keep,
	/** The velocity turns with the facing, about the local up (a 180 reverse flips it: Smash's B-reverse) */
	TurnWithFacing
};

/**
 * The shape of the strike (plan precisione-weakpoint 2): what the hard-surface default and the stab-only hurtboxes read. Unspecified is every
 * timeline authored before the classification; it keeps the old hard-surface behaviour (immediate recoil) until the author picks a kind.
 */
UENUM(BlueprintType)
enum class EGSTN_StrikeKind : uint8
{
	Unspecified,
	/** A cut: the blade travels sideways. Default hard-surface behaviour FinishThenRecoil */
	Swing,
	/** A thrust along the blade. Default ImmediateRecoil; the only kind that reaches stab-only hurtboxes */
	Stab
};

/** What an attack does when it lands on a part too hard for it (a ricochet; plan precisione-weakpoint 2) */
UENUM(BlueprintType)
enum class EGSTN_HardSurfaceBehaviour : uint8
{
	/** From the strike kind: Swing finishes then recoils, Stab and Unspecified recoil at once */
	Auto,
	/** The swing completes its active frames (sliding along the surface when the timeline's deflection is on), then the recoil replaces the recovery */
	FinishThenRecoil,
	/** The recoil starts on the frame after the contact (the M4 H12 behaviour) */
	ImmediateRecoil
};

/**
 * How a swing slides along a hard surface after a ricochet (plan precisione-weakpoint 1). The slide is recomputed every tick from the timeline,
 * the contact frame and the quantised contact normal of the sync state (FGSTN_HardContactState), so every machine sweeps the same bent path.
 * Only Root and BakedSocket hitboxes slide; WorldAtMoveStart hitboxes keep their authored path.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_SweepDeflection
{
	GENERATED_BODY()

	/** Off = the swing keeps its authored path through the hard part (which it no longer hits) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deflection")
	bool bEnabled = true;

	/** The largest bend of one frame's motion the slide accepts; a steeper hit stops the blade (its hitboxes close for the rest of the swing) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deflection", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "90.0", ForceUnits = "deg"))
	float MaxDeflectAngleDeg = 60.f;

	/** Frames after the contact during which the path bends; after them the offset reached so far is kept and the path follows the authored motion */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deflection", meta = (EditCondition = "bEnabled", ClampMin = "0", ForceUnits = "Frames"))
	int32 MaxDeflectFrames = 8;
};

/** The strike settings one hitbox group runs with, its own or the move's (FGSTN_CombatTimelineData::ResolveStrike). Plain data, never Auto */
struct FGSTN_StrikeSettings
{
	EGSTN_StrikeKind Kind = EGSTN_StrikeKind::Unspecified;
	EGSTN_HardSurfaceBehaviour Behaviour = EGSTN_HardSurfaceBehaviour::ImmediateRecoil;
	const FGSTN_SweepDeflection* Deflection = nullptr;

	/** Auto resolved from the kind: Swing finishes then recoils, Stab and Unspecified recoil at once */
	static EGSTN_HardSurfaceBehaviour ResolveBehaviour(EGSTN_StrikeKind InKind, EGSTN_HardSurfaceBehaviour InBehaviour)
	{
		if (InBehaviour != EGSTN_HardSurfaceBehaviour::Auto)
		{
			return InBehaviour;
		}
		return InKind == EGSTN_StrikeKind::Swing ? EGSTN_HardSurfaceBehaviour::FinishThenRecoil : EGSTN_HardSurfaceBehaviour::ImmediateRecoil;
	}
};

/**
 * Condition of a cancel window. OnHit/OnBlock are false until hits exist (M4); OnWhiff is "not hit yet". OnParry is the value of the
 * defence plugin (M6): it is data only here, false in every state this module writes, so a plugin that raises the parry confirm gives it a meaning.
 */
UENUM(BlueprintType)
enum class EGSTN_CancelCondition : uint8
{
	Always,
	OnHit,
	OnBlock,
	OnWhiff,
	OnParry
};

/** Whether the movement track adds to the velocity of the character or replaces it */
UENUM(BlueprintType)
enum class EGSTN_MoveMixMode : uint8
{
	Additive,
	Override
};

/**
 * What happens to the velocity the character already has (V0, sampled on the frame the movement track starts) when the track
 * starts and ends. The track's own contribution is called T(f) below.
 */
UENUM(BlueprintType)
enum class EGSTN_MomentumMode : uint8
{
	/** V0 is zeroed on the first frame of the track. From then on the velocity is only T(f) (the mix mode makes no difference) */
	Discard,
	/** V0 is kept untouched. Additive: velocity = V0 + T(f) for the whole track. Override: T(f) replaces it while the track runs, and V0 is not restored afterwards */
	Preserve,
	/** V0 is kept like Preserve while the track runs (so the mix mode applies to it), then zeroed on the frame after the last frame of the track */
	ClearAtEnd
};

/** Which weapon a switch track selects */
UENUM(BlueprintType)
enum class EGSTN_SwitchTarget : uint8
{
	Next,
	Previous,
	Index
};

/** What a hurtbox modifier does to the hurtboxes it addresses (the resolved states are EGSTN_HurtState) */
UENUM(BlueprintType)
enum class EGSTN_HurtboxModifierKind : uint8
{
	/** Hits land but do no damage */
	Invincible,
	/** Hits pass through */
	Intangible,
	/** No knockback or hitstun while the stagger force is below the track's ArmorThreshold; damage applies */
	Armor
};

/** Discriminator of the closed set of track types, so the player can cast without RTTI and without a type chain */
enum class EGSTN_TimelineTrackKind : uint8
{
	None,
	CancelWindow,
	Movement,
	Tags,
	Event,
	WeaponSwitch,
	Hitbox,
	HurtboxModifier,
	/** FGSTN_TimelineTrack_Hurtbox: a hitbox track's shapes used as extra hurtboxes of the one who plays the move */
	Hurtbox
};

/** Which hurtbox detail level a move asks for while it runs (FGSTN_CombatTimelineData::HurtboxDetail) */
UENUM(BlueprintType)
enum class EGSTN_HurtboxDetailOverride : uint8
{
	/** The level of the character's hurtbox set (Simple unless the set says otherwise) */
	UseSet,
	Simple,
	Detailed
};

/**
 * Addresses hurtboxes or hitboxes by part name, group name or tag, never by "the whole body". A selector with nothing set
 * addresses nothing (validation reports it): invincibility of the whole character is spelled by naming the group that
 * holds every part. The ids are only checked against the character's real parts when the caller gives a list (M4 defines
 * the part vocabulary), so assets authored now stay valid.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_PartSelector
{
	GENERATED_BODY()

	/** One part, e.g. a foot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FName PartName = NAME_None;

	/** A group of parts, e.g. the feet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FName GroupName = NAME_None;

	/** Every part carrying this tag (matches hierarchically). A game tag of any root, so no Categories filter */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FGameplayTag PartTag;

	bool IsEmpty() const { return PartName.IsNone() && GroupName.IsNone() && !PartTag.IsValid(); }
};

/**
 * Base of every track: the frame range it covers. Polymorphic through TInstancedStruct like the input conditions; the set
 * of kinds is closed (see EGSTN_TimelineTrackKind) so the player evaluates them with a switch. A track that fires at one
 * frame (event, weapon switch) uses StartFrame (SwapFrame for the switch) and ignores EndFrame.
 */
USTRUCT(BlueprintType, meta = (Hidden))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack
{
	GENERATED_BODY()

	virtual ~FGSTN_TimelineTrack() = default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (ClampMin = "0"))
	int32 StartFrame = 0;

	/** Exclusive */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Track", meta = (ClampMin = "0"))
	int32 EndFrame = 1;

	virtual EGSTN_TimelineTrackKind GetKind() const { return EGSTN_TimelineTrackKind::None; }

	bool Contains(int32 Frame) const { return Frame >= StartFrame && Frame < EndFrame; }
};

/** While open, the running timeline may be cancelled into the commands the query matches. */
USTRUCT(BlueprintType, meta = (DisplayName = "Cancel Window"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_CancelWindow : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** Matched against the command tags (e.g. Command.Special | Command.Movement.Jump) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cancel")
	FGameplayTagQuery IntoCommands;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cancel")
	EGSTN_CancelCondition Condition = EGSTN_CancelCondition::Always;

	virtual EGSTN_TimelineTrackKind GetKind() const override { return EGSTN_TimelineTrackKind::CancelWindow; }
};

/** One layered move: a velocity curve in the local space of the character over the track's frames. */
USTRUCT(BlueprintType, meta = (DisplayName = "Movement"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_Movement : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** Velocity in cm/s, local space, sampled by frames since StartFrame. Soft: resolved by the simulation side */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	TSoftObjectPtr<UCurveVector> VelocityCurve;

	/** Scale applied to the gravity of the character along its own gravity direction while the track runs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float GravityScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bLockFacing = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	EGSTN_MoveMixMode MixMode = EGSTN_MoveMixMode::Override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	EGSTN_MomentumMode MomentumMode = EGSTN_MomentumMode::Discard;

	/** Ends the track (and the timeline's airborne part) on landing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bCancelOnLand = false;

	/** Frames the character stays locked after landing when bCancelOnLand ends the track */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 LandingLagFrames = 0;

	/**
	 * Air control (Mover's AirControlPercentage, 0..1) while the track runs and the character is airborne, in place of the kit's (owner 2026-11-17:
	 * the ground dodge is a highly controllable long jump). Below 0 = the kit's. Read by the run stage air values, so it needs the kit's run stages
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float AirControlOverride = -1.f;

	virtual EGSTN_TimelineTrackKind GetKind() const override { return EGSTN_TimelineTrackKind::Movement; }
};

/** Tags the owner carries while the window is open (lock tags allowed). Granted by the combat tick, never by the ability. */
USTRUCT(BlueprintType, meta = (DisplayName = "Tags"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_Tags : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer Tags;

	virtual EGSTN_TimelineTrackKind GetKind() const override { return EGSTN_TimelineTrackKind::Tags; }
};

/** A cue and/or a gameplay event at StartFrame. */
USTRUCT(BlueprintType, meta = (DisplayName = "Event"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_Event : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event", meta = (Categories = "GameplayCue"))
	FGameplayTag CueTag;

	/** May trigger other abilities, which are adopted if they are timelines */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	FGameplayTag GameplayEventTag;

	/**
	 * Names this key for the move's own Blueprint (CP1 of the Blueprint seams): UGSTN_GA_BaseCombat raises On Move Key with it on the frame
	 * the key is crossed, once, on every machine and in every re-simulation. Every Event key raises it; an empty tag is a key the graph tells
	 * apart by its frame. Any tag works: the graph compares it, nothing else reads it
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	FGameplayTag KeyTag;

	/**
	 * Authoritative event key (seam D-1, game-modes.md 2): when set, the combat tick posts a sim event of this type to the entity's outbox on the
	 * frame the key is crossed, with OutboxTags and OutboxMagnitude (a spin's sense, a count). The outbox is published on the authority only,
	 * once per frame, so level objects and modes react to the move without a server request of their own. Empty = cue and gameplay event only
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event|Outbox")
	FGameplayTag OutboxEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event|Outbox", meta = (EditCondition = "OutboxEventTag.IsValid()"))
	FGameplayTagContainer OutboxTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event|Outbox", meta = (EditCondition = "OutboxEventTag.IsValid()"))
	float OutboxMagnitude = 0.f;

	/**
	 * Move actions run on the frame the key is crossed (CP2 of the Blueprint seams), after the cue and the gameplay event, before On Move Key:
	 * fire a projectile, send an event, apply an effect, open a tag window. The action types live in the modules that can run them (the base
	 * FGSTN_MoveAction in GSTNCombatNP, the projectile shot in GSTNProjectileNP), so this pure module names them only by path: without those
	 * modules the list is just data nobody runs. Each runs inside the simulation, once per crossing and again in a re-simulation
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event|Actions", meta = (BaseStruct = "/Script/GSTNCombatNP.GSTN_MoveAction", ExcludeBaseStruct))
	TArray<FInstancedStruct> Actions;

	virtual EGSTN_TimelineTrackKind GetKind() const override { return EGSTN_TimelineTrackKind::Event; }
};

/** Swaps the active weapon at SwapFrame. Only on weapon switch abilities. */
USTRUCT(BlueprintType, meta = (DisplayName = "Weapon Switch"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_WeaponSwitch : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 SwapFrame = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch")
	EGSTN_SwitchTarget Target = EGSTN_SwitchTarget::Next;

	/** Weapon index when Target is Index */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch", meta = (ClampMin = "0"))
	int32 Index = 0;

	virtual EGSTN_TimelineTrackKind GetKind() const override { return EGSTN_TimelineTrackKind::WeaponSwitch; }
};

/**
 * What the attacker does when its hit lands on a part that is too hard for the attack (a ricochet, combat-m4 3b): no
 * hitstop, the timeline ends into this recoil. Pure data; M3 only reads the attacker-outcome flag (never set before M4).
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_RicochetResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ricochet")
	TSoftObjectPtr<UAnimMontage> RecoilMontage;

	/** Length of the recoil, counted from the frame the ricochet starts. 0 = no response authored */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ricochet", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 RecoilFrames = 0;

	/** Optional recoil movement; its frames count from the start of the recoil */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ricochet")
	bool bHasMovement = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ricochet", meta = (EditCondition = "bHasMovement"))
	FGSTN_TimelineTrack_Movement Movement;

	/** Played once when the recoil starts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ricochet", meta = (Categories = "GameplayCue"))
	FGameplayTag CueTag;

	bool IsAuthored() const { return RecoilFrames > 0; }
};

/**
 * The hitboxes of a frame range (the FGSTN_HitboxTrack of combat-m4-hits 3). The track is active for [StartFrame, EndFrame);
 * the Frame of a hitbox key counts from StartFrame. Attached to a part or group when the combatant has any, never the whole
 * body. The hits themselves are found by the attacker tick (H8); the timeline player only reports which hitboxes are open.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Hitbox"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_Hitbox : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** The designer's name of the group, shown by the editor instead of "Group N" (owner 2026-11-16). A label only: nothing looks a group up by it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox")
	FName GroupName = NAME_None;

	/** The part or group the hitbox is attached to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox")
	FGSTN_PartSelector Part;

	/**
	 * Parenting (owner 2026-11-16): on = the group is a transform of its own, GroupOffset in GroupSpace (on GroupSocket), and every hitbox with
	 * bAttachToGroup is its child: its keys are relative to the group, so moving the group moves them all. Off = each hitbox is placed by its own
	 * Space and Socket, the behaviour of every timeline made before parenting existed
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox|Group Transform")
	bool bUseGroupTransform = false;

	/** Where the group lives: Root, BakedSocket (on GroupSocket) or WorldAtMoveStart. Projectile is not a group space and places nothing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox|Group Transform", meta = (EditCondition = "bUseGroupTransform"))
	EGSTN_HitboxSpace GroupSpace = EGSTN_HitboxSpace::Root;

	/** The baked socket (or bone) the group follows with GroupSpace BakedSocket. Baked like a hitbox's socket (Ctrl+B) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox|Group Transform", meta = (EditCondition = "bUseGroupTransform && GroupSpace == EGSTN_HitboxSpace::BakedSocket"))
	FName GroupSocket = NAME_None;

	/** The group's transform in its space: its children's keys are relative to it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox|Group Transform", meta = (EditCondition = "bUseGroupTransform"))
	FTransform GroupOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox")
	TArray<FGSTN_HitboxSpec> Hitboxes;

	/**
	 * Off = the move's StrikeKind, HardSurfaceBehaviour and SweepDeflection apply to this group. On = this group's own: a move that slashes then
	 * thrusts is two hitbox tracks, one Swing and one Stab (plan precisione-weakpoint, owner 2026-11-09). Nothing here is tied to a weapon or a
	 * blade: a punch or a kick marked Swing slides and recoils on a hard part like a sword does
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox|Strike")
	bool bOverrideStrike = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox|Strike", meta = (EditCondition = "bOverrideStrike"))
	EGSTN_StrikeKind StrikeKind = EGSTN_StrikeKind::Unspecified;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox|Strike", meta = (EditCondition = "bOverrideStrike"))
	EGSTN_HardSurfaceBehaviour HardSurfaceBehaviour = EGSTN_HardSurfaceBehaviour::Auto;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox|Strike", meta = (EditCondition = "bOverrideStrike"))
	FGSTN_SweepDeflection SweepDeflection;

	/** This group's own recoil, first in the chain: group, move, weapon, kit, project default of the strike kind (UGSTN_CombatSettings) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox|Strike")
	bool bHasRicochetResponse = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox|Strike", meta = (EditCondition = "bHasRicochetResponse"))
	FGSTN_RicochetResponse RicochetResponse;

	virtual EGSTN_TimelineTrackKind GetKind() const override { return EGSTN_TimelineTrackKind::Hitbox; }

	/** Is this hitbox a child of the group's transform? */
	bool IsParentedToGroup(const FGSTN_HitboxSpec& Spec) const { return bUseGroupTransform && Spec.bAttachToGroup; }

	/** The space a hitbox is placed in after parenting: the group's for a child, its own otherwise */
	EGSTN_HitboxSpace GetEffectiveSpace(const FGSTN_HitboxSpec& Spec) const { return IsParentedToGroup(Spec) ? GroupSpace : Spec.Space; }

	/** The socket a hitbox follows after parenting (meaningful with GetEffectiveSpace BakedSocket) */
	FName GetEffectiveSocket(const FGSTN_HitboxSpec& Spec) const { return IsParentedToGroup(Spec) ? GroupSocket : Spec.Socket; }

	/**
	 * What sits between a hitbox's evaluated keys and its space: the group's offset for a child, identity otherwise. Every reader places a hitbox as
	 * Keys * GetParentOffset * Space, so the simulation, the minions, the bake and the editor preview agree
	 */
	const FTransform& GetParentOffset(const FGSTN_HitboxSpec& Spec) const { return IsParentedToGroup(Spec) ? GroupOffset : FTransform::Identity; }
};

/**
 * Extra hurtboxes of the character playing the move, for a frame range (owner 2026-11-16): the leg stretched by a kick, the arm of a long punch, so
 * the attack can be hit back where it reaches (Smash-like extended hurtboxes). It IS a hitbox track for everything but the hit: same shapes, keys,
 * spaces (Root and BakedSocket; WorldAtMoveStart and Projectile are not hurtbox spaces and place nothing), group transform, names, so the timeline
 * editor edits it exactly like a hitbox group. Nothing of its hitboxes' hit data is read, and the attacker never sees it: the only readers are
 * FGSTN_HurtboxPose (the victim side) and the editor.
 *
 * A hit on one of its shapes counts as a hit on the set hurtbox named CountsAs (the torso, the leg it extends): that hurtbox's priority, reaction
 * scale, tags, part and state (a Hurtbox Modifier on it applies to its extension too). None = the set's first hurtbox.
 * Kept by the derived struct, so every reader of hitbox tracks that filters by GetKind (the player, the phases) or by GetHitboxTrack skips it.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Hurtbox"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_Hurtbox : public FGSTN_TimelineTrack_Hitbox
{
	GENERATED_BODY()

	/** The hurtbox of the character's set this group's shapes count as. None = the set's first one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hurtbox")
	FName CountsAs = NAME_None;

	/**
	 * On = while the track is open, this group's shapes REPLACE the CountsAs hurtbox instead of adding to it: the hand-made feet of a move (owner
	 * 2026-11-16, "the feet need care in some moves, I will do those by hand"). Off = they add to it (a stretched limb)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hurtbox")
	bool bReplaceCountsAs = false;

	virtual EGSTN_TimelineTrackKind GetKind() const override { return EGSTN_TimelineTrackKind::Hurtbox; }
};

namespace GSTNTimelineTracks
{
	/** The attack hitbox track of a track instance: null for anything else, including a Hurtbox track (a hitbox track by inheritance, never an attack) */
	inline const FGSTN_TimelineTrack_Hitbox* AsAttackHitbox(const TInstancedStruct<FGSTN_TimelineTrack>& Instance)
	{
		const FGSTN_TimelineTrack_Hitbox* Track = Instance.GetPtr<FGSTN_TimelineTrack_Hitbox>();
		return Track && Track->GetKind() == EGSTN_TimelineTrackKind::Hitbox ? Track : nullptr;
	}

	inline FGSTN_TimelineTrack_Hitbox* AsAttackHitboxMutable(TInstancedStruct<FGSTN_TimelineTrack>& Instance)
	{
		FGSTN_TimelineTrack_Hitbox* Track = Instance.GetMutablePtr<FGSTN_TimelineTrack_Hitbox>();
		return Track && Track->GetKind() == EGSTN_TimelineTrackKind::Hitbox ? Track : nullptr;
	}
}

/**
 * Changes how the addressed hurtboxes take hits for a frame range (the feet of a dodge, an armored arm). Which hurtboxes,
 * which frames and which kind are designer data; nothing in C++ or in an ability decides them. A hurtbox is addressed when it
 * matches ANY of: Part.PartName or AffectedHurtboxes by name, Part.PartTag or AffectedTags by tag (hierarchical). Part.GroupName
 * is reserved for the parts layer (a hurtbox has no group of its own). Nothing set addresses nothing (validation reports it):
 * the whole body is spelled by naming it, never by leaving the selector empty.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Hurtbox Modifier"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_HurtboxModifier : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** The hurtboxes affected, by part name, group name or tag. Never empty together with the lists below (validation) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hurtbox")
	FGSTN_PartSelector Part;

	/** The state the addressed hurtboxes take (Normal is "no track") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hurtbox")
	EGSTN_HurtboxModifierKind Kind = EGSTN_HurtboxModifierKind::Invincible;

	/** With Kind Armor: hits whose stagger force is below this do not stagger. Infinite is super armor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hurtbox", meta = (EditCondition = "Kind == EGSTN_HurtboxModifierKind::Armor"))
	FGSTN_StaggerThreshold ArmorThreshold;

	/** Hurtboxes by FGSTN_Hurtbox::Name, in union with Part and AffectedTags */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hurtbox")
	TArray<FName> AffectedHurtboxes;

	/** Hurtboxes whose Tags match any of these, in union with the names */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hurtbox", meta = (Categories = "Combat.Hurtbox"))
	FGameplayTagContainer AffectedTags;

	/**
	 * The portion of the addressed hurtboxes the state covers, in root-relative shapes (the first 40 % of a foot, a weak
	 * point left exposed). Empty = the whole hurtbox. Data only in H1: the HurtState step (H4) decides with the contact point.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hurtbox")
	TArray<FGSTN_HitShape> ShapeOverrides;

	virtual EGSTN_TimelineTrackKind GetKind() const override { return EGSTN_TimelineTrackKind::HurtboxModifier; }

	/** Does the selector (names, tag, lists) leave nothing to address? */
	bool IsEmpty() const { return Part.IsEmpty() && AffectedHurtboxes.IsEmpty() && AffectedTags.IsEmpty(); }

	/** Is this hurtbox one of the addressed (union of names and tags)? */
	bool Addresses(const FGSTN_Hurtbox& Hurtbox) const
	{
		if (!Part.PartName.IsNone() && Part.PartName == Hurtbox.Name)
		{
			return true;
		}
		if (AffectedHurtboxes.Contains(Hurtbox.Name))
		{
			return true;
		}
		if (Part.PartTag.IsValid() && Hurtbox.Tags.HasTag(Part.PartTag))
		{
			return true;
		}
		return !AffectedTags.IsEmpty() && Hurtbox.Tags.HasAny(AffectedTags);
	}
};

/**
 * The cancellable tail of a timeline (combat.md 11.3, plan owner rule R-A): from the tail start on, the timeline may be
 * cancelled by anything (any command, stick movement, jump) and the montage blends out over BlendOutFrames. Generic on
 * purpose: stagger in M4 is the same pattern (an unskippable part, then a cancellable tail). A timeline without a tail has
 * no cancellable part: only its cancel windows and override rules apply.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_TimelineTail
{
	GENERATED_BODY()

	/** Off = no cancellable tail */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tail")
	bool bHasTail = false;

	/** First frame of the tail. Negative = FirstActionableFrame. Clamped to the timeline length */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tail", meta = (EditCondition = "bHasTail", ForceUnits = "Frames"))
	int32 StartFrame = INDEX_NONE;

	/** Frames the montage takes to blend out when the tail is cancelled; clamped to what is left of the timeline */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tail", meta = (EditCondition = "bHasTail", ClampMin = "0", ForceUnits = "Frames"))
	int32 BlendOutFrames = 6;

	/** The tail start in [0, TotalFrames], INDEX_NONE if there is no tail */
	int32 ResolveStartFrame(int32 TotalFrames, int32 FirstActionableFrame) const
	{
		if (!bHasTail)
		{
			return INDEX_NONE;
		}
		return FMath::Clamp(StartFrame < 0 ? FirstActionableFrame : StartFrame, 0, FMath::Max(TotalFrames, 0));
	}

	/** Blend-out frames that fit between the resolved start and the end */
	int32 ResolveBlendOutFrames(int32 TotalFrames, int32 FirstActionableFrame) const
	{
		const int32 Start = ResolveStartFrame(TotalFrames, FirstActionableFrame);
		return Start == INDEX_NONE ? 0 : FMath::Clamp(BlendOutFrames, 0, FMath::Max(TotalFrames - Start, 0));
	}
};

/**
 * What the socket baker (combat-m5-editor 7) used to fill FGSTN_CombatTimelineData::BakedSockets, so the editor can tell later that the
 * frames no longer match their sources. Authoring data only: the member that holds it is WITH_EDITORONLY_DATA, so it is stripped from
 * cooked builds and never reaches a simulation or the network. The struct type itself is always declared (UHT needs it either way).
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_BakeInfo
{
	GENERATED_BODY()

	/** Hash of every input of the bake (montage data, section, rate, length, tick rate, mesh, sockets). 0 together with no When = never baked by the editor */
	UPROPERTY(VisibleAnywhere, Category = "Bake")
	uint32 InputHash = 0;

	/** Frames per second of the simulation tick the frames were sampled at */
	UPROPERTY(VisibleAnywhere, Category = "Bake")
	float TickRateHz = 0.f;

	/** The skeletal mesh the sockets were read from */
	UPROPERTY(VisibleAnywhere, Category = "Bake", meta = (AllowedClasses = "/Script/Engine.SkeletalMesh"))
	FSoftObjectPath Mesh;

	/** UTC time of the bake. Zero ticks = never baked by the editor */
	UPROPERTY(VisibleAnywhere, Category = "Bake")
	FDateTime When = FDateTime(0);

	bool IsSet() const { return When.GetTicks() != 0; }
};

/**
 * Which stick turns a move during its steer window (FGSTN_SteerWindow, owner decision 2026-11-14: chosen per move). The stick is read from the
 * frame's predicted input command, never from a fixed key: the move stick and the aim stick are whatever the player's input config maps to them.
 */
UENUM(BlueprintType)
enum class EGSTN_SteerSource : uint8
{
	/** The aim stick, only while the precision reticle is on (plan precisione-weakpoint 5). Every move authored before 2026-11-14; for moves that need a reticle */
	AimStickWithReticle,
	/** The move stick, reticle or not (Monster Hunter style): the default the owner wants for new melee moves */
	MoveStick,
	/** The aim stick while the reticle is on and the aim stick is pushed, else the move stick */
	MoveOrAimStick,
};

/**
 * When and how a move turns during its startup (research handoffs/attack-aiming-research.md Â§2, owner 2026-11-14). The cap and the rate stay
 * MaxSteerDegrees and SteerRateDegPerSecond on the timeline; this says which stick, in which frames, and how the facing is held. Pure data:
 * FGSTN_StartupSteer::ResolveFrame turns it into a yaw per frame from the move origin, the frame's command and the facing of the previous frame.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_SteerWindow
{
	GENERATED_BODY()

	/** Which stick steers. AimStickWithReticle keeps the behaviour of every move made before this struct existed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steer")
	EGSTN_SteerSource Source = EGSTN_SteerSource::AimStickWithReticle;

	/** First timeline frame the stick steers in (a charge that only turns from frame 6, a signature aerial from 0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steer", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 StartFrame = 0;

	/**
	 * Stick magnitude (unit circle) under which a stick does not aim this move: the start snap (AimPolicy), the reverse window (never below
	 * this, whatever its StickThreshold) and the steering. A resting thumb must not turn an attack. 0.25 is the value every move had while it
	 * was a constant (FGSTN_StartupSteer::DeadZone)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steer", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StickDeadZone = 0.25f;

	/**
	 * Frame the direction locks at (exclusive). -1 = the first active frame (the hitboxes never sweep while the stick turns them). A value past
	 * the first active frame is honoured but tracks "for most of the animation", which players read as unfair (Absolver)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steer", meta = (ClampMin = "-1", ForceUnits = "Frames"))
	int32 EndFrame = -1;

	/**
	 * Multiplier of SteerRateDegPerSecond over the window: X = 0 at StartFrame, 1 at the lock; Y = the factor (1 = full rate). Free at the start
	 * and heavy toward the lock, say. No keys = 1. Ignored when the rate is 0 (the turn is then immediate)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steer")
	FRuntimeFloatCurve RateScale;

	/**
	 * Move stick sources only: from the move's first frame to the end of its last active frame, the facing the move holds replaces the turn the
	 * movement mode would make toward the move stick. Without it the stick keeps turning the character freely (the cap and the lock mean nothing)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steer")
	bool bHoldFacing = true;

	/** Shows the steering arrows and the allowed arc for this move (presentation; GSTN.Combat.SteerIndicator overrides it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steer")
	bool bShowIndicator = true;
};

/**
 * Reverse window (owner 2026-11-16, Smash's turnaround / B-reverse): in the first frames after a move starts, pushing the move stick hard enough
 * away from the move's direction turns the whole move to the stick, at once. Pure data, read by FGSTN_StartupSteer::ResolveOriginAim. It only
 * applies to attacks (a move with an active hitbox, without bIgnoreMoveInput) and never reaches the first active frame. Frame 0 is the start aim's
 * (AimPolicy): a stick already held back when the move comes out is a turnaround, not a reverse, and keeps the momentum.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ReverseWindow
{
	GENERATED_BODY()

	/** The window is timeline frames 1 to Frames (inclusive), cut before the first active frame. 0 = the move cannot be reversed (a back air) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reverse", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 Frames = 3;

	/** How far the move stick must be pushed (0..1 of the unit circle). High, so a thumb drifting back does not turn an attack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reverse", meta = (ClampMin = "0.25", ClampMax = "1.0"))
	float StickThreshold = 0.7f;

	/** How far from the move's direction the stick must point to count as "back" (180 = only straight back) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reverse", meta = (ClampMin = "0.0", ClampMax = "180.0", ForceUnits = "deg"))
	float MinAngleDegrees = 120.f;

	/** What the reverse does to the momentum. Keep by default; TurnWithFacing for aerial specials that should flip the drift */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reverse")
	EGSTN_ReverseMomentum Momentum = EGSTN_ReverseMomentum::Keep;
};

/**
 * The frame-data source of truth of a combat ability, inline on the ability or in a UGSTN_CombatTimeline asset.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_CombatTimelineData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Frames", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 TotalFrames = 0;

	/** First frame the character is free again by the normal rules (cancel windows and override rules apply before it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Frames", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 FirstActionableFrame = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Frames")
	FGSTN_TimelineTail Tail;

	/** Played with the synced montage at the start. Soft: never loaded by the data */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Animation")
	TSoftObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Animation")
	FName MontageSection = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.f;

	/**
	 * How the montage leaves when the timeline ends by itself (TotalFrames reached; owner 2026-11-13: a move cut before the second punch of its
	 * animation must not show that punch). -1 = as before: the montage's own blend, with the animation still playing under it. 0 = cut on the
	 * last frame. More = a blend over that many frames. A cancelled tail blends over Tail's BlendOutFrames instead. Presentation only: the
	 * simulation ends at TotalFrames whatever this says
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Animation", meta = (ClampMin = "-1", ForceUnits = "Frames"))
	int32 EndBlendOutFrames = -1;

	/**
	 * On: while the montage blends out (at the end, or when the tail is cancelled) the pose of the last frame is held, so nothing of the
	 * montage past the timeline is drawn. Off: the montage keeps playing under the blend, which can show the start of what follows in it
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Animation")
	bool bHoldPoseOnEnd = false;

	/**
	 * A timeline ability (a UGSTN_GA_BaseCombat child) the combat model starts on the frame this timeline ends BY ITSELF (TotalFrames reached; never
	 * after a cancel, a hit or a replacing move): the move links into its recovery, e.g. the dropkick's land clip into a get-up, instead of handing
	 * the pose back to the locomotion graph. Started like a forced timeline (source Forced), so its own frame data, cancel windows and tail apply;
	 * the combat model adds it to the timeline registry by itself (no binding needed). None = the move ends as before. Soft: loaded with the
	 * timeline's content, never in a tick
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Chain", meta = (AllowedClasses = "/Script/GSTNCombatNP.GSTN_GA_BaseCombat"))
	TSoftClassPtr<UObject> FollowUpTimeline;

	/** Ground, air or either (EGSTN_MoveGroundRule). Any = no rule, the behaviour of every timeline made before 2026-11-16 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Ground")
	EGSTN_MoveGroundRule GroundRule = EGSTN_MoveGroundRule::Any;

	/**
	 * Ground moves only: the timeline frame from which the character may leave the ground (the take-off), -1 = never (any air frame cancels).
	 * A move with a take-off may also start in coyote time (its binding takes Input.Context.Coyote as Grounded)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Ground", meta = (EditCondition = "GroundRule == EGSTN_MoveGroundRule::Ground", ClampMin = "-1", ForceUnits = "Frames"))
	int32 TakeoffFromFrame = -1;

	/** Ground moves only: the take-off must happen before this frame (exclusive); later, an air frame cancels again. -1 = until the end */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Ground", meta = (EditCondition = "GroundRule == EGSTN_MoveGroundRule::Ground && TakeoffFromFrame >= 0", ClampMin = "-1", ForceUnits = "Frames"))
	int32 TakeoffUntilFrame = -1;

	/** A ground move that may take off (and so may start in coyote time) */
	bool AllowsTakeoff() const { return GroundRule == EGSTN_MoveGroundRule::Ground && TakeoffFromFrame >= 0; }

	/** Frame is inside the take-off window */
	bool IsTakeoffFrame(int32 Frame) const { return AllowsTakeoff() && Frame >= TakeoffFromFrame && (TakeoffUntilFrame < 0 || Frame < TakeoffUntilFrame); }

	/** Which way the move faces when it starts: the stick by default for attacks (EGSTN_AimPolicy::Auto, owner 2026-11-16) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Aim")
	EGSTN_AimPolicy AimPolicy = EGSTN_AimPolicy::Auto;

	/** The most the start aim turns the move from where the character looks, either way (180 = any direction) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Aim", meta = (EditCondition = "AimPolicy != EGSTN_AimPolicy::KeepFacing", ClampMin = "0.0", ClampMax = "180.0", ForceUnits = "deg"))
	float MaxTurnDegrees = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Aim", meta = (EditCondition = "AimPolicy == EGSTN_AimPolicy::Camera", ClampMin = "0", ForceUnits = "Frames"))
	int32 TurnUntilFrame = 0;

	/**
	 * Right-stick steering during startup (plan precisione-weakpoint 5, FGSTN_StartupSteer): the most the move's direction can turn
	 * from the facing it started with, either way. On top of AimPolicy, for every policy. 0 = the aim stick does not steer this move
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Aim", meta = (ClampMin = "0.0", ClampMax = "180.0", ForceUnits = "deg"))
	float MaxSteerDegrees = 20.f;

	/**
	 * Aim handling while this move turns with the reticle on (M11 L9, the "ergonomics" of a heavy weapon, owner 2026-11-12): the fastest the
	 * steered facing follows the aim stick, inside the MaxSteerDegrees cone. So the move's rotation limit says how far it may turn and
	 * this says how fast. 0 = at once (every move authored before L9). The facing is Mover sync state, so stepping from it is rollback-safe
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Aim", meta = (EditCondition = "MaxSteerDegrees > 0", ClampMin = "0.0", ForceUnits = "deg/s"))
	float SteerRateDegPerSecond = 0.f;

	/**
	 * Which stick steers, in which frames, and whether the facing is held (owner 2026-11-14). The default reproduces the reticle steer above.
	 * 180 in MaxSteerDegrees with a short window is a signature move "aimable in any direction" in its first frames (Helm Breaker style)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Aim", meta = (EditCondition = "MaxSteerDegrees > 0"))
	FGSTN_SteerWindow Steer;

	/** Turnaround / B-reverse in the first frames (owner 2026-11-16): a hard flick of the move stick away from the move turns it. See FGSTN_ReverseWindow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Aim")
	FGSTN_ReverseWindow Reverse;

	/**
	 * While the timeline runs the move stick neither walks nor turns the character: Mover gets no move input and the facing of the previous
	 * frame as its orientation, so the only motion is the move's own (its pushes, root motion). For moves that must not drift or turn under
	 * a held stick: the get-ups and get-up rolls (react-hook-1 review: the roll back turned and slid about 1 m). The stick still reaches the
	 * combat tick (DI, the commands)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Movement")
	bool bIgnoreMoveInput = false;

	/**
	 * What the move does with the running speed it starts with, on the ground (owner 2026-11-13): slide on the feet along the momentum,
	 * differently when started against it. Off by default. Separate from root motion and from movement tracks; see FGSTN_MomentumSlide
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Movement")
	FGSTN_MomentumSlide MomentumSlide;

	/** Startup override window (combat.md 8.3). -1 = the default of the command tier (settings), 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Override", meta = (ClampMin = "-1", ForceUnits = "Frames"))
	int32 OverrideFrames = -1;

	/** Commands that may replace the timeline inside the override window; empty = any command of a higher tier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Override")
	FGameplayTagQuery OverrideInto;

	/** Appended to the buffer reset rules while the timeline runs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Input")
	TArray<TInstancedStruct<FGSTN_BufferResetRule>> BufferResetRules;

	/** The tracks, in any order; the player scans them once per frame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Tracks")
	TArray<TInstancedStruct<FGSTN_TimelineTrack>> Tracks;

	/**
	 * Root-relative transform of every socket a BakedSocket hitbox uses, and of every bone a hurtbox follows (FGSTN_Hurtbox::bFollowBone), one per
	 * timeline frame. Written by the baking tool
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Tracks")
	TMap<FName, FGSTN_BakedSocketFrames> BakedSockets;

	/**
	 * Bones baked for the hurtboxes of whoever plays this move, on top of the ones the editor finds by itself (the bones followed by every hurtbox set
	 * of the project that the bake mesh has). Usually empty; add a bone here when a set the editor cannot find (made at run time) follows it
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Hurtboxes")
	TArray<FName> HurtboxBones;

	/** The hurtbox detail level while this move runs (owner 2026-11-16). UseSet = the set's own (Simple unless it says otherwise) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Hurtboxes")
	EGSTN_HurtboxDetailOverride HurtboxDetail = EGSTN_HurtboxDetailOverride::UseSet;

#if WITH_EDITORONLY_DATA
	/**
	 * What the editor's baker used for BakedSockets (combat-m5-editor 7.3). Not read at run time and not in any serializer of the simulation;
	 * absent from cooked data. Frames written by another tool have none, which the editor reports as "baked outside the editor".
	 */
	UPROPERTY(VisibleAnywhere, Category = "Timeline|Tracks")
	FGSTN_BakeInfo BakeInfo;
#endif

	/** Optional recoil the timeline ends into when the attacker-outcome flag says ricochet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Ricochet")
	bool bHasRicochetResponse = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Ricochet", meta = (EditCondition = "bHasRicochetResponse"))
	FGSTN_RicochetResponse RicochetResponse;

	/** Cut or thrust of the move: the default of every hitbox group that does not override it (FGSTN_TimelineTrack_Hitbox::bOverrideStrike) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Ricochet")
	EGSTN_StrikeKind StrikeKind = EGSTN_StrikeKind::Unspecified;

	/** Auto follows StrikeKind; an aerial stab with long active frames is the usual reason to pick FinishThenRecoil by hand */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Ricochet")
	EGSTN_HardSurfaceBehaviour HardSurfaceBehaviour = EGSTN_HardSurfaceBehaviour::Auto;

	/** The slide along the hard surface while FinishThenRecoil completes the swing; default of the groups that do not override it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Ricochet")
	FGSTN_SweepDeflection SweepDeflection;

	/** HardSurfaceBehaviour with Auto resolved: never Auto */
	EGSTN_HardSurfaceBehaviour ResolveHardSurfaceBehaviour() const
	{
		if (HardSurfaceBehaviour != EGSTN_HardSurfaceBehaviour::Auto)
		{
			return HardSurfaceBehaviour;
		}
		return FGSTN_StrikeSettings::ResolveBehaviour(StrikeKind, HardSurfaceBehaviour);
	}

	/** One past the last frame any hitbox track is open (0 with none) */
	int32 GetActiveEndFrame() const;

	/** The hitbox track at TrackIndex (an index into Tracks), null when it is not one (a Hurtbox track is not) */
	const FGSTN_TimelineTrack_Hitbox* GetHitboxTrack(int32 TrackIndex) const
	{
		return Tracks.IsValidIndex(TrackIndex) ? GSTNTimelineTracks::AsAttackHitbox(Tracks[TrackIndex]) : nullptr;
	}

	/** The strike settings of a hitbox group: its own when it overrides them, the move's otherwise (also for INDEX_NONE) */
	FGSTN_StrikeSettings ResolveStrike(int32 TrackIndex) const;

	/** Where a FinishThenRecoil recoil begins: one past the last frame of the group that met the hard face (the move's active end without one) */
	int32 GetStrikeEndFrame(int32 TrackIndex) const
	{
		const FGSTN_TimelineTrack_Hitbox* Track = GetHitboxTrack(TrackIndex);
		return Track ? Track->EndFrame : GetActiveEndFrame();
	}

	/** The ricochet response of this timeline, null if none is authored */
	const FGSTN_RicochetResponse* GetRicochetResponse() const
	{
		return bHasRicochetResponse && RicochetResponse.IsAuthored() ? &RicochetResponse : nullptr;
	}

	/**
	 * The response of a hitbox group: its own when authored, else the move's; null = the caller's fallbacks. A Landing track's index gives its
	 * landing lag (FGSTN_TimelineTrack_Landing: the lag is played by the recoil player), never the move's response; a Ledge Check's index gives
	 * its Recoil response the same way (FGSTN_TimelineTrack_LedgeCheck)
	 */
	const FGSTN_RicochetResponse* GetRicochetResponse(int32 TrackIndex) const;

	/**
	 * The baked root-relative transform of a socket at a timeline frame. A frame past the baked range holds the last one and
	 * a negative frame the first. False for an unknown or empty socket.
	 */
	bool FindBakedSocket(FName Socket, int32 Frame, FTransform& OutTransform) const
	{
		const FGSTN_BakedSocketFrames* Baked = BakedSockets.Find(Socket);
		if (!Baked || Baked->Frames.IsEmpty())
		{
			return false;
		}
		OutTransform = Baked->Frames[FMath::Clamp(Frame, 0, Baked->Frames.Num() - 1)];
		return true;
	}

	/** First frame of the cancellable tail, INDEX_NONE if there is none */
	int32 GetTailStartFrame() const { return Tail.ResolveStartFrame(TotalFrames, FirstActionableFrame); }

	/** The override window length once the default of the tier is known (used when OverrideFrames is -1) */
	int32 ResolveOverrideFrames(int32 DefaultOverrideFrames) const { return FMath::Max(OverrideFrames < 0 ? DefaultOverrideFrames : OverrideFrames, 0); }
};
