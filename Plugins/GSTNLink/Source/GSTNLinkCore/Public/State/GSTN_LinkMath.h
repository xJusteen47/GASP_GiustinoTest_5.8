// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_GrabRules.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "State/GSTN_LinkState.h"
#include "GSTN_LinkMath.generated.h"

/** A hit on one entity of a link this frame, as the break rules need it */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkBreakHit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	bool bHit = false;

	/** The hit came from the linked partner (a pummel) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	bool bFromPartner = false;

	/** The knockback the hit would have given, in cm/s, before the link absorbed it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float Knockback = 0.f;
};

/**
 * What the break rules read for one entity of a link (m6 spec 7.5), taken from the finalised state of the frame by the watchdog: values
 * only. The same record is built once for each side of a link, from that side's point of view.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkBreakInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	int32 Frame = 0;

	/** The frame the link formed on (from the link state) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	int32 LinkFrame = 0;

	/** The frame the link ends by itself (the mash end); 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	int32 EndFrame = 0;

	/** A break was decided earlier (a command, the link tick) and waits for the watchdog */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	bool bPendingBreak = false;

	/** The reason of that break; None counts as Forced */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	EGSTN_LinkBreakReason PendingReason = EGSTN_LinkBreakReason::None;

	/** The role tag of the entity this input is about, to pick its hit break rule */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (Categories = "Combat.Link.Role"))
	FGameplayTag OwnRoleTag;

	/** How far the parked entity is from where the link expects it (its socket), in cm; 0 for a type that parks nobody */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float PartnerDistanceCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	bool bOwnDead = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	bool bPartnerDead = false;

	/** The state tags the partner projects, for PartnerMustNot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	FGameplayTagContainer PartnerStateTags;

	/** The hit this entity took this frame, if any */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	FGSTN_LinkBreakHit Hit;
};

/** The break rules' verdict for one entity of a link */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkBreakDecision
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	bool bBreak = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	EGSTN_LinkBreakReason Reason = EGSTN_LinkBreakReason::None;
};

/**
 * Stateless link math (m6 spec 8, 7.5 and 7.3): the mash, the break evaluator, the parked placement and the reason tags. Doubles
 * and integers, no world, actor or component reads; the same inputs give the same result on every machine and in a resimulation.
 */
UCLASS()
class GSTNLINKCORE_API UGSTN_LinkMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---- Mash ----

	/**
	 * Frames a grab lasts at formation: clamp(BaseFrames + FramesPerDamagePercent x DamagePercent, MinFrames, MaxFrames), then scaled
	 * by the grabbed entity's resistance (1 = unchanged), rounded, at least 1. Add LinkFrame for the end frame
	 */
	UFUNCTION(BlueprintPure, Category = "Link|Mash")
	static int32 MashInitialFrames(const FGSTN_MashRule& Rule, float DamagePercent, float Resistance = 1.f);

	/**
	 * The end frame after this frame's mash input: each counted press (and, with bCountFlicks, each flick) moves it ReducePerPress
	 * earlier. It never goes below the current frame, and an end frame that is already past does not move. 0 (no mash end) stays 0
	 */
	UFUNCTION(BlueprintPure, Category = "Link|Mash")
	static int32 MashProgress(const FGSTN_MashRule& Rule, int32 EndFrame, int32 Frame, int32 Presses, int32 Flicks);

	/** The mash end was reached: the grabbed entity is free */
	UFUNCTION(BlueprintPure, Category = "Link|Mash")
	static bool MashEscaped(int32 EndFrame, int32 Frame) { return EndFrame > 0 && Frame >= EndFrame; }

	// ---- Break ----

	/**
	 * Whether the link breaks for this entity on this frame, and why. One reason wins, in this order: a pending break (its own reason,
	 * Forced if none), the entity's death, the partner's death, a forbidden partner state, a hit the role counts, distance, the
	 * mash end (Escape), the timeout. MaxFrames is the definition's; 0 = no timeout.
	 *
	 * A hit from the partner breaks the link only when the role's rule says bPartnerHitsBreak, so a pummel keeps the grab; a role with
	 * no hit rule is never broken by a hit.
	 */
	UFUNCTION(BlueprintPure, Category = "Link|Break")
	static FGSTN_LinkBreakDecision EvaluateBreak(const FGSTN_LinkBreakRules& Rules, int32 MaxFrames, const FGSTN_LinkBreakInput& Input);

	/** The tag of a generic break reason, or an invalid tag for None and for a plugin's own reason index */
	static FGameplayTag BreakReasonTag(EGSTN_LinkBreakReason Reason);

	/** The reason a generic break reason tag names, or None */
	static EGSTN_LinkBreakReason BreakReasonFromTag(const FGameplayTag& Tag);

	// ---- Parked placement ----

	/**
	 * Where a parked role's entity stands: the role's offset applied in the space of the partner's socket, which is the partner's root
	 * when the role names none. This overload takes the socket as the partner's root, for a role with no socket
	 */
	UFUNCTION(BlueprintPure, Category = "Link|Placement")
	static FTransform ParkedTransform(const FTransform& PartnerTransform, const FGSTN_LinkRoleDef& ParkedRole);

	/** As above, with the partner's baked socket transform relative to its root (deterministic: never the live mesh) */
	static FTransform ParkedTransformOnSocket(const FTransform& PartnerTransform, const FTransform& SocketRootRelative, const FGSTN_LinkRoleDef& ParkedRole);

	/**
	 * The parked placement of the entity holding RoleIndex (1 or 2), or false when that role is not the definition's parked one. It is
	 * the same derivation for either role, so a grab (role 2 parked) and a latch (role 1 parked) share it, and it depends on the
	 * partner's transform and the definition only, never on the partner's role state
	 */
	static bool TryGetParkedTransform(const UGSTN_LinkTypeDefinition& Definition, int32 RoleIndex, const FTransform& PartnerTransform,
		const FTransform& SocketRootRelative, FTransform& OutTransform);
};
