// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "State/GSTN_LinkState.h"
#include "Sync/GSTN_EntityRef.h"

class UGSTN_LinkTypeDefinition;

/** What a link needs to know about one entity of its host, as values: no actor, no component */
struct GSTNLINKCORE_API FGSTN_LinkEntityView
{
	FGSTN_EntityRef Ref;

	/** The state tags the entity projects */
	FGameplayTagContainer Tags;

	/** The weight a grab or latch is decided by */
	float EffectiveWeight = 100.f;

	/** The damage percent the mash duration reads */
	float DamagePercent = 0.f;

	FTransform Transform = FTransform::Identity;
	bool bAlive = true;

	/** The entity cannot be grabbed / latched onto before these frames */
	int32 GrabImmunityEndFrame = 0;
	int32 LatchImmunityEndFrame = 0;

	/** Both link slots */
	FGSTN_LinkStates Links;
};

/** A link event the host publishes: formed or broken, with the reason and the frame (the authoritative outbox entry, m6 spec 7.6) */
struct GSTNLINKCORE_API FGSTN_LinkEventRecord
{
	/** GSTNLinkTags::Event_LinkFormed, Event_LinkBroken or Event_GrabEscaped */
	FGameplayTag Event;
	int32 Frame = 0;
	FGSTN_EntityRef Initiator;
	FGSTN_EntityRef Target;
	uint8 LinkTypeIndex = 0;
	EGSTN_LinkBreakReason Reason = EGSTN_LinkBreakReason::None;
};

/** Which immunity stamp a host sets on an entity when its link ends */
enum class EGSTN_LinkImmunityKind : uint8
{
	Grab,
	Latch
};

/**
 * What a link needs from its host (m6 spec 7.0 item 8): the entities as values, the definitions by index, the link states, the
 * immunity stamps, the teleport of an exiting entity and the event outbox. The Network Prediction module implements it over the
 * combat model; FGSTN_FakeLinkHost implements it with plain structs, so the plugins built on links test their rules, break
 * evaluation and placement without NP, a world or a pawn.
 */
class GSTNLINKCORE_API IGSTN_LinkHost
{
public:
	virtual ~IGSTN_LinkHost() = default;

	/** The combat frame links are decided on */
	virtual int32 GetFrame() const = 0;

	/** The entity as values, or false when the reference names no entity of the host (a destroyed one, a stale serial) */
	virtual bool GetEntityView(const FGSTN_EntityRef& Entity, FGSTN_LinkEntityView& OutView) const = 0;

	/** Writes one link slot of an entity */
	virtual void SetLinkState(const FGSTN_EntityRef& Entity, EGSTN_LinkChannel Channel, const FGSTN_LinkState& State) = 0;

	/** The definition behind a link type index, or null for 0 and for an unknown index */
	virtual const UGSTN_LinkTypeDefinition* GetLinkDefinition(uint8 LinkTypeIndex) const = 0;

	/** Stamps the immunity of an entity */
	virtual void SetImmunity(const FGSTN_EntityRef& Entity, EGSTN_LinkImmunityKind Kind, int32 EndFrame) = 0;

	/** Puts an exiting entity at a place with a velocity (the host turns it into its movement request) */
	virtual void PlaceEntity(const FGSTN_EntityRef& Entity, const FTransform& Transform, const FVector& Velocity) = 0;

	/** Publishes an event once */
	virtual void PublishEvent(const FGSTN_LinkEventRecord& Event) = 0;
};
