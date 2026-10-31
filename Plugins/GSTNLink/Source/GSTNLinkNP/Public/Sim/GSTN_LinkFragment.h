// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "State/GSTN_LinkState.h"
#include "Sync/GSTN_SyncFragment.h"
#include "GSTN_LinkFragment.generated.h"

/**
 * The link state of one entity as per-entity combat sync state (m6 spec 5.1, 7.1): the two link channels (Body parks, Hand never does),
 * the generic block of link actions (refusal bookkeeping, cooldown, partner moves, the exit the watchdog decided), the grab immunity stamp
 * and the last history frame the mash counted. A sync fragment of the combat extension surface, so it rides in the combat sync state
 * beside the core, is compared and reconciled with it and is restored by rollback. An entity that never linked has no fragment, which
 * costs nothing on the wire; one that linked once keeps a fragment holding only its serials.
 *
 * It is simulated state, written by the link tick extension, the inbox handlers of LinkResponse / LinkCommand and the outcome applier of
 * the LinkRequest step, nowhere else (never from GAS, never from presentation). No tag: the link type is a registry index, the role an index
 * of the definition's two roles, the reason an index of the registry's reason table.
 *
 * Wire format: nothing at all for a fragment at its defaults (IsDefault; the list framing says 0 bytes), else a presence byte and the groups
 * that are present: Body and Hand (each the link state's own form: a header byte, the serial, the live fields; at most 22 bytes), actions
 * (refused serial 2, refused frame 4, cooldown 4, active move 6, pending move 6, exit 4 + 3 x 4 + 4 + 3 x 4 + 1), grab immunity 4, mash
 * processed 4. An idle entity is the one flag bit of the fragment list; a live grab on one channel is 1 + 22 bytes.
 */
USTRUCT()
struct GSTNLINKNP_API FGSTN_LinkFragment : public FGSTN_SyncFragment
{
	GENERATED_BODY()

	/** Both link channels: Body (the grab, the latch, a mount) and Hand (a held object) */
	FGSTN_LinkStates Links;

	/** The link actions and partner moves of the entity (m6 spec 7.0 item 5); idle for a type without partner moves */
	FGSTN_LinkActions Actions;

	/** The entity cannot be grabbed before this frame (set when a grab ends, F11/F12). 0 = never immune */
	int32 GrabImmunityEndFrame = 0;

	/** The last input history frame the mash rule counted (grabbed side, F12), so a re-simulated frame counts a press once. 0 = none */
	int32 MashProcessedFrame = 0;

	FGSTN_LinkState& Get(EGSTN_LinkChannel Channel) { return Links.Get(Channel); }
	const FGSTN_LinkState& Get(EGSTN_LinkChannel Channel) const { return Links.Get(Channel); }

	/** Every field at its default (serials included): written as zero bytes */
	bool IsDefault() const;

	bool IsGrabImmune(int32 Frame) const { return Frame < GrabImmunityEndFrame; }

	bool operator==(const FGSTN_LinkFragment& Other) const;

	virtual void NetSerialize(FArchive& Ar) override;

	/** Exact equality of every field: the exit position and velocity are the watchdog's quantised result, written identically on every machine */
	virtual bool ShouldReconcile(const FGSTN_SyncFragment& Authority) const override;

	/** Discrete state: the copy of the later sample the registry hands over is already the result */
	virtual void Interpolate(const FGSTN_SyncFragment* From, const FGSTN_SyncFragment* To, float PCT) override {}

	/** "Link body{...} hand{...} immune 120 mash 98" with the groups that are live */
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
};

/**
 * Registers and unregisters FGSTN_LinkFragment under GSTNFeatureIds::Link (id 18 of the central registry). The module does it at startup
 * and shutdown, before the first simulation tick; exposed so a test can restore the registration it changed.
 */
struct GSTNLINKNP_API FGSTN_LinkFragmentRegistration
{
	/** True when the fragment is registered after the call (registering twice is fine) */
	static bool Register();

	static void Unregister();
};
