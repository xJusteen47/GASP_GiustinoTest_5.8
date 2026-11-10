// Held objects, simulation side (needs the GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "Sync/GSTN_InboxEntry.h"
#include "Sync/GSTN_SyncFragment.h"

#include "GSTN_HeldSim.generated.h"

/**
 * The hand of an entity in its combat sync state (M10 slice 2): which object it holds, so "holding" is predicted and rolled back like every other
 * combat state, and the held commands' bindings (Combat.Held.Carrying) agree on every machine for the same frame. Written only by the host's
 * Held inbox commands (pickup, drop, throw, break), consumed one frame after the host decided; a client gets it by correction, as a grab.
 *
 * The object itself stays the host's (UGSTN_HeldObjectComponent: where it is drawn, its flight): the fragment is the holder's half of the link.
 * Added on the first hold, so an entity that never held anything carries no bytes for it.
 */
USTRUCT()
struct GSTNHELDRUNTIME_API FGSTN_HeldFragment : public FGSTN_SyncFragment
{
	GENERATED_BODY()

	/** The host's object id + 1; 0 = empty hand */
	uint16 ObjectRef = 0;
	/** The hold's serial: a release only clears the hold it names, so a late release never empties a newer hold */
	uint8 Serial = 0;
	/** First frame of the hold; INDEX_NONE with an empty hand */
	int32 HoldFrame = INDEX_NONE;

	bool IsHolding() const { return ObjectRef != 0; }
	bool operator==(const FGSTN_HeldFragment& Other) const { return ObjectRef == Other.ObjectRef && Serial == Other.Serial && HoldFrame == Other.HoldFrame; }

	virtual void NetSerialize(FArchive& Ar) override;
	/** Exact equality: nothing here is a float */
	virtual bool ShouldReconcile(const FGSTN_SyncFragment& Authority) const override;
	/** Discrete state: the later sample is the result */
	virtual void Interpolate(const FGSTN_SyncFragment* From, const FGSTN_SyncFragment* To, float PCT) override {}
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
};

/** The Held inbox kinds (GSTNInboxKinds::FirstHeld..LastHeld) */
namespace GSTNHeldInboxKinds
{
	/** The host's hold or release of an object (FGSTN_HeldCommandEntry) */
	constexpr uint8 Command = GSTNInboxKinds::FirstHeld;
}

/** Payload of a Held command: the host's pickup (bHold) or the end of a hold (drop, throw, break) */
USTRUCT()
struct GSTNHELDRUNTIME_API FGSTN_HeldCommandEntry
{
	GENERATED_BODY()

	UPROPERTY()
	bool bHold = false;

	UPROPERTY()
	uint16 ObjectRef = 0;

	UPROPERTY()
	uint8 Serial = 0;
};

/** The Held command kind and its handler (writes FGSTN_HeldFragment of the receiver) */
struct GSTNHELDRUNTIME_API FGSTN_HeldInbox
{
	static bool Register();
	static void Unregister();
	static FGSTN_InboxEntry MakeCommand(const FGSTN_HeldCommandEntry& Payload);
	/** The handler, public for tests: a hold replaces the hand, a release clears it only when object and serial match */
	static void Apply(FGSTN_HeldFragment& Fragment, const FGSTN_HeldCommandEntry& Command, int32 Frame);
};

/** Projects Combat.Held.Carrying from FGSTN_HeldFragment, every frame on every machine (set, never added: a resimulated frame inherits nothing) */
class GSTNHELDRUNTIME_API FGSTN_HeldTickExtension : public IGSTN_CombatTickExtension
{
public:
	virtual uint8 GetTypeId() const override { return GSTNFeatureIds::Held; }
	virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) override;
};
