// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitData.h"
#include "GSTN_HitInbox.generated.h"

struct FGSTN_InboxEntry;

/**
 * A hit on its way to a victim (combat-m4-hits 7): written by the attacker's combat tick at frame F, consumed by the victim's
 * combat tick at F + 1, so the reaction is always one frame after the hit and two fighters that hit each other on the same frame
 * both land, whatever their NP ids. Plain local data: it is not part of the sync state, because the attacker's re-simulation
 * writes it again after a rollback.
 *
 * It is the payload of the inbox kind GSTNInboxKinds::Hit (FGSTN_CombatInbox): a USTRUCT with no reflected property only so that an
 * instanced struct can hold it, and so its fields and their meaning are exactly what they were before the inbox became open.
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_HitInboxEntry
{
	GENERATED_BODY()

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

	/** The attacker's running timeline (registry index) and the binding that started it (table index), -1 for none: for the tags of the outbox events only, never read by the simulation */
	int16 AttackerTimeline = -1;
	int16 AttackerBinding = -1;

	/** Set when the attacker's outcome was a ricochet: the victim still gets the entry for its cue and event, nothing else */
	bool bRicochet = false;

	/** The entry as a message of kind Hit, stamped with this entry's Frame and AttackerId (the first keys of the drain order) */
	FGSTN_InboxEntry ToInboxEntry() const;

	/** The hit an inbox entry carries, null for another kind or a payload of another type */
	static const FGSTN_HitInboxEntry* FromInboxEntry(const FGSTN_InboxEntry& Entry);
};

template <>
struct TStructOpsTypeTraits<FGSTN_HitInboxEntry> : public TStructOpsTypeTraitsBase2<FGSTN_HitInboxEntry>
{
	enum
	{
		// No reflected property to copy member by member: the instanced struct copies it with the C++ copy
		WithCopy = true
	};
};
