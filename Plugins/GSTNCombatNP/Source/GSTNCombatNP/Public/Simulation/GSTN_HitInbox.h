// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitData.h"

/**
 * A hit on its way to a victim (combat-m4-hits 7): written by the attacker's combat tick at frame F, consumed by the victim's
 * combat tick at F + 1, so the reaction is always one frame after the hit and two fighters that hit each other on the same frame
 * both land, whatever their NP ids. Plain local data: it is not part of the sync state, because the attacker's re-simulation
 * writes it again after a rollback.
 */
struct FGSTN_HitInboxEntry
{
	/** The simulation frame the attacker's tick resolved the overlap in */
	int32 Frame = INDEX_NONE;

	/** The NP proxy id of the attacker */
	int32 AttackerId = INDEX_NONE;

	/** The hit data with the relationship variant already applied: the victim never re-derives it */
	FGSTN_HitData Hit;

	FVector HitLocation = FVector::ZeroVector;

	/** The velocity of the hitbox at the contact, for the launch direction and the impact */
	FVector HitboxVelocity = FVector::ZeroVector;

	/** The attacker's facing yaw in degrees at the hit */
	float AttackerYaw = 0.f;

	/** Index of the attacker-to-victim relationship in the rules' table (Combat.Relationship.*), never the tag */
	uint8 Relationship = 0;

	/** Which hurtbox the overlap picked (the rule of combat-m4-hits 3b item 2); the victim never re-derives it */
	uint8 HurtboxIndex = 0;

	/** The attacker's team id (255 = none), for the relationship rules of a pipeline that resolves it itself */
	uint8 AttackerTeam = 255;

	/** Which hitbox of the attacker's timeline made the hit: the second key of the drain order */
	uint8 AttackerHitboxIndex = 0;

	/** EGSTN_HurtState of the hurtbox as the attacker saw it: Invincible means the hit registered on the attacker but the victim takes no damage or reaction */
	uint8 HurtState = 0;

	/** Set when the attacker's outcome was a ricochet: the victim still gets the entry for its cue and event, nothing else */
	bool bRicochet = false;
};

/**
 * The inbox of one victim (an inbox kind of the combat inboxes: the typed entry is the "message", the drain order the
 * deterministic processing order of modularity-and-tags 5b). The victim's component owns it. The container knows nothing about
 * the machine: whether an entry is written at all is decided by UGSTN_CombatSimComponent::PostHit, which refuses where the
 * victim instance does not tick.
 */
class GSTNCOMBATNP_API FGSTN_HitInbox
{
public:
	/** Entries kept at most. A frame holds one per attacker hitbox that overlaps; more than this is a content error and the extra is dropped */
	static constexpr int32 MaxEntries = 64;

	/** @return false when the inbox is full and the entry was dropped */
	bool Add(const FGSTN_HitInboxEntry& Entry);

	/**
	 * Moves the entries stamped before Frame (the frame the victim's tick is on) into Out, ordered by (AttackerId, AttackerHitboxIndex,
	 * HurtboxIndex), and removes them. Out is appended to, so a caller drains into scratch it clears itself
	 */
	void Drain(int32 Frame, TArray<FGSTN_HitInboxEntry>& Out);

	/**
	 * Rollback: drops the entries stamped at or after FirstResimulatedFrame, the first frame the re-simulation runs. The attackers'
	 * re-simulation writes them again; the earlier ones were already delivered to a state that was kept.
	 */
	void TrimFrom(int32 FirstResimulatedFrame);

	int32 Num() const { return Entries.Num(); }
	bool IsEmpty() const { return Entries.IsEmpty(); }
	void Reset() { Entries.Reset(); }
	const TArray<FGSTN_HitInboxEntry>& GetEntries() const { return Entries; }

private:
	TArray<FGSTN_HitInboxEntry> Entries;
};
