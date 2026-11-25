// Links, Network Prediction half: the grab spin (owner 2026-11-25).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Data/GSTN_GrabSpinTrack.h"
#include "GSTN_LinkGrabSpinMoves.generated.h"

/**
 * The frame data of one turn of the grab spin, so a setup script retunes a Blueprint child without code (UGSTN_LinkContentLibrary::WriteGrabSpinMove).
 * DEFAULTS = the owner's recommended options of 2026-11-25 (thread "Perfezionare grab e lancio dei nemici", 3A 4A 5A 6A), numbers from Ganondorf in
 * Smash Ultimate where one exists: the ring hit is the collateral hit of his throws (5 %, BKB 30, KBG 100: SmashWiki Ganondorf (SSBU) forward and back
 * throw pages), the held partner's hit is his pummel (1.6 %, Ultimate Frame Data). Turn length, cost and radius are ours, placeholders for the playtest.
 */
USTRUCT(BlueprintType)
struct GSTNLINKNP_API FGSTN_GrabSpinMoveSpec
{
	GENERATED_BODY()

	/** One turn's length (frames). The move is actionable from its last frame, and chains into the next turn from ChainFromFrame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin", meta = (ClampMin = "4", ForceUnits = "Frames"))
	int32 TotalFrames = 24;

	/** Full turns the grabber makes over TotalFrames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float Turns = 1.f;

	/** From this frame another stick rotation chains the next turn, and the throws, the release and the put-down cancel the spin */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 ChainFromFrame = 12;

	/** The swept hit on the fighters around: a sphere in front of the grabber (where the held partner hangs) for [RingStartFrame, RingEndFrame) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 RingStartFrame = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 RingEndFrame = 22;

	/** The sphere's radius: about the held partner's body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float RingRadius = 70.f;

	/** The sphere's centre ahead of the grabber's root: the hold point (FGSTN_GrabHoldPose, 60 cm) plus half a body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ForceUnits = "cm"))
	float RingReach = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "0.0"))
	float RingDamage = 5.f;

	/** cm/s (Smash BKB 30 x the knockback unit scale 22.5, docs/notes/handovers feel-f2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "0.0"))
	float RingBaseKnockback = 675.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "0.0"))
	float RingKnockbackGrowth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "-90.0", ClampMax = "90.0", ForceUnits = "deg"))
	float RingAngleDeg = 45.f;

	/** The frame the held partner takes its hit (a pummel: damage only, no reaction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Held", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 HeldHitFrame = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Held", meta = (ClampMin = "0.0"))
	float HeldDamage = 1.6f;

	/** Frames each turn takes off the grab (FGSTN_LinkHitData::GrabTimeCostFrames): the held partner frees itself sooner, so nobody spins for ever */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Held", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 GrabTimeCostFrames = 20;
};

/**
 * The grab spin (owner 2026-11-25): holding a partner, a 360 rotation of the stick (Command.Link.Spin.Clockwise / .CounterClockwise, the GSTNInput
 * Spin condition) swings it round as a weapon. One turn per move: the grabber turns (FGSTN_GrabSpinTrack, the link tick writes the facing), a sphere
 * where the partner hangs sweeps the circle and hits the fighters around (LinkFilter ExcludeLinkedPartner), the partner takes a pummel hit that costs
 * grab time (LinkedPartnerOnly, no reaction), and from ChainFromFrame the next rotation chains another turn and the throws cancel it, so the spin
 * ends in a throw. The partner's own mash still runs: it can escape mid-spin.
 *
 * PLACEHOLDER frame data and no montage (capsule phase, owner 2026-11-24): a Blueprint child adds a montage and retunes through the spec.
 */
UCLASS(Abstract)
class GSTNLINKNP_API UGSTN_GA_LinkGrabSpin : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkGrabSpin(const FObjectInitializer& ObjectInitializer);

	/** Replaces the frames and the tracks of OutTimeline with one turn of Spec in Sense, keeps its animation fields. Shared by the constructors and the content writer */
	static void FillGrabSpinTimeline(FGSTN_CombatTimelineData& OutTimeline, const FGSTN_GrabSpinMoveSpec& Spec, EGSTN_GrabSpinSense Sense);

	/** The commands a turn cancels into from ChainFromFrame: both spins, every throw, the release and the put-down */
	static FGameplayTagContainer ChainCommands();

	/** The sense of this class (the child decides) */
	virtual EGSTN_GrabSpinSense GetSense() const { return EGSTN_GrabSpinSense::Clockwise; }
};

UCLASS()
class GSTNLINKNP_API UGSTN_GA_LinkGrabSpinClockwise : public UGSTN_GA_LinkGrabSpin
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkGrabSpinClockwise(const FObjectInitializer& ObjectInitializer);
	virtual EGSTN_GrabSpinSense GetSense() const override { return EGSTN_GrabSpinSense::Clockwise; }
};

UCLASS()
class GSTNLINKNP_API UGSTN_GA_LinkGrabSpinCounterClockwise : public UGSTN_GA_LinkGrabSpin
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkGrabSpinCounterClockwise(const FObjectInitializer& ObjectInitializer);
	virtual EGSTN_GrabSpinSense GetSense() const override { return EGSTN_GrabSpinSense::CounterClockwise; }
};
