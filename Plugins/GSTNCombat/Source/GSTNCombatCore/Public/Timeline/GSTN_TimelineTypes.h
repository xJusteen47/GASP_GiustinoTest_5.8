// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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

/** What a hurtbox modifier does to the hurtboxes it addresses. Armor arrives with hits (M4), together with its threshold. */
UENUM(BlueprintType)
enum class EGSTN_HurtboxModifierKind : uint8
{
	/** Hits land but do no damage */
	Invincible,
	/** Hits pass through */
	Intangible
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

/** Declared so assets can be authored now; ignored until hits exist (M4). Attached to a part or group, never the whole body. */
USTRUCT(BlueprintType, meta = (DisplayName = "Hitbox"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_Hitbox : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** The part or group the hitbox is attached to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox")
	FGSTN_PartSelector Part;

	virtual EGSTN_TimelineTrackKind GetKind() const override { return EGSTN_TimelineTrackKind::Hitbox; }
};

/**
 * Changes how the addressed hurtboxes take hits for a frame range (the feet of a dodge, an armored arm). Which parts, which
 * frames and which kind are designer data; nothing in C++ or in an ability decides them. Declared now, ignored until M4.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Hurtbox Modifier"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_HurtboxModifier : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** The hurtboxes affected, by part name, group name or tag. Never empty (validation) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hurtbox")
	FGSTN_PartSelector Part;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hurtbox")
	EGSTN_HurtboxModifierKind Kind = EGSTN_HurtboxModifierKind::Invincible;

	virtual EGSTN_TimelineTrackKind GetKind() const override { return EGSTN_TimelineTrackKind::HurtboxModifier; }
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

	/** First frame of the cancellable tail, INDEX_NONE if there is none */
	int32 GetTailStartFrame() const { return Tail.ResolveStartFrame(TotalFrames, FirstActionableFrame); }

	/** The override window length once the default of the tier is known (used when OverrideFrames is -1) */
	int32 ResolveOverrideFrames(int32 DefaultOverrideFrames) const { return FMath::Max(OverrideFrames < 0 ? DefaultOverrideFrames : OverrideFrames, 0); }
};
