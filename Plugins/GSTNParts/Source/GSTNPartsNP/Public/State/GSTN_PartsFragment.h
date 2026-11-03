// Destructible parts, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartsLimits.h"
#include "Math/GSTN_PartsState.h"
#include "Sync/GSTN_SyncFragment.h"
#include "GSTN_PartsFragment.generated.h"

/**
 * The parts of one entity as per-entity combat sync state (combat-parts.md §5.1): the broken mask, the quantised damage of every
 * part and the break serial. A sync fragment of the combat extension surface, so it rides in the combat sync state beside the
 * core, is compared and reconciled with it, and is restored by rollback. An entity without a part set has no fragment: no bytes on
 * the wire and nothing to compare. The main pool is not here, it lives in the attribute where M3 and M4 put it.
 *
 * It is simulated state: integers only, changed by the outcome applier at the victim's tick and nowhere else (never from GAS, never
 * from presentation). The members mirror FGSTN_PartsState, the plain POD the part math works on, so ToState and SetState are the
 * only conversions.
 *
 * Wire format (the byte encoding of the P0c spike, 3 + N bytes, N = the part count of the set):
 *
 *   empty (mask 0, serial 0, every damage 0)  ->  no bytes at all; the list framing's length byte says 0
 *   otherwise    uint16 BrokenMask (little endian), N x uint8 PartDamage, uint8 BreakSerial (4 bits used, a larger value is an error)
 *
 * N is not sent: both machines load the same part set, and the reader derives N from the payload length (the registry reads into a
 * default instance, so the count cannot be an instance field read before the bytes). A pristine entity therefore costs only the
 * list framing, 3 bytes (type id, length, count) plus the list's flag bit; N = 16 damaged is a 19 byte payload and 23 bytes with the
 * framing. Bit packing would save 0 to 2 bytes and cost the length-derived N, so it was not chosen.
 */
USTRUCT()
struct GSTNPARTSNP_API FGSTN_PartsFragment : public FGSTN_SyncFragment
{
	GENERATED_BODY()

	/** Bytes of the payload besides the damage array: the 16 bit mask and the serial */
	static constexpr int32 HeaderBytes = 3;

	/** Bit i set once part i broke; only an explicit restore clears it */
	uint16 BrokenMask = 0;

	/** Damage over cap of part i, 0..255 (0 pristine, 255 at the cap). Entries at and beyond PartCount stay 0 */
	uint8 PartDamage[GSTN_PartsLimits::MaxParts] = {};

	/** Counts the events that changed the mask, in 4 bits, wrapping: with the part it is the key events are deduplicated by */
	uint8 BreakSerial = 0;

	/**
	 * Parts of the entity's set, 0..16. Set by whoever attaches the fragment from the part set; after a read it is derived from the
	 * payload length (0 for an empty fragment). It only decides how many damage bytes are written: not state, not compared
	 */
	uint8 PartCount = 0;

	/** Mask 0, serial 0 and no damage: written as zero bytes */
	bool IsEmpty() const;

	/** Damage bytes the payload carries: PartCount, raised to cover the highest part with damage or a broken bit so nothing is ever cut off */
	int32 GetWireCount() const;

	/** The payload size for a part count, in bytes, as written for a non-empty fragment */
	static constexpr int32 GetPayloadBytes(int32 InPartCount) { return HeaderBytes + InPartCount; }

	/** The state the fragment holds, as the part math takes it */
	FGSTN_PartsState ToState() const;

	/** Overwrites the state (mask, damage, serial) and records the part count; the one write the outcome applier makes */
	void SetState(const FGSTN_PartsState& State, int32 InPartCount);

	virtual void NetSerialize(FArchive& Ar) override;

	/** Exact integer equality of mask, damage and serial: nothing here is a float, so there is no tolerance. PartCount is not state */
	virtual bool ShouldReconcile(const FGSTN_SyncFragment& Authority) const override;

	/** Discrete state: the copy of the later sample the registry hands over is already the result, presentation reads the mask and never a blend */
	virtual void Interpolate(const FGSTN_SyncFragment* From, const FGSTN_SyncFragment* To, float PCT) override {}

	/** "Parts mask 0x0005 serial 3 n=4 [0:12% 1:100% 2:0% 3:0%]": the mask in hex, the percent of each part rounded to the nearest integer */
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
};
