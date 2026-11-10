// Held objects, simulation side (needs the GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "Sync/GSTN_InboxEntry.h"
#include "Sync/GSTN_SyncFragment.h"

class UGSTN_LinkTypeDefinition;
class UGSTN_LinkRegistry;
struct FGSTN_LinkState;
enum class EGSTN_HeldBreakReason : uint8;

/**
 * The hold as a GSTNLink Hand link (M10 G1; slice 2 kept it in a fragment of its own). A Held link joins the holder (role 1, Combat.Link.Role.Holder)
 * and the object (role 2), a partner of kind GSTNEntityKinds::Object with no snapshot entry: the host forms it with a LinkCommand Form on the
 * holder's inbox (stamped with the holder's latest frame, consumed one frame later) and ends it with a Break of the same serial. The holder's
 * link fragment is sync state, so "holding" is predicted and rolled back like a grab; a client gets it by correction.
 *
 * The type is made in code (no asset): Hand channel, nobody parked, the holder acts while linked, no break rule of GSTNLink's own (the host's
 * held break rules decide, FGSTN_HeldBreakRules); the watchdog only ends a hold whose object is gone, through the held subsystem's partner
 * resolver. Its role effects are where G6's carry load goes.
 */
struct GSTNHELDRUNTIME_API FGSTN_HeldLinkType
{
	/** A new definition of the type (tests build their own) */
	static UGSTN_LinkTypeDefinition* Build(UObject* Outer);
	/** Adds the process's definition to the link registry, once (the registry is an engine subsystem: called after engine init) */
	static void Register();
	static void Unregister();
	/** The registered definition, or null */
	static const UGSTN_LinkTypeDefinition* Get();
	/** The type's wire index in Registry (the process's when null); 0 when it is not registered */
	static uint8 GetTypeIndex(const UGSTN_LinkRegistry* Registry = nullptr);

	/** Is this Hand state a live hold (a Held link, the holder's side)? */
	static bool IsHolding(const FGSTN_LinkState& Hand, uint8 HeldTypeIndex);

	/** The generic break reason a held break ends the link with (drop = Release, throw = Throw, holder state = Hit, KO = OwnDeath, gone = Forced) */
	static uint8 ToLinkReason(EGSTN_HeldBreakReason Reason);

	/**
	 * The host's command entries. Hold: a Form of the Held type on the holder's Hand channel with Serial, the partner ObjectId of kind Object
	 * (ObjectSerial = the object's hold serial). Release: the Break of that serial. The caller stamps both
	 */
	static FGSTN_InboxEntry MakeHold(uint8 TypeIndex, uint8 Serial, int32 ObjectId, uint8 ObjectSerial);
	static FGSTN_InboxEntry MakeRelease(uint8 Serial, uint8 LinkReason);
};

/** Projects Combat.Held.Carrying from the holder's Hand link, every frame on every machine (set, never added: a resimulated frame inherits nothing) */
class GSTNHELDRUNTIME_API FGSTN_HeldTickExtension : public IGSTN_CombatTickExtension
{
public:
	virtual uint8 GetTypeId() const override { return GSTNFeatureIds::Held; }
	virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) override;
};
