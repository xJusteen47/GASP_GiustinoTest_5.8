// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Input/GSTN_BufferResetRules.h"
#include "StructUtils/InstancedStruct.h"
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
UENUM(BlueprintType)
enum class EGSTN_AimPolicy : uint8
{
	KeepFacing,
	SnapToStick,
	SnapIfAimOrTilt,
	Camera
};

/** Condition of a cancel window. OnHit/OnBlock are false until hits exist (M4); OnWhiff is "not hit yet". */
UENUM(BlueprintType)
enum class EGSTN_CancelCondition : uint8
{
	Always,
	OnHit,
	OnBlock,
	OnWhiff
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
	HurtboxModifier
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
 * The hitboxes of a frame range (the FGSTN_HitboxTrack of combat-m4-hits 3). The track is active for [StartFrame, EndFrame);
 * the Frame of a hitbox key counts from StartFrame. Attached to a part or group when the combatant has any, never the whole
 * body. The hits themselves are found by the attacker tick (H8); the timeline player only reports which hitboxes are open.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Hitbox"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_Hitbox : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** The part or group the hitbox is attached to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox")
	FGSTN_PartSelector Part;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox")
	TArray<FGSTN_HitboxSpec> Hitboxes;

	virtual EGSTN_TimelineTrackKind GetKind() const override { return EGSTN_TimelineTrackKind::Hitbox; }
};

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Aim")
	EGSTN_AimPolicy AimPolicy = EGSTN_AimPolicy::KeepFacing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Aim", meta = (EditCondition = "AimPolicy != EGSTN_AimPolicy::KeepFacing", ClampMin = "0.0", ClampMax = "180.0", ForceUnits = "deg"))
	float MaxTurnDegrees = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Aim", meta = (EditCondition = "AimPolicy == EGSTN_AimPolicy::Camera", ClampMin = "0", ForceUnits = "Frames"))
	int32 TurnUntilFrame = 0;

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

	/** Root-relative transform of every socket a BakedSocket hitbox uses, one per timeline frame. Written by the baking tool */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline|Tracks")
	TMap<FName, FGSTN_BakedSocketFrames> BakedSockets;

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

	/** The ricochet response of this timeline, null if none is authored */
	const FGSTN_RicochetResponse* GetRicochetResponse() const
	{
		return bHasRicochetResponse && RicochetResponse.IsAuthored() ? &RicochetResponse : nullptr;
	}

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
