// Automation tests of the part events (plan P5): the payload, the dedupe key, the hit envelope and the order independence of two
// outcome entries. The events are built from a hand-made outcome entry; the part math is tested in GSTNPartsCore.

#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Events/GSTN_PartEvents.h"
#include "GameFramework/Actor.h"
#include "GSTN_PartsTags.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Tests/GSTN_PartsGASTestTypes.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_PartHitOutcome MakeOutcome(int32 PartIndex, int32 Band, uint8 Serial, uint16 NewlyBroken, uint8 Flags)
	{
		FGSTN_PartHitOutcome Outcome;
		Outcome.PartIndex = PartIndex;
		Outcome.BandIndex = Band;
		Outcome.ResultBreakSerial = Serial;
		Outcome.NewlyBrokenMask = NewlyBroken;
		Outcome.ResultBrokenMask = NewlyBroken;
		Outcome.Flags = Flags;
		return Outcome;
	}

	FGSTN_SimEvent MakeEnvelope(AActor* Attacker, AActor* Victim)
	{
		FGSTN_SimEvent Envelope;
		Envelope.EventType = GSTNPartsTags::Part_Effect;
		Envelope.Frame = 42;
		Envelope.Instigator = Attacker;
		Envelope.Target = Victim;
		Envelope.Magnitude = 77.f;
		Envelope.Tags.AddTag(GSTNPartsTags::Part_Effect);
		Envelope.Tags.AddTag(GSTNPartsTags::Entity_Destructible);
		return Envelope;
	}

	/** (event type name, part index, serial) of every event, sorted: the multiset a consumer sees, whatever the order of the entries */
	TArray<FString> Fingerprint(const TArray<FGSTN_SimEvent>& Events)
	{
		TArray<FString> Out;
		for (const FGSTN_SimEvent& Event : Events)
		{
			const FGSTN_PartEventPayload Payload = FGSTN_PartEvents::GetPayload(Event);
			Out.Add(FString::Printf(TEXT("%s/%d/%d"), *Event.EventType.ToString(), Payload.PartIndex, Payload.BreakSerial));
		}
		Out.Sort();
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGASPayloadRoundTripTest, "GSTNParts.GAS.Events.PayloadRoundTrip", GSTNPartsGASTest::Flags)
bool FGSTN_PartsGASPayloadRoundTripTest::RunTest(const FString& Parameters)
{
	int32 Checked = 0;
	for (const int32 Part : { 0, 1, 7, 15, 255 })
	{
		for (int32 Serial = 0; Serial <= 15; ++Serial)
		{
			for (const int32 Band : { 0, 1, 2, 3, 254, 255 })
			{
				FGSTN_PartEventPayload Payload;
				Payload.PartIndex = static_cast<uint8>(Part);
				Payload.BreakSerial = static_cast<uint8>(Serial);
				Payload.Band = static_cast<uint8>(Band);
				const float Packed = Payload.Pack();
				TestEqual(TEXT("the packed value is an exact integer"), Packed, FMath::FloorToFloat(Packed));
				if (!(FGSTN_PartEventPayload::Unpack(Packed) == Payload))
				{
					AddError(FString::Printf(TEXT("round trip failed for part %d serial %d band %d"), Part, Serial, Band));
					return false;
				}
				++Checked;
			}
		}
	}
	TestEqual(TEXT("every combination was checked"), Checked, 5 * 16 * 6);

	const FGSTN_PartEventPayload None = FGSTN_PartEventPayload::Make(INDEX_NONE, 3, INDEX_NONE);
	TestEqual(TEXT("INDEX_NONE part is 255"), static_cast<int32>(None.PartIndex), 255);
	TestEqual(TEXT("INDEX_NONE band is 255"), static_cast<int32>(None.Band), 255);
	TestEqual(TEXT("the serial is kept"), static_cast<int32>(None.BreakSerial), 3);
	TestEqual(TEXT("the largest packed value stays exact in a float"), static_cast<uint32>(FGSTN_PartEventPayload::Make(254, 254, 254).Pack()), 254u | (254u << 8) | (254u << 16));
	TestTrue(TEXT("a non-positive magnitude unpacks to the zero payload"), FGSTN_PartEventPayload::Unpack(-4.f) == FGSTN_PartEventPayload::Unpack(0.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGASDedupeKeyTest, "GSTNParts.GAS.Events.DedupeKey", GSTNPartsGASTest::Flags)
bool FGSTN_PartsGASDedupeKeyTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UObject> EntityA(NewObject<UGameplayEffect>(GetTransientPackage()));
	TStrongObjectPtr<UObject> EntityB(NewObject<UGameplayEffect>(GetTransientPackage()));

	const auto MakeKey = [](const UObject* Entity, uint8 Part, uint8 Serial)
	{
		FGSTN_PartEventKey Key;
		Key.Entity = FObjectKey(Entity);
		Key.PartIndex = Part;
		Key.BreakSerial = Serial;
		return Key;
	};

	FGSTN_PartEventDedupe Dedupe;
	TestTrue(TEXT("a first key is new"), Dedupe.TryRegister(MakeKey(EntityA.Get(), 2, 1)));
	TestFalse(TEXT("the same key again is a repeat (a rollback re-delivered the break)"), Dedupe.TryRegister(MakeKey(EntityA.Get(), 2, 1)));
	TestTrue(TEXT("another serial is a new break"), Dedupe.TryRegister(MakeKey(EntityA.Get(), 2, 2)));
	TestTrue(TEXT("another part is a new break"), Dedupe.TryRegister(MakeKey(EntityA.Get(), 3, 1)));
	TestTrue(TEXT("another entity is a new break"), Dedupe.TryRegister(MakeKey(EntityB.Get(), 2, 1)));
	TestEqual(TEXT("four keys held"), Dedupe.Num(), 4);
	TestTrue(TEXT("Contains sees them"), Dedupe.Contains(MakeKey(EntityA.Get(), 2, 1)));

	// The window is bounded: the 4 bit serial wraps, so an old key is forgotten and may come back
	for (int32 Index = 0; Index < FGSTN_PartEventDedupe::Capacity; ++Index)
	{
		Dedupe.TryRegister(MakeKey(EntityB.Get(), static_cast<uint8>(Index % 16), static_cast<uint8>(5 + Index / 16)));
	}
	TestTrue(TEXT("the window never grows past its capacity"), Dedupe.Num() <= FGSTN_PartEventDedupe::Capacity);
	TestFalse(TEXT("the oldest key was forgotten"), Dedupe.Contains(MakeKey(EntityA.Get(), 2, 1)));
	Dedupe.Reset();
	TestEqual(TEXT("reset empties it"), Dedupe.Num(), 0);

	// The key of an event: its target and the payload's part and serial
	FGSTN_SimEvent Event;
	Event.EventType = GSTNPartsTags::Event_Part_Broken;
	Event.Magnitude = FGSTN_PartEventPayload::Make(4, 9, 1).Pack();
	const FGSTN_PartEventKey Key = FGSTN_PartEvents::MakeKey(Event);
	TestEqual(TEXT("part from the payload"), static_cast<int32>(Key.PartIndex), 4);
	TestEqual(TEXT("serial from the payload"), static_cast<int32>(Key.BreakSerial), 9);
	TestTrue(TEXT("a break event is deduplicated"), FGSTN_PartEvents::IsBreakEvent(Event));
	Event.EventType = GSTNPartsTags::Event_Part_Damaged;
	TestFalse(TEXT("a damaged event repeats legitimately and is not"), FGSTN_PartEvents::IsBreakEvent(Event));
	Event.EventType = GSTNPartsTags::Event_Part_AllBroken;
	TestTrue(TEXT("AllBroken is deduplicated too"), FGSTN_PartEvents::IsBreakEvent(Event));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGASEnvelopeTest, "GSTNParts.GAS.Events.EnvelopeCarriesHitData", GSTNPartsGASTest::Flags)
bool FGSTN_PartsGASEnvelopeTest::RunTest(const FString& Parameters)
{
	GSTNPartsGASTest::FRig Rig;
	AActor* Attacker = Rig.World->SpawnActor<AActor>();
	AActor* Victim = Rig.World->SpawnActor<AActor>();
	const FGSTN_SimEvent Envelope = MakeEnvelope(Attacker, Victim);

	// Damage + weak point + two parts broken (a cascade) + the last one: the five kinds, in the documented order
	const FGSTN_PartHitOutcome Outcome = MakeOutcome(3, 1, 5, (1u << 2) | (1u << 3),
		GSTNPartOutcomeFlags::PartDamaged | GSTNPartOutcomeFlags::WeakPointHit | GSTNPartOutcomeFlags::AllBroken);

	FGSTN_SimEventOutbox Outbox;
	TestEqual(TEXT("five events appended"), FGSTN_PartEvents::Append(Outcome, Envelope, Outbox), 5);
	TestEqual(TEXT("the outbox holds them"), Outbox.Events.Num(), 5);
	if (Outbox.Events.Num() != 5)
	{
		return false;
	}

	const FGameplayTag Expected[5] = { GSTNPartsTags::Event_Part_Damaged, GSTNPartsTags::Event_Part_WeakPointHit, GSTNPartsTags::Event_Part_Broken,
		GSTNPartsTags::Event_Part_Broken, GSTNPartsTags::Event_Part_AllBroken };
	const int32 ExpectedPart[5] = { 3, 3, 2, 3, 3 };
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FGSTN_SimEvent& Event = Outbox.Events[Index];
		TestTrue(*FString::Printf(TEXT("event %d type"), Index), Event.EventType == Expected[Index]);
		TestTrue(*FString::Printf(TEXT("event %d keeps the hit's tags"), Index), Event.Tags.HasAllExact(Envelope.Tags));
		TestEqual(*FString::Printf(TEXT("event %d keeps the frame"), Index), Event.Frame, 42);
		TestTrue(*FString::Printf(TEXT("event %d keeps the instigator"), Index), Event.Instigator.Get() == Attacker);
		TestTrue(*FString::Printf(TEXT("event %d keeps the target"), Index), Event.Target.Get() == Victim);
		const FGSTN_PartEventPayload Payload = FGSTN_PartEvents::GetPayload(Event);
		TestEqual(*FString::Printf(TEXT("event %d part"), Index), static_cast<int32>(Payload.PartIndex), ExpectedPart[Index]);
		TestEqual(*FString::Printf(TEXT("event %d serial"), Index), static_cast<int32>(Payload.BreakSerial), 5);
		TestEqual(*FString::Printf(TEXT("event %d band"), Index), static_cast<int32>(Payload.Band), 1);
	}

	// A ricochet only tells the ricochet
	TArray<FGSTN_SimEvent> Ricochet;
	FGSTN_PartEvents::Make(MakeOutcome(1, 2, 0, 0, GSTNPartOutcomeFlags::Ricochet), Envelope, Ricochet);
	TestEqual(TEXT("one ricochet event"), Ricochet.Num(), 1);
	if (Ricochet.Num() == 1)
	{
		TestTrue(TEXT("it is the ricochet tag"), Ricochet[0].EventType == GSTNPartsTags::Event_Part_Ricochet);
	}

	// A body hit that broke nothing is silent; so is the empty outcome
	TArray<FGSTN_SimEvent> None;
	FGSTN_PartEvents::Make(MakeOutcome(INDEX_NONE, INDEX_NONE, 0, 0, 0), Envelope, None);
	TestTrue(TEXT("a body hit has no part event"), None.IsEmpty());

	// The envelope itself is untouched
	TestEqual(TEXT("the source event keeps its magnitude"), Envelope.Magnitude, 77.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGASTwoEntriesTest, "GSTNParts.GAS.Events.TwoEntriesOrderIndependent", GSTNPartsGASTest::Flags)
bool FGSTN_PartsGASTwoEntriesTest::RunTest(const FString& Parameters)
{
	GSTNPartsGASTest::FRig Rig;
	AActor* Attacker = Rig.World->SpawnActor<AActor>();
	AActor* Victim = Rig.World->SpawnActor<AActor>();
	const FGSTN_SimEvent Envelope = MakeEnvelope(Attacker, Victim);

	// Two hits of one drain break different parts; the entries differ in the serial they carry
	const FGSTN_PartHitOutcome First = MakeOutcome(1, 0, 1, 1u << 1, GSTNPartOutcomeFlags::PartDamaged);
	const FGSTN_PartHitOutcome Second = MakeOutcome(2, 0, 2, 1u << 2, GSTNPartOutcomeFlags::PartDamaged);

	TArray<FGSTN_SimEvent> ForwardEvents;
	FGSTN_PartEvents::Make(First, Envelope, ForwardEvents);
	FGSTN_PartEvents::Make(Second, Envelope, ForwardEvents);
	TArray<FGSTN_SimEvent> BackwardEvents;
	FGSTN_PartEvents::Make(Second, Envelope, BackwardEvents);
	FGSTN_PartEvents::Make(First, Envelope, BackwardEvents);

	TestEqual(TEXT("four events either way"), ForwardEvents.Num(), 4);
	TestTrue(TEXT("the same events whatever the order of the entries"), Fingerprint(ForwardEvents) == Fingerprint(BackwardEvents));

	// A consumer deduplicating by key acts on the same two breaks in either order, and once each when the drain is delivered twice
	const auto RegisterBreaks = [](const TArray<FGSTN_SimEvent>& Events, FGSTN_PartEventDedupe& Dedupe)
	{
		int32 Acted = 0;
		for (const FGSTN_SimEvent& Event : Events)
		{
			if (FGSTN_PartEvents::IsBreakEvent(Event) && Dedupe.TryRegister(FGSTN_PartEvents::MakeKey(Event)))
			{
				++Acted;
			}
		}
		return Acted;
	};
	FGSTN_PartEventDedupe ForwardDedupe;
	FGSTN_PartEventDedupe BackwardDedupe;
	TestEqual(TEXT("two breaks acted on, forward"), RegisterBreaks(ForwardEvents, ForwardDedupe), 2);
	TestEqual(TEXT("two breaks acted on, backward"), RegisterBreaks(BackwardEvents, BackwardDedupe), 2);
	TestEqual(TEXT("redelivery acts on nothing"), RegisterBreaks(ForwardEvents, ForwardDedupe), 0);
	TestEqual(TEXT("the same keys either way"), ForwardDedupe.Num(), BackwardDedupe.Num());
	for (const FGSTN_SimEvent& Event : BackwardEvents)
	{
		if (FGSTN_PartEvents::IsBreakEvent(Event))
		{
			TestTrue(TEXT("a key of the backward order is known to the forward one"), ForwardDedupe.Contains(FGSTN_PartEvents::MakeKey(Event)));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
