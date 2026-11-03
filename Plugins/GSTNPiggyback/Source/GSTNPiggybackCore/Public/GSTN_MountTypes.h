// Piggyback, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_PairMoveTypes.h"
#include "Rules/GSTN_CombatRules.h"
#include "Rules/GSTN_KitExtension.h"
#include "GSTN_MountTypes.generated.h"

/**
 * Why a mount request is accepted or refused (combat-m7-piggyback.md 3). One verdict per request, from UGSTN_MountUtils::EvaluateEligibility,
 * the single rule the rider's prediction and the carrier's resolution both run, so both machines reach the same verdict from the same inputs.
 * The order of the checks is the order of EvaluateEligibility's doc comment, not the order of this enum (which is the spec's, kept stable
 * because the verdict travels as a byte in the refusal event's payload).
 */
UENUM(BlueprintType)
enum class EGSTN_MountVerdict : uint8
{
	Accept = 0,
	/** The carrier's kit cannot be mounted (no mount kit data, or bCanBeMounted off) */
	NotAllowedKit = 1,
	/** The carrier or the rider is already in a Body link (one rider per carrier, a rider cannot carry) */
	AlreadyLinked = 2,
	/** The carrier is in hitstun */
	CarrierHitstun = 3,
	/** The carrier holds Combat.Lock.Link or is committed to a move that refuses links */
	CarrierBusyLock = 4,
	/** The rider weighs more than the carrier can carry (equal is allowed) */
	WeightTooHigh = 5,
	/** The rider's kit cannot ride (no mount kit data, or bCanRide off) */
	RiderIneligible = 6,
	/** The rider is farther from the carrier than the carrier's mount range (equal is allowed) */
	OutOfRange = 7,
	/** The rider's remount cooldown has not ended */
	Cooldown = 8
};

/**
 * Where a rider is put down when its mount ends without a launch (combat-m7-piggyback.md 6.4). Each mode is an ordered candidate list of
 * UGSTN_MountUtils::ExitCandidates, fed to GSTNLink's exit placer, which keeps the first free one. A new mode is a new entry plus its case
 * in ExitCandidates (pure, with a test).
 */
UENUM(BlueprintType)
enum class EGSTN_ExitMode : uint8
{
	/** Behind the carrier first, then its left, right and front */
	BehindCarrier,
	/** The carrier's right first, then its left, back and front */
	SideOfCarrier,
	/** The anchor itself first (where the rider was shown), then as BehindCarrier */
	AtAnchor
};

/**
 * The carry penalties of a carrier at ratio 1 (rider weight = the carrier's MaxCarryWeight), scaled linearly by the quantised ratio
 * (UGSTN_MountUtils::ScaleCarryTuning). Data only: the carry load effect of the Runtime module applies them.
 */
USTRUCT(BlueprintType)
struct GSTNPIGGYBACKCORE_API FGSTN_CarryTuning
{
	GENERATED_BODY()

	/** Fraction of the ground and air speed lost at ratio 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpeedPenalty = 0.3f;

	/** Fraction of the acceleration lost at ratio 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AccelPenalty = 0.3f;

	/** Fraction of the jump velocity lost at ratio 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float JumpPenalty = 0.25f;

	/** Air jumps taken away at ratio 1 (scaled down and rounded toward zero below it) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carry", meta = (ClampMin = "0"))
	int32 AirJumpsRemoved = 1;
};

/** What a kit offers as a carrier (combat-m7-piggyback.md 3). Part of FGSTN_MountKitExtension */
USTRUCT(BlueprintType)
struct GSTNPIGGYBACKCORE_API FGSTN_CarrierProfile
{
	GENERATED_BODY()

	/** Whether an ally may ride this kit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier")
	bool bCanBeMounted = true;

	/** Commands this kit cannot run while carrying (combat-m7-piggyback.md 7) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier", meta = (Categories = "Command"))
	FGameplayTagContainer DisallowedCommands;

	/** The carrier's socket the rider's anchor hangs from; None = the carrier's root */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier")
	FName MountSocket;

	/** The rider's anchor relative to MountSocket */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier")
	FTransform MountOffset = FTransform::Identity;

	/** Interaction range of a mount request: the rider must be at most this far from the carrier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MountMaxDistance = 150.f;

	/**
	 * The heaviest rider this kit carries (equal is allowed), in the units of FGSTN_RiderProfile::Weight (100 = a standard fighter). Kit data in G5:
	 * a link rule reads only its request context, which carries neither the combat attribute MaxCarryWeight nor the initiator's weight, so the
	 * mount decides on the two kits' declared values, the same on every machine (m7-doc-deltas G5)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier", meta = (ClampMin = "0.0"))
	float CarryCapacity = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier")
	FGSTN_CarryTuning Carry;

	/** Where the rider is put down by a dismount, a drop or a break */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier")
	EGSTN_ExitMode DefaultExit = EGSTN_ExitMode::BehindCarrier;

	/** Distance of the exit candidates from the carrier's root, on the carrier's ground plane */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ExitDistance = 90.f;

	/** Frames after a mount ends before the same rider may mount again (about 0.5 s at 62.5 Hz) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier", meta = (ClampMin = "0"))
	int32 RemountCooldownFrames = 30;
};

/** What a kit offers as a rider (combat-m7-piggyback.md 3). Part of FGSTN_MountKitExtension */
USTRUCT(BlueprintType)
struct GSTNPIGGYBACKCORE_API FGSTN_RiderProfile
{
	GENERATED_BODY()

	/** Whether this kit may ride an ally */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Rider")
	bool bCanRide = true;

	/** The rider's weight for the mount rule and the carry ratio (100 = a standard fighter), compared with the carrier's CarryCapacity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Rider", meta = (ClampMin = "0.0"))
	float Weight = 100.f;

	/** Hand IK target socket on the carrier (optional, presentation only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Rider")
	FName RiderPoseSocketHands;

	/** Foot IK target socket on the carrier (optional, presentation only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Rider")
	FName RiderPoseSocketFeet;

	/** The whole arc the rider may face relative to the carrier, centred on the carrier's forward (360 = free, 0 = fixed). GSTNLink's YawLimitDeg */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Rider", meta = (ClampMin = "0.0", ClampMax = "360.0", ForceUnits = "deg"))
	float AnchorYawLimitDeg = 360.f;
};

/**
 * The mount data of a kit, as an entry of its extension array (FGSTN_KitExtensionList), so the kit asset never grows a property that names
 * piggyback (modularity-and-tags 5b). A kit without this entry can neither carry nor ride. The link rule of the Runtime module reads it from
 * the request side (FGSTN_LinkRequestSide::FindKitExtension).
 */
USTRUCT(BlueprintType)
struct GSTNPIGGYBACKCORE_API FGSTN_MountKitExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback")
	FGSTN_CarrierProfile Carrier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback")
	FGSTN_RiderProfile Rider;

	/**
	 * The Runtime module's loadout extension binds the interact button's mount meanings for this kit (combat-m7-piggyback.md 7): Mount (requires
	 * Interact.Target.Mount) and Dismount (requires the rider role) when the kit can ride, Drop (Interact + crouch held, requires the carrier role)
	 * when it can be mounted. False = the kit binds the commands itself
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Bindings")
	bool bAddDefaultBindings = true;

	/** The mount move; empty = the Runtime module's UGSTN_GA_Mount. A timeline ability whose hitbox names the mount link type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Bindings", meta = (MetaClass = "/Script/GameplayAbilities.GameplayAbility"))
	FSoftClassPath MountAbility;

	/** The rider's dismount; empty = UGSTN_GA_Dismount (a Release key: the rider role's reason is Dismount) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Bindings", meta = (MetaClass = "/Script/GameplayAbilities.GameplayAbility"))
	FSoftClassPath DismountAbility;

	/** The carrier's drop; empty = UGSTN_GA_Drop (a Release key: the carrier role's reason is Drop) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Bindings", meta = (MetaClass = "/Script/GameplayAbilities.GameplayAbility"))
	FSoftClassPath DropAbility;

	/**
	 * The pair moves this kit can play (a UGSTN_PairMoveSet of the Runtime module, by path so Core names no Runtime class). The loadout
	 * extension adds them to the mount type's pair-move registry when the kit is compiled; the kit still binds each initiator move itself
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Pair Moves", meta = (AllowedClasses = "/Script/GSTNPiggybackRuntime.GSTN_PairMoveSet"))
	FSoftObjectPath PairMoves;
};

/**
 * The mount rules of a match, as an entry of the combat rules asset's extension array (UGSTN_CombatRules::RulesExtensions), chosen per game
 * mode like the rest of the rules (combat-m7-piggyback.md 3). Absent = every mount is refused, so a game mode opts in by data.
 */
USTRUCT(BlueprintType)
struct GSTNPIGGYBACKCORE_API FGSTN_MountRules : public FGSTN_RulesExtension
{
	GENERATED_BODY()

	/** Only allies may mount each other (the type's team rule; a mountable enemy is another link type) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Request")
	bool bAllyOnly = true;

	/** The rider's pending phase before its own screen attaches it (optimistic accept, combat-m7-piggyback.md 6.2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Request", meta = (ClampMin = "0", ClampMax = "8"))
	int32 PendingFrames = 2;

	/** Server safety: a request with no answer after this many frames is dropped */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Request", meta = (ClampMin = "1"))
	int32 PendingTimeoutFrames = 20;

	/** A hit on the carrier whose knockback speed exceeds this breaks the mount (default: the reaction tuning's tumble threshold) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Hits", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float CarrierBreakKnockback = 1800.f;

	/** A hit on the rider whose knockback speed exceeds this breaks the mount (default: twice the carrier's) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Hits", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float RiderBreakKnockback = 3600.f;

	/** Scale of the rider's hitstun from a hit below its break threshold (it flinches and takes the damage) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Hits", meta = (ClampMin = "0.0"))
	float RiderHitstunScale = 0.5f;

	/** Scale of the rider's knockback from a hit below its break threshold (0 = it is not launched) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Hits", meta = (ClampMin = "0.0"))
	float RiderKnockbackScale = 0.f;

	/** Hits on the rider coming from within this arc around the carrier's facing land on the carrier instead (0 = off) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Hits", meta = (ClampMin = "0.0", ClampMax = "360.0", ForceUnits = "deg"))
	float CarrierCoverFrontDeg = 0.f;

	/** The carrier's raised shield also blocks hits aimed at its rider (needs GSTNDefence; read by name, never linked) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Hits")
	bool bCarrierShieldCoversRider = false;

	/** The mount breaks when the carrier's transform jumps more than this in one frame (a respawn, a teleport) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Break", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxCarrierTeleportCm = 500.f;

	/**
	 * Who places a rider that leaves on its own (a dismount, or any break the rider's link marks pending): true = PREDICTED (owner decision S1,
	 * 2026-11-01; built in G6). The mount type sets GSTNLink's bInitiatorPlacesExit: the rider's link tick places the exit with the mount's
	 * placer around the carrier as the rider's view shows it (on the authority the carrier rewound to the rider's input time), the rider ends
	 * the link on the next frame by itself, and the watchdog copies that exit into its command, so the rider's client takes no correction
	 * unless the place overlaps the world. False = SERVER-GIVEN (G5): the watchdog places the rider and its client takes the exit as a
	 * correction. Exits the carrier causes (a drop) are server-given either way
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Exit")
	bool bPredictRiderExits = true;

	/** Credit of a pair move whose definition does not choose one */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Credit")
	EGSTN_PairCredit DefaultCredit = EGSTN_PairCredit::OwnerOnly;
};

/**
 * Everything EvaluateEligibility decides from, as plain values (combat-m7-piggyback.md 3). The Runtime module fills it from both entities'
 * snapshot entries of the request frame (link state, action state, weights, kit data, cooldown), so the rider's prediction and the
 * carrier's resolution read the same values. No world, no actor, no tag container: the busy and lock state is already reduced to bools.
 */
USTRUCT(BlueprintType)
struct GSTNPIGGYBACKCORE_API FGSTN_MountEligibilityInput
{
	GENERATED_BODY()

	// ---- Carrier ----

	/** The carrier's kit has mount data with bCanBeMounted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier")
	bool bCarrierCanBeMounted = false;

	/** The carrier is in a Body link (any type, any role) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier")
	bool bCarrierLinked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier")
	bool bCarrierInHitstun = false;

	/** The carrier holds Combat.Lock.Link or plays a move that refuses links */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier")
	bool bCarrierBusyOrLocked = false;

	/** The carrier's capacity (the combat attribute MaxCarryWeight) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier")
	double CarrierMaxCarryWeight = 0.0;

	/** The carrier profile's MountMaxDistance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Carrier", meta = (ForceUnits = "cm"))
	double MountMaxDistanceCm = 0.0;

	// ---- Rider ----

	/** The rider's kit has mount data with bCanRide */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Rider")
	bool bRiderCanRide = false;

	/** The rider is in a Body link (any type, any role) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Rider")
	bool bRiderLinked = false;

	/** The rider's weight (the combat attribute Weight) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Rider")
	double RiderWeight = 0.0;

	/** First frame the rider may mount again (an end-frame stamp; 0 = no cooldown) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Rider")
	int32 RiderCooldownEndFrame = 0;

	// ---- Request ----

	/** The request's simulation frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Request")
	int32 Frame = 0;

	/** Distance between the two roots at the request frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Request", meta = (ForceUnits = "cm"))
	double DistanceCm = 0.0;
};

/** A credit amount shared between the hitbox owner and the pair move's partner (UGSTN_MountUtils::SplitCredit) */
USTRUCT(BlueprintType)
struct GSTNPIGGYBACKCORE_API FGSTN_CreditSplit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Credit")
	double OwnerAmount = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piggyback|Credit")
	double PartnerAmount = 0.0;
};
