// Defence, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/GSTN_DefenceViews.h"
#include "GSTN_DefenceMath.generated.h"

class UGSTN_DefenceRules;

/**
 * Stateless shield, parry, tech and landing math of the defence plugin (m6 spec 3 "Math", 5.3, 5.4, 6). Every function takes its
 * inputs as arguments and reads no world, actor or component: the same inputs give the same result on every machine and in a
 * resimulation. Doubles and integers throughout; frames are fixed-tick frames (62.5 Hz).
 *
 * The shield is a bubble of FIXED size (owner decision 2026-10-26): no function here takes the shield value or the stick to size,
 * shift or tilt the bubble. The value only decides the break, the stun and the pushback.
 */
UCLASS()
class GSTNDEFENCECORE_API UGSTN_DefenceMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---- Availability ----

	/**
	 * The one test of whether the shield exists for an entity (m6 spec 1d): the mode's rules enable it AND the kit has one. False
	 * means no binding, no bubble, no Combat.State.Shield* tag. A null rules asset has no shield.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static bool IsShieldAvailable(const UGSTN_DefenceRules* Rules, bool bKitHasShield);

	// ---- Shield value ----

	/**
	 * The shield value at Frame, analytically from the stamped view. Held: V - Drain * (Frame - Stamp), floored at 0. Released: V until
	 * max(ReleaseFrame + RegenDelayFrames, StampFrame), then V + Regen * (Frame - that), capped at Max. A view that was never raised
	 * (ReleaseFrame INDEX_NONE) regenerates from StampFrame + RegenDelayFrames. A frame at or before the stamp returns the stamped value.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static double ShieldValueAt(const FGSTN_ShieldView& View, int32 Frame, const FGSTN_ShieldRates& Rates);

	/** The view re-stamped at Frame with the value the OLD rates give there. Call it when a rate changes: the new rates then apply from Frame on (documented approximation of m6 5.3) */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static FGSTN_ShieldView RestampShield(const FGSTN_ShieldView& View, int32 Frame, const FGSTN_ShieldRates& Rates);

	/** The press event: value stamped at Frame, held, PressFrame = Frame, no release */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static FGSTN_ShieldView RaiseShield(const FGSTN_ShieldView& View, int32 Frame, const FGSTN_ShieldRates& Rates);

	/** The release event: value stamped at Frame, no longer held, ReleaseFrame = Frame */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static FGSTN_ShieldView ReleaseShield(const FGSTN_ShieldView& View, int32 Frame, const FGSTN_ShieldRates& Rates);

	/** A blocked hit: value stamped at Frame minus Damage, floored at 0. The bubble keeps its size. A value of 0 afterwards is a break */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static FGSTN_ShieldView ApplyShieldDamage(const FGSTN_ShieldView& View, int32 Frame, const FGSTN_ShieldRates& Rates, double Damage);

	/**
	 * The shield damage of a blocked hit (m6 5.3): the hit's ShieldDamage, or Damage * ShieldDamagePerDamage when the hit says 0,
	 * times the victim's IncomingShieldDamageMultiplier and the relationship scale.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static double ShieldDamageOf(double HitShieldDamage, double HitDamage, const FGSTN_ShieldRules& Rules, double VictimMultiplier, double RelationshipScale);

	/** The scale of IncomingShieldDamageScaleByRelationship for a relationship tag: the exact entry, else the deepest entry the tag matches, else 1 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static double RelationshipScaleOf(const FGSTN_ShieldRules& Rules, FGameplayTag Relationship);

	// ---- Bubble ----

	/** The bubble's centre relative to the entity root: CentreHeightCm along the entity's local up (so any gravity works). A zero up gives the origin */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static FVector ShieldCentreAt(const FGSTN_ShieldRules& Rules, const FVector& EntityUp);

	/** The world bubble of an entity: the centre from its transform's local up, the radius RadiusCm. There is no value or tilt input */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static FGSTN_ShieldBubble MakeShieldBubble(const FGSTN_ShieldRules& Rules, const FTransform& EntityTransform);

	/**
	 * The bubble as the extra hurtbox the snapshot extension contributes (m6 5.3 "Contact"): a root-relative sphere of RadiusCm
	 * centred CentreHeightCm up, the highest priority, ignoring the hit types in IgnoreHitTypes (the non-blockable ones), tagged with
	 * the defence feature id as its source. Root-relative, so the lag-compensation history needs only the entity transform.
	 */
	static FGSTN_ExtraHurtbox MakeShieldBubbleHurtbox(const FGSTN_ShieldRules& Rules, const FGameplayTagContainer& IgnoreHitTypes);

	// ---- Reaction to a blocked hit ----

	/** min(MaxShieldStun, ShieldStunBase + ShieldStunPerDamage * Damage), rounded half up. A negative damage counts as 0 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static int32 ShieldStunFrames(double Damage, const FGSTN_ShieldRules& Rules);

	/** PushbackSpeed + PushbackSpeedPerDamage * Damage in cm/s, for PushbackFrames. A negative damage counts as 0 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static double PushbackSpeedOf(double Damage, const FGSTN_ShieldRules& Rules);

	// ---- Parry ----

	/**
	 * Whether a hit stamped at HitFrame lands inside the shield's perfect-shield window. Anchor Press: [PressFrame, PressFrame + Window).
	 * Anchor Release: [ReleaseFrame, ReleaseFrame + Window), only if the shield was held at least MinHeldFramesBeforeRelease; a shield
	 * still held (ReleaseFrame INDEX_NONE) has no release window. A window of 0 frames never opens.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static bool EvaluateParryWindow(int32 PressFrame, int32 HitFrame, int32 ReleaseFrame, const FGSTN_ParryRules& Rules);

	/**
	 * The one parry decision both sources go through (m6 5.4). Common conditions first (the hit is parryable, neither side is immune, the
	 * hit is not a grab, a projectile only where the profile allows it), then the shield source, then the move source. The shield
	 * wins when both would parry. The profile is the shield's (Rules.Parry.ProfileIndex) or the move's.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static FGSTN_ParryDecision ResolveParry(const FGSTN_ParryContext& Context, const UGSTN_DefenceRules* Rules);

	/** The window end after a parried hit: HitFrame + 1 with bCloseOnParry (so hits of the same frame are parried together), else unchanged */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static int32 MoveWindowEndAfterParry(bool bCloseOnParry, int32 HitFrame, int32 WindowEndFrame);

	/** The attacker's stun for a hit class: the exact entry of the profile's map, else the deepest entry the hit type matches, else AttackerStunFrames */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static int32 AttackerStunFramesOf(const FGSTN_ParryProfile& Profile, FGameplayTag HitType);

	/** Whether the container carries Combat.Immunity.Parry (read by name, so the tag is the same whoever granted it) */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static bool HasParryImmunity(const FGameplayTagContainer& Tags);

	/** Whether the hit type is Grab, Throw or Pummel. Read by name: the link plugin declares them and this plugin must run without it */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static bool IsGrabLikeHitType(FGameplayTag HitType);

	// ---- Tech ----

	/**
	 * The press rule of m6 6: a press counts unless it falls before the end of hitstop, before the end of the lockout (it does NOT
	 * extend the lockout), or the entity is in hitstun that is not a tumble and the rules do not tech there (no lockout either). An
	 * accepted press stamps LastTechPressFrame and starts the lockout. Returns whether the press counted.
	 */
	static bool RegisterTechPress(FGSTN_TechState& State, int32 PressFrame, int32 HitstopEndFrame, bool bTumble, const FGSTN_TechRules& Rules);

	/**
	 * The tech decision of m6 6 from the tech-button presses of the history (ascending frames) and the newest tumble contact: each
	 * press newer than LastTechPressFrame goes through RegisterTechPress; then, if ContactFrame is new, success iff a counted press
	 * lies within [ContactFrame - WindowFrames, ContactFrame + LateFrames]. A success clears the lockout. The contact is marked
	 * handled either way. ContactFrame INDEX_NONE evaluates no contact.
	 */
	static FGSTN_TechEvaluation EvaluateTech(const FGSTN_TechState& State, TConstArrayView<int32> PressFrames, int32 ContactFrame, int32 HitstopEndFrame, bool bTumble, const FGSTN_TechRules& Rules);

	// ---- Landing ----

	/**
	 * The mini ground bounce (m6 6, owner revision): GroundBounceSpeed along Up, tilted toward the stick by GroundBounceSteerDeg times the
	 * stick magnitude (clamped to 1). The stick is read at the contact frame, so the result is deterministic. A zero stick bounces straight up.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static FVector GroundBounceVelocity(const FGSTN_GroundBounceRules& Rules, const FVector& Up, const FVector& StickOnPlane);
};
