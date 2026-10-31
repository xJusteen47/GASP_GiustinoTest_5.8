// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_GrabRules.h"
#include "Host/GSTN_LinkHost.h"
#include "State/GSTN_LinkMath.h"
#include "State/GSTN_LinkState.h"

class IGSTN_ExitCollisionQuery;
class UGSTN_LinkTypeDefinition;

/**
 * The test double of the link host (m6 spec 7.0 item 8): it holds entities and their link states as plain structs and executes link
 * requests, break evaluation and exits SYNCHRONOUSLY, in the same call. There is no Network Prediction, no inbox and no delay, so a
 * plugin built on links (held objects, piggyback) tests its rules and its data against the real request rules, break rules and
 * placers without a world.
 *
 * It runs the same rules and math the real host does, but none of the timing: a break takes effect inside Step instead of two frames
 * later, and a formation inside RequestLink instead of one. It is for rules and data, never for latency or rollback.
 */
class GSTNLINKCORE_API FGSTN_FakeLinkHost : public IGSTN_LinkHost
{
public:
	/** One entity of the fake world */
	struct FEntity
	{
		FGSTN_EntityRef Ref;
		FGameplayTagContainer Tags;
		float EffectiveWeight = 100.f;
		float DamagePercent = 0.f;
		FTransform Transform = FTransform::Identity;
		FVector Velocity = FVector::ZeroVector;
		bool bAlive = true;
		int32 GrabImmunityEndFrame = 0;
		int32 LatchImmunityEndFrame = 0;
		FGSTN_LinkStates Links;

		/** A hit this entity takes at the next Step, set by the test and cleared by that Step */
		FGSTN_LinkBreakHit PendingHit;
	};

	/** What one Step did */
	struct FStepResult
	{
		int32 Breaks = 0;
		int32 Orphans = 0;
	};

	/** The kit rules every initiator of this host uses; a test changes them to change the kit */
	FGSTN_GrabRules GrabRules;
	FGSTN_LatchRules LatchRules;

	/** The world the exit placers ask, or null for none */
	const IGSTN_ExitCollisionQuery* ExitCollision = nullptr;

	/** Frames after a formation during which a one-sided link is not yet an orphan. Negative = the settings' value */
	int32 OrphanGraceFrames = INDEX_NONE;

	/** Adds a fighter and returns its reference (kind Fighter, ids from 0, serial 1) */
	FGSTN_EntityRef AddFighter(const FGameplayTagContainer& Tags, float EffectiveWeight, const FTransform& Transform);

	FEntity* FindEntity(const FGSTN_EntityRef& Ref);
	const FEntity* FindEntity(const FGSTN_EntityRef& Ref) const;

	/** Registers a definition and returns its link type index (1-based; the registry's table in the real host). The definition must outlive the host */
	uint8 RegisterType(const UGSTN_LinkTypeDefinition& Definition);

	void SetFrame(int32 NewFrame) { Frame = NewFrame; }
	void AdvanceFrames(int32 Frames) { Frame += Frames; }

	/**
	 * A link request, decided and executed at once: the shared checks and the definition's rules run on the target's view; on accept
	 * both link slots are written (serial, roles, frame), with the mash end on the target when bApplyMash, and LinkFormed is published.
	 * Returns the verdict
	 */
	FGSTN_LinkRuleResult RequestLink(const FGSTN_EntityRef& Initiator, const FGSTN_EntityRef& Target, uint8 LinkTypeIndex, bool bApplyMash = false);

	/** Breaks the link on a channel of an entity for a reason, at once. False when the entity holds no active link there */
	bool RequestBreak(const FGSTN_EntityRef& Entity, EGSTN_LinkChannel Channel, EGSTN_LinkBreakReason Reason);

	/**
	 * What the watchdog does each frame: evaluates the break rules of every active link from the point of view of each side and breaks
	 * the ones that fire, and calls an orphan (one side holds a link its partner does not) after the grace, breaking the dangling side
	 * as Forced. Clears the pending hits
	 */
	FStepResult Step();

	const TArray<FGSTN_LinkEventRecord>& GetEvents() const { return Events; }
	void ClearEvents() { Events.Reset(); }

	// ---- IGSTN_LinkHost ----
	virtual int32 GetFrame() const override { return Frame; }
	virtual bool GetEntityView(const FGSTN_EntityRef& Entity, FGSTN_LinkEntityView& OutView) const override;
	virtual void SetLinkState(const FGSTN_EntityRef& Entity, EGSTN_LinkChannel Channel, const FGSTN_LinkState& State) override;
	virtual const UGSTN_LinkTypeDefinition* GetLinkDefinition(uint8 LinkTypeIndex) const override;
	virtual void SetImmunity(const FGSTN_EntityRef& Entity, EGSTN_LinkImmunityKind Kind, int32 EndFrame) override;
	virtual void PlaceEntity(const FGSTN_EntityRef& Entity, const FTransform& Transform, const FVector& Velocity) override;
	virtual void PublishEvent(const FGSTN_LinkEventRecord& Event) override { Events.Add(Event); }

private:
	/** Clears both slots of a link, places the parked entity, stamps the immunity and publishes the events */
	void BreakPair(const FGSTN_EntityRef& Entity, EGSTN_LinkChannel Channel, EGSTN_LinkBreakReason Reason);

	/** Puts the definition's parked entity (if the type parks one) where the definition's exit placer says, facing the entity that stays */
	void PlaceParkedEntity(const UGSTN_LinkTypeDefinition& Definition, const FGSTN_EntityRef& Initiator, const FGSTN_EntityRef& Target);

	int32 Frame = 0;
	TArray<FEntity> Entities;
	TArray<const UGSTN_LinkTypeDefinition*> Definitions;
	TArray<FGSTN_LinkEventRecord> Events;
};
