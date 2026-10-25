// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Hits/GSTN_HitShape.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Timeline/GSTN_TimelinePlayer.h"

class UGSTN_CombatRules;
struct FGSTN_CombatTimelineData;
struct FGSTN_StepSnapshot;

namespace GSTNHitRelationship
{
	/** Index of a Combat.Relationship.* tag in the inbox entry: 0 Self, 1 Ally, 2 Enemy, 3 Neutral, 255 for a tag a custom resolver made up */
	GSTNCOMBATNP_API uint8 ToIndex(const FGameplayTag& Relationship);

	/** The tag of an index ToIndex wrote; the empty tag for 255 (a tag a custom resolver made up: the pipeline resolves it again) */
	GSTNCOMBATNP_API FGameplayTag FromIndex(uint8 Index);
}

/** World shape of an active hitbox at a frame, for the debug draw and the harness trace */
struct FGSTN_AttackerDebugShape
{
	int32 Frame = INDEX_NONE;
	int32 HitboxIndex = 0;
	uint8 Space = 0;
	FGSTN_HitShape Shape;
	FTransform World;
};

/** What the attacker side of one combat tick reads. Everything in it is a value of the end of the previous frame or constant data */
struct FGSTN_AttackerTickInput
{
	int32 Frame = INDEX_NONE;

	/** The NP proxy id of the attacker; INDEX_NONE (not registered yet) skips the whole step */
	int32 AttackerId = INDEX_NONE;
	FGenericTeamId AttackerTeam;

	/** The timeline the state says is running, null when none */
	const FGSTN_CombatTimelineData* Timeline = nullptr;

	/** The timeline frame this tick evaluates: the frame minus the start minus the paused frames */
	int32 TimelineFrame = 0;

	const FGSTN_StepSnapshot* Snapshot = nullptr;

	/** Null runs the pipeline-less defaults: Enemy and Neutral are hittable, no hitstop */
	const UGSTN_CombatRules* Rules = nullptr;

	/** The attacker's actor transform at the end of the previous frame (the Mover sim state tracker) and its up direction */
	FTransform Root;
	FVector Up = FVector::UpVector;

	/** The groups the "before candidates" extension point masks this frame */
	FGSTN_HitGroupMask GroupMask;

	/** The length of a simulation step, for the velocity of the hitbox */
	float StepSeconds = 1.f / 60.f;

	/** Presentation only: when set, the world shape of every active hitbox is appended. Never read back by the simulation */
	TArray<FGSTN_AttackerDebugShape>* DebugShapes = nullptr;
};

/** One hit the attacker side made this frame, after its own consequences were written into the state */
struct FGSTN_AttackerHit
{
	int32 VictimId = INDEX_NONE;
	uint8 HitGroup = 0;

	/** What goes to the victim's inbox (also on a ricochet) */
	FGSTN_HitInboxEntry Entry;

	bool bRicochet = false;

	/** The attacker's own hitstop for this hit, 0 for none. The state carries the longest of the frame */
	int32 AttackerHitstopFrames = 0;

	float MeterGain = 0.f;

	/** The ring was full and the oldest target was replaced by this one */
	bool bReplacedTarget = false;

	/**
	 * What the attacker predicted the victim would do, for the presentation of its own machine (M4 H10): the knockback of the pipeline
	 * (tier, hitstun, launch velocity before the victim's DI) and the hit's cue and place. Never read by the simulation
	 */
	bool bHasKnockback = false;
	FGSTN_KnockbackResult Knockback;
	FGameplayTag CueTag;
	FVector HitLocation = FVector::ZeroVector;
};

/**
 * The attacker side of the combat tick (combat-m4-hits 6): the open hitboxes of the running timeline against the hurtboxes of the
 * step snapshot, the once-per-group rule, and the attacker's own consequences of each new hit (confirm flag, hitstop window and freeze,
 * meter request). Pure function of its inputs and the sync core: no world, no component, no time, no role. Candidates run in entity id order and
 * hitboxes in timeline order, so the result does not depend on which attacker ticks first; the victims sort again when they drain.
 */
struct GSTNCOMBATNP_API FGSTN_CombatAttacker
{
	/**
	 * Runs the step on Core (the state this frame writes). Appends the hits to OutHits (reset first). Scratch is the caller's reusable
	 * evaluation buffer. Does nothing on a hitstop frame: the caller must not call it then (a timeline that stands still hits nothing new)
	 */
	static void Tick(const FGSTN_AttackerTickInput& In, FGSTN_CombatSyncCore& Core, FGSTN_TimelineFrameResult& Scratch, TArray<FGSTN_AttackerHit>& OutHits);

	/** The bit of a hit group in FGSTN_HitSyncState::HitConfirmFlags: groups past 7 share bits (the flag only gates cancel routes) */
	static uint8 GetConfirmBit(uint8 HitGroup) { return static_cast<uint8>(1u << (HitGroup & 7)); }
};
