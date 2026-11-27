// Links, Network Prediction half: the grab spin (owner 2026-11-25).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Data/GSTN_GrabSpinTrack.h"
#include "GSTN_LinkGrabSpinMoves.generated.h"

/**
 * The frame data of one grab spin move (one stick rotation), so a setup script retunes a Blueprint child without code
 * (UGSTN_LinkContentLibrary::WriteGrabSpinMove). DEFAULTS = the owner's answers of 2026-11-25 (thread "Perfezionare grab e lancio dei nemici"):
 *  3: one stick rotation = TWO turns, then a settle for the eye only (actionable, cancellable tail);
 *  4: 8 % to the fighters around and 3 % to the held one ("per il personaggio 8 e 3"; per enemy later), and a wall slam that hurts the held one
 *     with some hitstop;
 *  5: each move costs grab time, tunable;
 *  6: the normal throws stay, they turn toward the stick (FGSTN_GrabSpinMoveSpec is not involved: the throws' own steer, setup_grab_ganon_playtest.py).
 * The ring's knockback is the collateral hit of Ganondorf's throws (BKB 30, KBG 100: SmashWiki Ganondorf (SSBU)). Turn length, cost, radius and the
 * wall slam are ours, placeholders for the playtest.
 */
USTRUCT(BlueprintType)
struct GSTNLINKNP_API FGSTN_GrabSpinMoveSpec
{
	GENERATED_BODY()

	/** Frames of the turns. The move is actionable from here (FirstActionableFrame) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin", meta = (ClampMin = "4", ForceUnits = "Frames"))
	int32 TurnFrames = 48;

	/** Full turns over TurnFrames (owner 2026-11-25: two per stick rotation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float Turns = 2.f;

	/** The settle after the turns: the cancellable tail (anything, stick included, ends it). Presentation only */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 SettleFrames = 12;

	/** How far the facing swings past the last turn during the settle and back (capsule stand-in for the settle animation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin", meta = (ClampMin = "0.0", ClampMax = "90.0", ForceUnits = "deg"))
	float SettleOvershootDeg = 15.f;

	/** From this frame the throws, the release and the put-down cancel the spin */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 ThrowFromFrame = 12;

	/** From this frame another stick rotation chains the next spin (inside the second turn, so the spin can go on without a stop) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 ChainFromFrame = 36;

	/** The swept hit on the fighters around: a sphere in front of the grabber (where the held partner hangs) for [RingStartFrame, RingEndFrame) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 RingStartFrame = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 RingEndFrame = 46;

	/** The sphere's radius: about the held partner's body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float RingRadius = 70.f;

	/** The sphere's centre ahead of the grabber's root: the hold point (FGSTN_GrabHoldPose, 60 cm) plus half a body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ForceUnits = "cm"))
	float RingReach = 90.f;

	/** Once per fighter per move (one hit group), whatever the number of turns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "0.0"))
	float RingDamage = 8.f;

	/** cm/s (Smash BKB 30 x the knockback unit scale 22.5, docs/notes/handovers feel-f2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "0.0"))
	float RingBaseKnockback = 675.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "0.0"))
	float RingKnockbackGrowth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Ring", meta = (ClampMin = "-90.0", ClampMax = "90.0", ForceUnits = "deg"))
	float RingAngleDeg = 45.f;

	/** The frame the held partner takes its hit (a pummel: damage only, no reaction), once per move */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Held", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 HeldHitFrame = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Held", meta = (ClampMin = "0.0"))
	float HeldDamage = 3.f;

	/** Frames each move takes off the grab (FGSTN_LinkHitData::GrabTimeCostFrames): the held partner frees itself sooner, so nobody spins for ever */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Held", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 GrabTimeCostFrames = 20;

	/** The wall slam: once per move, on the first frame of the turns the probe toward the hold point touches static world geometry. 0 = no wall slam */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Wall", meta = (ClampMin = "0.0"))
	float WallDamage = 4.f;

	/** Hitstop on the held partner (and the impact's weight) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Wall", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 WallHitstopFrames = 8;

	/** Hitstop on the grabber: the turn pauses (the yaw is a function of the unpaused timeline frame, so it resumes where it stopped) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Wall", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 WallAttackerHitstopFrames = 4;

	/** The probe: a sphere swept from the grabber this far along the facing (FGSTN_GrabSpinTrack::WallProbeReach) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Wall", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float WallProbeReach = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab Spin|Wall", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float WallProbeRadius = 30.f;
};

/** The hit groups of the spin's hitboxes: each victim is hit once per group and move, so the ring, the held hit and the wall slam stay apart */
namespace GSTNGrabSpinGroups
{
	inline constexpr uint8 Ring = 0;
	inline constexpr uint8 Held = 1;
	inline constexpr uint8 Wall = 2;
}

/**
 * The grab spin (owner 2026-11-25): holding a partner, a 360 rotation of the stick (Command.Link.Spin.Clockwise / .CounterClockwise, the GSTNInput
 * Spin condition) swings it round as a weapon. Two turns per move: the grabber turns (FGSTN_GrabSpinTrack, the link tick writes the facing), a sphere
 * where the partner hangs sweeps the circle and hits the fighters around (LinkFilter ExcludeLinkedPartner), the partner takes a pummel hit that costs
 * grab time (LinkedPartnerOnly, no reaction) and a wall slam hit when the probe touches a wall, the throws cancel it from ThrowFromFrame and the next
 * rotation chains another spin from ChainFromFrame, so the spin ends in a throw; after the turns a cancellable settle. The partner's escape timer
 * still runs (no mash on the playtest kit, owner 2026-11-25): it can free itself mid-spin.
 *
 * PLACEHOLDER frame data and no montage (capsule phase, owner 2026-11-24): a Blueprint child adds a montage and retunes through the spec.
 */
UCLASS(Abstract)
class GSTNLINKNP_API UGSTN_GA_LinkGrabSpin : public UGSTN_GA_BaseCombat
{
	GENERATED_BODY()

public:
	UGSTN_GA_LinkGrabSpin(const FObjectInitializer& ObjectInitializer);

	/** Replaces the frames and the tracks of OutTimeline with one spin of Spec in Sense, keeps its animation fields. Shared by the constructors and the content writer */
	static void FillGrabSpinTimeline(FGSTN_CombatTimelineData& OutTimeline, const FGSTN_GrabSpinMoveSpec& Spec, EGSTN_GrabSpinSense Sense);

	/** The commands the spin cancels into from ThrowFromFrame: every throw, the release and the put-down */
	static FGameplayTagContainer ThrowCommands();

	/** The commands it cancels into from ChainFromFrame: both spins */
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
