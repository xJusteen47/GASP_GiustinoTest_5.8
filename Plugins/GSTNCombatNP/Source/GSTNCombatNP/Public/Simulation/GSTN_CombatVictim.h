// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Events/GSTN_SimEvent.h"
#include "Hits/GSTN_HitResolution.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_HitInbox.h"
#include "World/GSTN_StepSnapshot.h"

class UAbilitySystemComponent;
class UGSTN_CombatRules;
class UGSTN_HurtboxSetDefinition;

/** One entry of the drain after the rules resolved it, for the layer that applies effects and raises events (the component) */
struct FGSTN_VictimHit
{
	FGSTN_HitInboxEntry Entry;

	/** The attacker's outcome was a ricochet: only the cue and the event happen */
	bool bRicochet = false;

	/** The hit pipeline's context after the run (the view the appliers saw) and its outcome with the appliers' events and effects merged in */
	FGSTN_HitContextView View;
	FGSTN_HitOutcome Outcome;

	bool bStaggered = false;
	EGSTN_HitReaction Reaction = EGSTN_HitReaction::StaggerInPlace;

	/** A PushBack that had no ground to slide on and became a Launch */
	bool bPromoted = false;
	bool bTumble = false;
	int32 HitstunFrames = 0;
	int32 HitstopFrames = 0;

	/** The state this hit left the victim's damage in (after AdvanceVictim): the KO checks read it */
	float DamagePercentAfter = 0.f;
	float HealthFractionAfter = 1.f;
};

/** What the victim step reads. Everything is a value of the end of the previous frame or constant data */
struct FGSTN_VictimTickInput
{
	int32 Frame = INDEX_NONE;

	/** The victim's own entry of the step snapshot: transform, up, hurt states, reaction state. Null skips the whole step */
	const FGSTN_SnapshotEntry* Self = nullptr;

	/** Null runs the pipeline-less defaults: nothing staggers, no hitstop */
	const UGSTN_CombatRules* Rules = nullptr;

	/** The drained inbox, already in drain order */
	TConstArrayView<FGSTN_HitInboxEntry> Entries;

	/** The Mover context at the end of the previous frame (the tracker): standing on something, and in a wall mode */
	bool bGrounded = true;
	bool bOnWall = false;

	/** The victim's sync fragments, handed to the outcome appliers; may be null */
	FGSTN_SyncFragmentArray* Fragments = nullptr;
};

/** Why a combatant was knocked out; None for a frame that knocked nobody out */
enum class EGSTN_KOCause : uint8
{
	None,
	/** The health ran out, or the damage reached the cap of the rules */
	Health,
	/** The combatant's position was inside a KO zone */
	Zone
};

/** What the victim step did, besides the sync state it wrote */
struct FGSTN_VictimResult
{
	TArray<FGSTN_VictimHit, TInlineAllocator<4>> Hits;

	/** This frame's step knocked the combatant out (the state is KO now); for Health, KOHitIndex is the hit that did it */
	EGSTN_KOCause KO = EGSTN_KOCause::None;
	int32 KOHitIndex = INDEX_NONE;

	/** The victim's freeze, in frames from this frame; 0 for none (no hitstop, or a window that already covered it) */
	int32 FreezeFrames = 0;

	/** The freeze ends at rest: the last staggered hit was a StaggerInPlace */
	bool bFreezeZeroVelocity = false;

	/** The current move was cancelled by a stagger (a CancelTimeline request was written) */
	bool bCancelledMove = false;
};

/** The move a pending reaction turns into when the hitstop ends */
struct FGSTN_ReactionMove
{
	enum class EKind : uint8 { None, Launch, PushBack };

	EKind Kind = EKind::None;
	FVector Velocity = FVector::ZeroVector;
	float DecayPerFrame = 0.f;
	float GravityScale = 1.f;

	/** The lifetime of the move: the frames left of the hitstun */
	int32 Frames = 0;
};

struct FGSTN_ReactionStepInput
{
	int32 Frame = INDEX_NONE;
	const UGSTN_CombatRules* Rules = nullptr;

	/** The victim's local up and facing at the end of the previous frame */
	FVector Up = FVector::UpVector;
	FVector Facing = FVector::ForwardVector;
	bool bGrounded = true;

	/** This frame's stick (x right, y forward, -1..1) and the camera yaw it is relative to, degrees */
	FVector2D Stick = FVector2D::ZeroVector;
	double CameraYawDegrees = 0.0;

	/** Fraction of the knockback the victim resists (the KnockbackResistance attribute) */
	float KnockbackResistance = 0.f;
};

/**
 * The victim side of the combat tick (combat-m4-hits 7, M4 H9): the drained hits of the frame against the victim's snapshot entry, the state
 * they write (hitstun, windows, the pending reaction), and the move a pending reaction becomes at the end of the hitstop. Pure functions of
 * their inputs and the sync core: no world, no component, no time, no role.
 */
struct GSTNCOMBATNP_API FGSTN_CombatVictim
{
	/**
	 * The victim as the pipeline sees it at the end of the previous frame: the snapshot entry's values, with the health fraction the
	 * damage mode reads. The attacker side builds the victim of its prediction with the same function, so both sides run the rules on the
	 * same view.
	 */
	static FGSTN_HitVictimView MakeVictimView(const FGSTN_SnapshotEntry& Victim, const UGSTN_CombatRules* Rules);

	/** The weight scale the health weight curve gives a victim with this damage, 1 when no curve applies. Written into the snapshot for the attacker's machine */
	static float ComputeWeightScale(const UGSTN_CombatRules* Rules, const UGSTN_HurtboxSetDefinition* Hurtboxes, double DamagePercent, float HealthFraction);

	/**
	 * Resolves the entries in order, each on the victim the earlier ones left, and writes the result into Core: a staggered hit cancels the
	 * move and sets the hitstun state, the windows and the pending reaction; a hit below the threshold opens the hitstop window only, and a
	 * victim already in hitstun ignores it. A ricochet entry changes nothing. Out is reset first
	 */
	static void Resolve(const FGSTN_VictimTickInput& In, FGSTN_CombatSyncCore& Core, FGSTN_VictimResult& Out);

	/**
	 * The hitstop of the pending reaction ended: returns the move it becomes (DI from the stick, the knockback resistance applied) and clears
	 * the pending reaction. Then hitstun end: Actionable, or Tumble until the victim lands. Does nothing for a combatant that is not in hitstun
	 */
	static FGSTN_ReactionMove StepReaction(const FGSTN_ReactionStepInput& In, FGSTN_CombatSyncCore& Core);

	/** The combat state locks commands: a victim in hitstun, tumble or a surface impact reaction cannot act until it is Actionable again */
	static bool IsActionLocked(const FGSTN_CombatSyncCore& Core)
	{
		return Core.ActionState == EGSTN_ActionState::Hitstun || Core.ActionState == EGSTN_ActionState::Tumble || Core.ActionState == EGSTN_ActionState::Splat
			|| Core.ActionState == EGSTN_ActionState::KO;
	}

	/** The part of the lock that is a hit reaction (not a KO): the stick feeds DI and nothing else, and the locomotion is held (the movement lock tag is projected from it) */
	static bool IsInReaction(const FGSTN_CombatSyncCore& Core)
	{
		return Core.ActionState == EGSTN_ActionState::Hitstun || Core.ActionState == EGSTN_ActionState::Tumble || Core.ActionState == EGSTN_ActionState::Splat;
	}

	/**
	 * A combatant found inside a KO zone at the end of the previous frame is knocked out: the running move is cancelled and the state is the
	 * terminal KO. Does nothing to one that is out already. @return true when this call knocked it out; bCancelledMove says a move was cancelled
	 */
	static bool ApplyZoneKO(FGSTN_CombatSyncCore& Core, int32 Frame, bool bInZone, bool& bCancelledMove);

	/** The cap a hit left the victim at: Health reaches zero, PercentCapped reaches MaxHealth. The Percent mode has no cap and never knocks out */
	static bool ReachedCap(const UGSTN_CombatRules* Rules, const FGSTN_VictimHit& Hit);

	/**
	 * The outbox events of one resolved hit (combat.md 5.6): HitLanded (for the attacker's listeners) and HitReceived (the victim's), the
	 * second only when the hit was not a ricochet. Both carry the hit type, the relationship, bStaggered, the tumble and the reaction as tags,
	 * the damage as the magnitude, and MoveTags (the attacker's command and move) when the caller has them. Appended unconditionally: the
	 * host's FinalizeFrame publishes them on the authority, once
	 */
	static void AppendHitEvents(const FGSTN_VictimHit& Hit, int32 Frame, AActor* Attacker, AActor* Victim, const FGameplayTagContainer& MoveTags, FGSTN_SimEventOutbox& Outbox);

	/**
	 * Projects the combat state onto the ability system for the frame (the pre-tick): Combat.State.Hitstun (hitstun or splat), Tumble and
	 * Hitstop are set to what the state says, and Combat.Lock.Reaction is held while the state locks the combatant (a hit reaction or a KO),
	 * which stops the locomotion through the stats movement modifier; the stick feeds DI only. Idempotent and memberless: a function of the
	 * state, so a re-simulated frame ends with the same tags. Lock.Movement is never touched, a lock an ability or an effect grants stays
	 */
	static void ProjectStateTags(UAbilitySystemComponent& ASC, const FGSTN_CombatSyncCore& Core, int32 Frame);

	/** The KO event: the instigator is the attacker for a Health KO (the victim itself for a zone), the target the combatant; KOReason as a tag */
	static void AppendKOEvent(EGSTN_KOCause Cause, int32 Frame, AActor* Attacker, AActor* Victim, FGSTN_SimEventOutbox& Outbox);

	/** The stick on the plane normal to Up: forward is the camera's, turned about Up (never world Z) */
	static FVector StickToPlane(const FVector2D& Stick, double CameraYawDegrees, const FVector& Up);
};
