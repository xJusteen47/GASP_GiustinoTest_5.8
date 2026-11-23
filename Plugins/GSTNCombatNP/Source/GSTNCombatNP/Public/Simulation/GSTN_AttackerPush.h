// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"

struct FGSTN_StepSnapshot;

/** A hit of an attacker that came back as a Ricochet, as an attacker push extension is asked about it */
struct FGSTN_AttackerPushQuery
{
	/** The snapshot the hit was found against (the view snapshot of a fighter attacker, the fighters' snapshot of a minion) */
	const FGSTN_StepSnapshot& Snapshot;

	int32 VictimId = INDEX_NONE;

	/** The hurtbox the hit landed on: a set index, or an extra from GSTNHurtboxIndex::ExtraIndexBase */
	int32 HurtboxIndex = INDEX_NONE;

	/** The attacker's root, facing and up at the end of the previous frame (the side of a hit is the attacker's root) */
	FVector AttackerRoot = FVector::ZeroVector;
	FVector AttackerFacing = FVector::ForwardVector;
	FVector Up = FVector::UpVector;

	explicit FGSTN_AttackerPushQuery(const FGSTN_StepSnapshot& InSnapshot)
		: Snapshot(InSnapshot)
	{
	}
};

/**
 * Pushes back an attacker whose melee hit bounced off something an optional plugin owns (M11 L7d: the backpack dome pushes back whoever hits
 * it from outside). The push is data of that plugin, so the attacker side asks the registered extensions instead of the ricochet response:
 * the recoil animation stays the attacker's own, the push is added on top.
 *
 * Asked for a Ricochet hit only, with the snapshot the hit was found against, so the attacker's prediction and the authority agree. A pure
 * function of the query: no world, no component, no net mode. Today the minion attacker asks it (a fighter attacker's push is applied by the
 * plugin's own combat tick extension, which reads the same function).
 */
class GSTNCOMBATNP_API IGSTN_AttackerPushExtension
{
public:
	virtual ~IGSTN_AttackerPushExtension() = default;

	/** Feature id from GSTNFeatureIds: orders the extensions, fixed for the lifetime of the registration */
	virtual uint8 GetTypeId() const = 0;

	/** True and the push (world velocity, cm/s, fading to zero over OutFrames) when the extension owns what the hit bounced off */
	virtual bool GetPush(const FGSTN_AttackerPushQuery& Query, FVector& OutVelocity, int32& OutFrames) const = 0;
};

/** The registered attacker push extensions, in type id order. Game thread only; registered when a module starts, before any simulation tick */
class GSTNCOMBATNP_API FGSTN_AttackerPushRegistry
{
public:
	static FGSTN_AttackerPushRegistry& Get();

	/** False when one with the same type id is already registered */
	bool Register(IGSTN_AttackerPushExtension* Extension);
	void Unregister(IGSTN_AttackerPushExtension* Extension);

	/** The first extension in type id order that pushes; false when none does */
	bool GetPush(const FGSTN_AttackerPushQuery& Query, FVector& OutVelocity, int32& OutFrames) const;

	bool IsEmpty() const { return Entries.IsEmpty(); }

private:
	TArray<IGSTN_AttackerPushExtension*> Entries;
};
