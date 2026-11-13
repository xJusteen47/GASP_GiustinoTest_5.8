// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_CommandBindings.h"
#include "GSTN_GrabRules.generated.h"

/**
 * How the grabbed side mashes free (m6 spec 8, Mash). Frames are fixed-tick frames (62.5 Hz).
 *
 * The initial end frame of a grab is LinkFrame + clamp(BaseFrames + FramesPerDamagePercent x DamagePercent, MinFrames, MaxFrames)
 * scaled by the grabbed entity's resistance; each unconsumed press of one of the buttons (and, with bCountFlicks, each stick flick)
 * moves the end frame ReducePerPress earlier. The M1 Mash input condition is not used: it is a command matcher, and no slot fires
 * while linked.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_MashRule
{
	GENERATED_BODY()

	/** The buttons whose presses count. Empty = any button. Tags, not a mask: the host compiles them to the input config's mask */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mash", meta = (Categories = "Input.Button"))
	FGameplayTagContainer Buttons;

	/** Stick flicks (Tilt direction events) count as presses too */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mash")
	bool bCountFlicks = true;

	/** Frames of grab at 0 % damage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mash", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 BaseFrames = 150;

	/** Frames added per percent of damage the grabbed entity has taken */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mash", meta = (ClampMin = "0.0", ForceUnits = "Frames"))
	float FramesPerDamagePercent = 1.f;

	/** The grab never lasts less than this before the resistance scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mash", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 MinFrames = 60;

	/** The grab never lasts more than this before the resistance scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mash", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 MaxFrames = 480;

	/** Frames the end frame moves earlier per counted press */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mash", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ReducePerPress = 6;

	/**
	 * The attribute (by name, resolved by the GAS layer) that scales the duration of the grab on this entity: 1 = unchanged, 2 =
	 * twice as long. None = no scaling. Core has no GameplayAbilities dependency, so it holds only the name
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mash")
	FName ResistanceAttribute = NAME_None;
};

/**
 * The grab rules of a kit (m6 spec 3 and 8). The rule that decides a grab request reads them from the grabber's kit when the kit
 * carries a link extension, and from its own copy otherwise, so how heavy a target a kit can grab is the kit's data.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_GrabRules
{
	GENERATED_BODY()

	FGSTN_GrabRules();

	/**
	 * The victim's state tags must satisfy these. The default ignores Combat.Link.Linked, Combat.Immunity.Grab and hitstun; the state
	 * tags of the optional plugins are in IgnoreTagNames, because this plugin may not name them
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Request")
	FGSTN_TagRequirements Requirements;

	/**
	 * State tags of other plugins the victim must not carry, by name, resolved through RequestGameplayTag(..., false) when the
	 * request is decided: a name that no loaded plugin declares is skipped, so the list works with every combination of plugins.
	 * Default: Combat.State.Tumble, .Knockdown, .ShieldBroken and .Intangible
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Request")
	TArray<FName> IgnoreTagNames;

	/**
	 * Refuse a victim that carries Combat.Part.GrabLocked, read by name (the parts plugin projects it while its grab gate is unmet:
	 * a too-big enemy is ungrabbable until the required parts break). Without that plugin the tag does not exist and this is a no-op
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Request")
	bool bRespectGrabLocked = true;

	/** The heaviest victim this kit can grab, by the victim's effective weight (same unit as the knockback weight, 100 = a standard fighter) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Request", meta = (ClampMin = "0.0"))
	float MaxGrabWeight = 120.f;

	/** A press of the interact button released within this many frames is a tap, which grabs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Input", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 TapMaxFrames = 14;

	/** The press length that turns a tap into a hold (picking up an object, by the held-objects plugin); its pickup binding uses the same default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Input", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 HoldFrames = 15;

	/** How far from the grabber a target counts as in reach for the interact probe */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Input", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float InteractReachCm = 150.f;

	/** Which meaning wins when several targets are equally near the facing direction, first first. Empty = the interact plugin's default order */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Input", meta = (Categories = "Interact.Target"))
	TArray<FGameplayTag> InteractPriority;

	/** The victim cannot be grabbed again for this long after a release */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Immunity", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ImmunityFramesAfterRelease = 30;

	/** The victim cannot be grabbed again for this long after a throw (also for a thrown enemy in flight) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Immunity", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ImmunityFramesAfterThrow = 45;

	/** The grabber cannot grab again for this long after a throw, to stop instant regrabs on bosses */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Immunity", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 GrabberCooldownFrames = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Mash")
	FGSTN_MashRule Mash;

	/** Frames after the formation during which the grabber cannot pummel (no instant mash). A pummel never shortens the grabbed entity's mash */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Pummel", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 PummelFreeFrames = 15;

	/** Grab hits pass through the shield bubble (they are not blockable), so a grab beats a shield */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Defence")
	bool bGrabBeatsShield = true;
};

/**
 * The latch rules of a kit (owner 2026-10-26): the mirror of the grab's weight limit. A latch lands on a large enemy, so it is
 * accepted only when the victim's effective weight is at least MinLatchWeight; a victim between the two limits is grabbed or
 * latched according to which rule the hit carries, and one that fits neither is refused.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LatchRules
{
	GENERATED_BODY()

	FGSTN_LatchRules();

	/** The victim's state tags must satisfy these. The default ignores Combat.Link.Linked and Combat.Immunity.Latch (a boss that must not be ridden) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch|Request")
	FGSTN_TagRequirements Requirements;

	/** The lightest victim this kit can latch onto, by the victim's effective weight */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch|Request", meta = (ClampMin = "0.0"))
	float MinLatchWeight = 250.f;

	/**
	 * Refuse a victim that carries Combat.Part.GrabLocked, read by name. Off by default: a latch is usually how a player gets onto
	 * a big enemy whose parts are not broken yet; a design that wants the gate switches it on
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch|Request")
	bool bRespectGrabLocked = false;

	/** The victim cannot be latched again for this long after the latcher left */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Latch|Immunity", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ImmunityFramesAfterLatch = 30;
};
