// Destructible parts, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Events/GSTN_SimEvent.h"
#include "UObject/ObjectKey.h"
#include "GSTN_PartEvents.generated.h"

struct FGSTN_PartHitOutcome;

/**
 * What a part event says besides the envelope of the hit (combat-parts.md §5.4): which part, the break serial the state had when
 * the event was made and the penetration band of the hit. The simulation event type has no payload slot, so the three bytes travel
 * packed in the event's Magnitude (an integer below 2^24, exact in a float); Pack and Unpack are the only code that knows the
 * layout. 255 stands for "none" in every field (a body hit has no part and no band).
 */
USTRUCT(BlueprintType)
struct GSTNPARTSGAS_API FGSTN_PartEventPayload
{
	GENERATED_BODY()

	/** The part the event is about, 255 = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part Event")
	uint8 PartIndex = 255;

	/** BreakSerial of the part state after the hit: with the entity and the part it is the key presentation and events deduplicate by */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part Event")
	uint8 BreakSerial = 0;

	/** The penetration band the hit fell in, 255 = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part Event")
	uint8 Band = 255;

	/** The payload as the float stored in FGSTN_SimEvent::Magnitude */
	float Pack() const;

	/** The payload a Magnitude holds. A value that is not a packed payload reads as the fields it happens to spell: only part events carry one */
	static FGSTN_PartEventPayload Unpack(float Magnitude);

	/** The payload an index of the outcome stands for: INDEX_NONE and out of range values become 255 */
	static FGSTN_PartEventPayload Make(int32 PartIndex, int32 BreakSerial, int32 Band);

	bool operator==(const FGSTN_PartEventPayload& Other) const
	{
		return PartIndex == Other.PartIndex && BreakSerial == Other.BreakSerial && Band == Other.Band;
	}
};

/**
 * The key a part event is deduplicated by: (entity, part index, break serial), combat-parts.md §5.4. The entity is the object
 * identity of the victim's pawn; the serial is 4 bits and wraps, which is why the dedupe window below is small. Only the events
 * of a break (Broken, AllBroken) are deduplicated: Damaged, WeakPointHit and Ricochet legitimately repeat for one part and one
 * serial, one per hit.
 */
struct GSTNPARTSGAS_API FGSTN_PartEventKey
{
	FObjectKey Entity;
	uint8 PartIndex = 255;
	uint8 BreakSerial = 0;

	bool operator==(const FGSTN_PartEventKey& Other) const
	{
		return Entity == Other.Entity && PartIndex == Other.PartIndex && BreakSerial == Other.BreakSerial;
	}

	friend uint32 GetTypeHash(const FGSTN_PartEventKey& Key)
	{
		return HashCombineFast(GetTypeHash(Key.Entity), static_cast<uint32>(Key.PartIndex) | (static_cast<uint32>(Key.BreakSerial) << 8));
	}
};

/**
 * Remembers the keys of the break events a consumer already acted on (a presentation chunk, a HUD message, an objective), so a
 * rollback that re-delivers the same break does not play it twice. A bounded window: the oldest key is forgotten when the window
 * is full, because the 4 bit serial wraps and a key may legitimately come back after 16 breaks of one entity. Lives in the consumer
 * (outside the simulation), never in the sync state.
 */
class GSTNPARTSGAS_API FGSTN_PartEventDedupe
{
public:
	static constexpr int32 Capacity = 64;

	/** True the first time a key is seen inside the window (the consumer acts), false for a repeat */
	bool TryRegister(const FGSTN_PartEventKey& Key);

	bool Contains(const FGSTN_PartEventKey& Key) const { return Seen.Contains(Key); }

	int32 Num() const { return Order.Num(); }

	void Reset();

private:
	TSet<FGSTN_PartEventKey> Seen;
	TArray<FGSTN_PartEventKey> Order;
};

/**
 * Builds the part events of one hit and appends them to the simulation outbox, the way the combat core appends the hit events
 * (combat.md §5.6): unconditionally, with no role or resimulation branch; the host publishes the outbox once on the authority.
 * The event types are the native tags of GSTNPartsCore (Combat.Event.Part.*).
 *
 * The envelope is the event of the hit itself (its tags carry the attacker's command and move, the hit type, the relationship and
 * the reaction; its frame, instigator and target), so a listener filters a part event with the same tag query as a hit event. Only
 * the event type and the magnitude (the packed payload) are replaced.
 */
struct GSTNPARTSGAS_API FGSTN_PartEvents
{
	/**
	 * The events an outcome entry stands for, in a fixed order: Ricochet, or Damaged then WeakPointHit; then one Broken per newly
	 * broken part in ascending part index (a cascade gives one each); then AllBroken. Nothing for a body hit that broke nothing.
	 * Appended to Outbox; returns how many.
	 */
	static int32 Append(const FGSTN_PartHitOutcome& Outcome, const FGSTN_SimEvent& HitEnvelope, FGSTN_SimEventOutbox& Outbox);

	/** The same events, collected in a list instead of the outbox */
	static void Make(const FGSTN_PartHitOutcome& Outcome, const FGSTN_SimEvent& HitEnvelope, TArray<FGSTN_SimEvent>& Out);

	/** The dedupe key of a Broken or AllBroken event: its target as the entity and the payload's part and serial. Invalid target = a key with a null entity */
	static FGSTN_PartEventKey MakeKey(const FGSTN_SimEvent& Event);

	/** Whether the event is one of the break events the dedupe applies to (Broken, AllBroken) */
	static bool IsBreakEvent(const FGSTN_SimEvent& Event);

	/** The payload of a part event */
	static FGSTN_PartEventPayload GetPayload(const FGSTN_SimEvent& Event) { return FGSTN_PartEventPayload::Unpack(Event.Magnitude); }
};
