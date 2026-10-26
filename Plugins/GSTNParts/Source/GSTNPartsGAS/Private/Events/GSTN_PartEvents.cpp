// Destructible parts, ability-system half (no Network Prediction, no Mover).

#include "Events/GSTN_PartEvents.h"

#include "GSTN_PartsTags.h"
#include "Pipeline/GSTN_PartHitOutcome.h"

namespace
{
	constexpr uint8 NoneByte = 255;

	uint8 ToByte(int32 Value)
	{
		return Value >= 0 && Value < NoneByte ? static_cast<uint8>(Value) : NoneByte;
	}
}

float FGSTN_PartEventPayload::Pack() const
{
	// At most 24 bits, which a float holds exactly
	return static_cast<float>(static_cast<uint32>(PartIndex) | (static_cast<uint32>(BreakSerial) << 8) | (static_cast<uint32>(Band) << 16));
}

FGSTN_PartEventPayload FGSTN_PartEventPayload::Unpack(float Magnitude)
{
	const uint32 Bits = Magnitude > 0.f ? static_cast<uint32>(Magnitude) : 0u;
	FGSTN_PartEventPayload Payload;
	Payload.PartIndex = static_cast<uint8>(Bits & 0xFFu);
	Payload.BreakSerial = static_cast<uint8>((Bits >> 8) & 0xFFu);
	Payload.Band = static_cast<uint8>((Bits >> 16) & 0xFFu);
	return Payload;
}

FGSTN_PartEventPayload FGSTN_PartEventPayload::Make(int32 PartIndex, int32 BreakSerial, int32 Band)
{
	FGSTN_PartEventPayload Payload;
	Payload.PartIndex = ToByte(PartIndex);
	Payload.BreakSerial = ToByte(BreakSerial);
	Payload.Band = ToByte(Band);
	return Payload;
}

bool FGSTN_PartEventDedupe::TryRegister(const FGSTN_PartEventKey& Key)
{
	if (Seen.Contains(Key))
	{
		return false;
	}
	if (Order.Num() >= Capacity)
	{
		Seen.Remove(Order[0]);
		Order.RemoveAt(0);
	}
	Seen.Add(Key);
	Order.Add(Key);
	return true;
}

void FGSTN_PartEventDedupe::Reset()
{
	Seen.Reset();
	Order.Reset();
}

void FGSTN_PartEvents::Make(const FGSTN_PartHitOutcome& Outcome, const FGSTN_SimEvent& HitEnvelope, TArray<FGSTN_SimEvent>& Out)
{
	const auto Emit = [&Out, &HitEnvelope](const FGameplayTag& Type, const FGSTN_PartEventPayload& Payload)
	{
		FGSTN_SimEvent Event = HitEnvelope;
		Event.EventType = Type;
		Event.Magnitude = Payload.Pack();
		Out.Add(MoveTemp(Event));
	};

	const FGSTN_PartEventPayload HitPayload = FGSTN_PartEventPayload::Make(Outcome.PartIndex, Outcome.ResultBreakSerial, Outcome.BandIndex);

	if (Outcome.HasFlag(GSTNPartOutcomeFlags::Ricochet))
	{
		Emit(GSTNPartsTags::Event_Part_Ricochet, HitPayload);
	}
	else
	{
		if (Outcome.HasFlag(GSTNPartOutcomeFlags::PartDamaged))
		{
			Emit(GSTNPartsTags::Event_Part_Damaged, HitPayload);
		}
		if (Outcome.HasFlag(GSTNPartOutcomeFlags::WeakPointHit))
		{
			Emit(GSTNPartsTags::Event_Part_WeakPointHit, HitPayload);
		}
	}

	uint16 Newly = Outcome.NewlyBrokenMask;
	while (Newly != 0)
	{
		const int32 Index = FMath::CountTrailingZeros(static_cast<uint32>(Newly));
		Newly = static_cast<uint16>(Newly & (Newly - 1));
		Emit(GSTNPartsTags::Event_Part_Broken, FGSTN_PartEventPayload::Make(Index, Outcome.ResultBreakSerial, Outcome.BandIndex));
	}

	if (Outcome.HasFlag(GSTNPartOutcomeFlags::AllBroken))
	{
		Emit(GSTNPartsTags::Event_Part_AllBroken, HitPayload);
	}
}

int32 FGSTN_PartEvents::Append(const FGSTN_PartHitOutcome& Outcome, const FGSTN_SimEvent& HitEnvelope, FGSTN_SimEventOutbox& Outbox)
{
	TArray<FGSTN_SimEvent> Made;
	Make(Outcome, HitEnvelope, Made);
	for (FGSTN_SimEvent& Event : Made)
	{
		Outbox.Add(MoveTemp(Event));
	}
	return Made.Num();
}

int32 FGSTN_PartEvents::AppendRestored(AActor* Target, AActor* Instigator, int32 Frame, uint16 RestoredMask, uint8 BreakSerial, FGSTN_SimEventOutbox& Outbox)
{
	int32 Count = 0;
	while (RestoredMask != 0)
	{
		const int32 Index = FMath::CountTrailingZeros(static_cast<uint32>(RestoredMask));
		RestoredMask = static_cast<uint16>(RestoredMask & (RestoredMask - 1));
		FGSTN_SimEvent Event;
		Event.EventType = GSTNPartsTags::Event_Part_Restored;
		Event.Frame = Frame;
		Event.Target = Target;
		Event.Instigator = Instigator;
		Event.Magnitude = FGSTN_PartEventPayload::Make(Index, BreakSerial, 255).Pack();
		Outbox.Add(MoveTemp(Event));
		++Count;
	}
	return Count;
}

FGSTN_PartEventKey FGSTN_PartEvents::MakeKey(const FGSTN_SimEvent& Event)
{
	const FGSTN_PartEventPayload Payload = GetPayload(Event);
	FGSTN_PartEventKey Key;
	Key.Entity = FObjectKey(Event.Target.Get());
	Key.PartIndex = Payload.PartIndex;
	Key.BreakSerial = Payload.BreakSerial;
	return Key;
}

bool FGSTN_PartEvents::IsBreakEvent(const FGSTN_SimEvent& Event)
{
	return Event.EventType == GSTNPartsTags::Event_Part_Broken || Event.EventType == GSTNPartsTags::Event_Part_AllBroken;
}
