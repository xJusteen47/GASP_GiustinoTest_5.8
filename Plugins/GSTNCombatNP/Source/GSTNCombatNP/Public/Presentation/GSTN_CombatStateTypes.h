// Combat simulation (GAS-NPP fork): the combat state and events a Blueprint reads (bp-exposure P1, owner 1A 2A 2026-11-16).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Templates/SubclassOf.h"
#include "GSTN_CombatStateTypes.generated.h"

class AActor;
class UGSTN_GA_BaseCombat;
struct FGSTN_CombatSyncCore;
struct FGSTN_SimEvent;
struct FGSTN_VictimHit;

/** What the combatant is doing, as the combat state says it (the Blueprint face of the simulation's action state) */
UENUM(BlueprintType)
enum class EGSTN_CombatStatus : uint8
{
	/** Free: no move, no reaction */
	Actionable,
	/** A combat move (timeline) runs */
	InMove,
	/** Stunned by a hit, on the ground or in a light launch */
	Hitstun,
	/** Launched hard enough to tumble */
	Tumble,
	/** Pinned to a surface by a splat */
	Splat,
	/** Knocked out; stays so until respawned */
	KnockedOut
};

/** Where the combatant is in the air, as the jump state of the combat model says it */
UENUM(BlueprintType)
enum class EGSTN_CombatAirState : uint8
{
	Grounded,
	/** Rising after a jump of its own */
	Rising,
	/** Coming down, from a jump or off a ledge */
	Falling,
	/** Rising because something else launched it (a trampoline, a launcher) */
	Launched
};

/**
 * One read of a combatant's combat state (Get Combat State). A PRESENTATION read: the latest state this machine shows (the predicted state on
 * the owning client, the server's own on the server, the interpolated one on everybody else). Right for UI, animation, cameras and cosmetics;
 * an ability that decides inside the simulation reads the simulation queries of the combat component instead (Get Combat Clock, Is In Timeline...).
 * Frames are simulation frames (62.5 Hz). Invalid (bValid false) before the combatant's first simulated frame.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_CombatStateView
{
	GENERATED_BODY()

	/** False when the actor has no combat component or no state yet: every other field is its default */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State")
	bool bValid = false;

	/** The simulation frame the state is the end of */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State")
	int32 Frame = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Combat State")
	EGSTN_CombatStatus Status = EGSTN_CombatStatus::Actionable;

	// ---- Hit reaction

	/** Frames of hitstun left (0 when none). Counts down in simulation frames; a hitstop on top of it ends later by its length */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Hit Reaction")
	int32 HitstunFramesLeft = 0;

	/** Frames of hitstop (the freeze of a hit, on the attacker and on the victim) left, 0 when none */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Hit Reaction")
	int32 HitstopFramesLeft = 0;

	/** In a hit reaction that locks the controls: hitstun, tumble or splat */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Hit Reaction")
	bool bInHitstun = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Hit Reaction")
	bool bInTumble = false;

	/** Tumbling, but past the hitstun: the combatant may act (air recovery) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Hit Reaction")
	bool bTumbleActionable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Hit Reaction")
	bool bInHitstop = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Hit Reaction")
	bool bKnockedOut = false;

	/**
	 * Hard bounces of the current reaction: off floors, and off walls and ceilings together (combat rules GroundImpact / WallImpact). They
	 * count up within one launch and start again from 0 on a new staggering hit; a rise is a bounce on that frame (presentation effects)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Hit Reaction")
	int32 GroundBounceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Hit Reaction")
	int32 WallBounceCount = 0;

	/** The last wall or ceiling contact the simulation decided in this reaction (bounce or bump): its frame (INDEX_NONE = none) and its face */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Hit Reaction")
	int32 SurfaceImpactFrame = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Hit Reaction")
	bool bSurfaceImpactCeiling = false;

	// ---- Current move

	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Move")
	bool bInMove = false;

	/** The ability class of the running move, None outside a move */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Move")
	TSubclassOf<UGSTN_GA_BaseCombat> MoveAbility;

	/** The move's timeline frame (0 = its first frame; it stands still in hitstop), INDEX_NONE outside a move */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Move")
	int32 MoveFrame = INDEX_NONE;

	/** The move's length and first actionable frame from its frame data (0 when unknown) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Move")
	int32 MoveTotalFrames = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Move")
	int32 MoveFirstActionableFrame = 0;

	/** The move ended into a ricochet recoil (it bounced off armour) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Move")
	bool bInRecoil = false;

	/** The simulation frame the running move started on: two runs of one move have different start frames. INDEX_NONE outside a move */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Move")
	int32 MoveStartFrame = INDEX_NONE;

	// ---- Dodge landing

	/**
	 * Frame of a dodge landing's ending lag, counted from 1 (the landing frame is 1), 0 outside it (FGSTN_DodgeLandingState). The lag is the belly
	 * part and then the kit's get-up roll (owner 2026-11-25): a landing clip, or the greybox capsule, reads where it is from this and DodgeEndLagFrames
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Dodge")
	int32 DodgeEndLagFrame = 0;

	/** Length of that ending lag in frames (a roll-out command cuts it short: it is then the frames it lasted), 0 outside it */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Dodge")
	int32 DodgeEndLagFrames = 0;

	// ---- Air and ground

	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Air")
	EGSTN_CombatAirState AirState = EGSTN_CombatAirState::Grounded;

	/** Not standing on anything (from the movement component when the pawn has one, else from AirState) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Air")
	bool bAirborne = false;

	/** Frames left in which a ground jump is still allowed after walking off a ledge (coyote time), 0 when none */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Air")
	int32 CoyoteFramesLeft = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Air")
	int32 AirJumpsUsed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Air")
	bool bFastFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Air")
	bool bGliding = false;

	// ---- Run

	/** False when the kit has no run stages: RunStage is then 0 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Run")
	bool bHasRunStages = false;

	/** 0-based run stage (Mario run: 0 walk, 1 run, 2 sprint) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat State|Run")
	int32 RunStage = 0;

	/** The registry index of the running move: with MoveStartFrame it identifies one run of one move. Internal; INDEX_NONE outside a move */
	int32 MoveEntry = INDEX_NONE;

	/**
	 * The parts of the view that come from the combat core alone (status, reactions, move frame, jump state). The move's class and frame data
	 * and the movement facts (airborne from Mover, the run stage) are filled by the caller, which has the tables and the movement component
	 */
	static FGSTN_CombatStateView FromCore(const FGSTN_CombatSyncCore& Core);
};

/** A hit, for the Blueprint events On Hit Received and On Hit Landed and for Get Last Hit Received / Landed */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_CombatHitEventData
{
	GENERATED_BODY()

	/** Who hit (None when the attacker has no actor, e.g. a minion or a detached projectile whose owner is gone) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	TObjectPtr<AActor> Attacker = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	TObjectPtr<AActor> Victim = nullptr;

	/** The simulation frame the victim took the hit on */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	int32 Frame = INDEX_NONE;

	/** The hitbox's damage (the same number as the Combat.Event.HitReceived event's magnitude) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	float Damage = 0.f;

	/** The victim's damage percent after the hit (percent damage modes; 0 otherwise) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	float DamagePercentAfter = 0.f;

	/** The victim's health fraction after the hit (health damage modes; 1 otherwise) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	float HealthFractionAfter = 1.f;

	/** The hit made the victim react (stagger, push back or launch); false for a hit that only did damage */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	bool bStaggered = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	EGSTN_HitReaction Reaction = EGSTN_HitReaction::StaggerInPlace;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	bool bTumble = false;

	/** The attack bounced off (armour, a ricochet band): no reaction and no On Hit Received, only On Hit Landed */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	bool bRicochet = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	int32 HitstunFrames = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	int32 HitstopFrames = 0;

	/** The launch velocity the hit chose (cm/s, world), zero for no launch */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	FVector LaunchVelocity = FVector::ZeroVector;

	/** The launch angle above the horizontal, in degrees (90 = straight up, negative = spiked down); 0 without a launch */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	float LaunchAngle = 0.f;

	/** The command and move tags of the attack, the hit type, the relationship and the reaction tags (what the matching Combat.Event carries) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Hit")
	FGameplayTagContainer Tags;

	/**
	 * Built from the victim step's hit (pure, on every machine; only the authority's ever reaches a Blueprint). EventTags: the tags of the hit's
	 * Combat.Event.HitLanded outbox event, so the two never disagree
	 */
	static FGSTN_CombatHitEventData FromVictimHit(const FGSTN_VictimHit& Hit, int32 Frame, AActor* Attacker, AActor* Victim, const FGameplayTagContainer& EventTags);
};

/** A knock out, for On Knocked Out */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_CombatKOEventData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat KO")
	TObjectPtr<AActor> Victim = nullptr;

	/** Who scored it: the attacker of the KO hit; None for a KO by a zone (a blast zone, a pit) */
	UPROPERTY(BlueprintReadOnly, Category = "Combat KO")
	TObjectPtr<AActor> KnockedOutBy = nullptr;

	/** Combat.KOReason.Health or Combat.KOReason.Zone */
	UPROPERTY(BlueprintReadOnly, Category = "Combat KO")
	FGameplayTag Reason;

	UPROPERTY(BlueprintReadOnly, Category = "Combat KO")
	int32 Frame = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Combat KO")
	FGameplayTagContainer Tags;

	/** From the Combat.Event.KO outbox event. False (Out untouched) for another event or for the credit partner's copy */
	static bool FromSimEvent(const FGSTN_SimEvent& Event, FGSTN_CombatKOEventData& Out);
};

/** A change of the presented combat state, found by FGSTN_CombatStateWatcher */
enum class EGSTN_CombatStateChange : uint8
{
	MoveStarted,
	MoveEnded,
	HitstunStarted,
	HitstunEnded,
	TumbleStarted,
	TumbleEnded,
	Landed,
	LeftGround,
	RunStageChanged
};

/**
 * Finds the changes between the combat states a machine presents, frame after frame, for the state events (On Landed, On Tumble Started...).
 * It compares PRESENTED states only (one call per finalized frame, never from inside the simulation), so a re-simulation that ends in the same
 * state fires nothing and nothing fires twice for one change. A correction that changes the presented state does show: a predicted landing
 * the server refuses can give Landed then Left Ground a frame later. The first call only primes. Pure; tested without a world.
 */
class GSTNCOMBATNP_API FGSTN_CombatStateWatcher
{
public:
	/** Compares Now with the previous call's state and appends the changes, in a fixed order (ends before starts) */
	void Update(const FGSTN_CombatStateView& Now, TArray<EGSTN_CombatStateChange>& OutChanges);

	/** The state of the previous call (what On Run Stage Changed reports as the old stage) */
	const FGSTN_CombatStateView& GetPrevious() const { return Previous; }

	/** Simulation frames since the presented state last landed, INDEX_NONE while airborne or before the first landing seen */
	int32 GetFramesSinceLanded(int32 Frame) const;

	void Reset();

private:
	FGSTN_CombatStateView Previous;
	bool bPrimed = false;
	int32 LandedFrame = INDEX_NONE;
};
