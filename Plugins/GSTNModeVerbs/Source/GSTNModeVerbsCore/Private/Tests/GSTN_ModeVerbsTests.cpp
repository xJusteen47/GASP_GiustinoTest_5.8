// M12 G2: mode verbs, pure. The spec and payload, the once-only confirmation under forced rollbacks, and the requestable gate (ActionableFrom).

#include "GSTN_ModeVerbTags.h"
#include "GSTN_ModeVerbTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_VerbConfirmRequest MakeRequest(uint32 InstigatorId, int32 Frame, const FGameplayTag& Verb = GSTNModeVerbTags::Ability_Mode_Interact)
	{
		FGSTN_VerbConfirmRequest Request;
		Request.Spec.VerbTag = Verb;
		Request.Payload.VerbTag = Verb;
		Request.Payload.ConfirmFrame = Frame;
		Request.InstigatorId = InstigatorId;
		return Request;
	}

	/** One fixed tick of the host's world manager: the ability adds in the tick, the subsystem flushes after it */
	int32 PostTick(FGSTN_VerbConfirmQueue& Queue, bool bAuthority, bool bResimulating, int32 Frame, int32& OutPublishedTotal)
	{
		const int32 Published = Queue.Flush(bAuthority, bResimulating, Frame, [&OutPublishedTotal](const FGSTN_VerbConfirmRequest&) { ++OutPublishedTotal; });
		return Published;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ModeVerbsSpecPayloadTest, "GSTNModeVerbs.Core.SpecAndPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_ModeVerbsSpecPayloadTest::RunTest(const FString& Parameters)
{
	// Every native tag exists
	TArray<FGameplayTag> Declared;
	GSTNModeVerbTags::GetDeclaredTags(Declared);
	TestEqual(TEXT("ten native tags"), Declared.Num(), 10);
	for (const FGameplayTag& Tag : Declared)
	{
		TestTrue(TEXT("native tag is valid"), Tag.IsValid());
	}
	TestTrue(TEXT("verbs sit under the Ability.Mode root"), GSTNModeVerbTags::Ability_Mode_Interact.GetTag().MatchesTag(GSTNModeVerbTags::Ability_Mode));

	// Spec: an empty one is not a verb; the defaults confirm in the activation frame on the interact focus
	FGSTN_VerbSpec Spec;
	TestFalse(TEXT("a spec without a tag is not valid"), Spec.IsValid());
	Spec.VerbTag = GSTNModeVerbTags::Ability_Mode_Interact;
	TestTrue(TEXT("a spec with a tag is valid"), Spec.IsValid());
	TestEqual(TEXT("no hold by default"), Spec.HoldFrames, 0);
	TestTrue(TEXT("interact focus by default"), Spec.TargetSource == EGSTN_VerbTargetSource::InteractFocus);

	// Quantisation is the interact records' rounding
	TestTrue(TEXT("aim rounds to whole centimetres"), FGSTN_ModeVerbPayload::QuantiseAimCm(FVector(10.4, -10.6, 0.5)) == FGSTN_InteractCandidate::QuantiseCm(FVector(10.4, -10.6, 0.5)));
	TestEqual(TEXT("aim point x"), FGSTN_ModeVerbPayload::QuantiseAimCm(FVector(10.4, -10.6, 0.5)).X, 10);
	TestEqual(TEXT("aim point y"), FGSTN_ModeVerbPayload::QuantiseAimCm(FVector(10.4, -10.6, 0.5)).Y, -11);

	// Payload to the stream form and back
	FGSTN_ModeVerbPayload Payload;
	Payload.VerbTag = GSTNModeVerbTags::Ability_Mode_Interact;
	Payload.ConfirmFrame = 321;
	Payload.Magnitude = 2.f;
	FGameplayTagContainer Identity;
	Identity.AddTag(GSTNModeVerbTags::Interact_Target_Objective);

	const FGSTN_SimEvent Event = Payload.ToEvent(Identity);
	TestTrue(TEXT("event type is Message.Mode.Verb"), Event.EventType == GSTNModeVerbTags::Message_Mode_Verb);
	TestTrue(TEXT("event carries the verb tag"), Event.Tags.HasTagExact(GSTNModeVerbTags::Ability_Mode_Interact));
	TestTrue(TEXT("event carries the prop's identity tags"), Event.Tags.HasTagExact(GSTNModeVerbTags::Interact_Target_Objective));
	TestEqual(TEXT("event frame"), Event.Frame, 321);
	TestEqual(TEXT("event magnitude"), Event.Magnitude, 2.f);

	const FGSTN_ModeVerbPayload Back = FGSTN_ModeVerbPayload::FromEvent(Event);
	TestTrue(TEXT("the verb comes back (the identity tag is not mistaken for it)"), Back.VerbTag == GSTNModeVerbTags::Ability_Mode_Interact);
	TestEqual(TEXT("frame comes back"), Back.ConfirmFrame, 321);
	TestEqual(TEXT("magnitude comes back"), Back.Magnitude, 2.f);

	// A stream listener can match the verb with the usual hierarchical query
	TestTrue(TEXT("Ability.Mode matches the leaf"), Event.Tags.HasTag(GSTNModeVerbTags::Ability_Mode));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ModeVerbsOnceUnderRollbackTest, "GSTNModeVerbs.Core.OnceUnderRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_ModeVerbsOnceUnderRollbackTest::RunTest(const FString& Parameters)
{
	int32 Published = 0;

	// The authority (host): the ability confirms at frame 105; the host never re-simulates, so one flush publishes it
	{
		FGSTN_VerbConfirmQueue Queue;
		for (int32 Frame = 100; Frame <= 110; ++Frame)
		{
			if (Frame == 105)
			{
				Queue.Add(MakeRequest(7, 105));
			}
			PostTick(Queue, true, false, Frame, Published);
		}
		TestEqual(TEXT("host: exactly one authoritative event"), Published, 1);
		TestEqual(TEXT("host: nothing left pending"), Queue.NumPending(), 0);
	}

	// A rollback after the publish (a forced re-simulation of frames 103..108 appends the same confirmation again, three times over, whatever the
	// flag the flush sees): still one
	{
		Published = 0;
		FGSTN_VerbConfirmQueue Queue;
		Queue.Add(MakeRequest(7, 105));
		PostTick(Queue, true, false, 105, Published);
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			for (int32 Frame = 103; Frame <= 108; ++Frame)
			{
				if (Frame == 105)
				{
					Queue.Add(MakeRequest(7, 105));
				}
				PostTick(Queue, true, true, Frame, Published);
			}
			// The resimulation is over; the next real tick flushes
			PostTick(Queue, true, false, 109 + Pass, Published);
		}
		TestEqual(TEXT("host: re-simulated confirmations never publish again"), Published, 1);
	}

	// Rollback before the first publish: the confirmation is appended during resimulated flushes, which hold it; the real flush publishes it once
	{
		Published = 0;
		FGSTN_VerbConfirmQueue Queue;
		Queue.Add(MakeRequest(7, 205));
		Queue.Add(MakeRequest(7, 205));
		TestEqual(TEXT("a duplicate in the queue is refused at Add"), Queue.NumPending(), 1);
		PostTick(Queue, true, true, 205, Published);
		TestEqual(TEXT("a resimulated flush publishes nothing"), Published, 0);
		TestEqual(TEXT("and keeps the confirmation"), Queue.NumPending(), 1);
		PostTick(Queue, true, false, 205, Published);
		TestEqual(TEXT("the real flush publishes it once"), Published, 1);
	}

	// A client predicts the same confirmation on every pass and publishes none of them
	{
		Published = 0;
		FGSTN_VerbConfirmQueue Queue;
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Queue.Add(MakeRequest(7, 105));
			PostTick(Queue, false, Pass > 0, 105, Published);
		}
		TestEqual(TEXT("client: nothing published"), Published, 0);
		TestEqual(TEXT("client: nothing kept"), Queue.NumPending(), 0);
	}

	// Distinct confirmations stay distinct: another player on the same frame, another verb, another frame
	{
		Published = 0;
		FGSTN_VerbConfirmQueue Queue;
		Queue.Add(MakeRequest(7, 105));
		Queue.Add(MakeRequest(8, 105));
		Queue.Add(MakeRequest(7, 105, GSTNModeVerbTags::Ability_Mode_CallDrop));
		Queue.Add(MakeRequest(7, 120));
		PostTick(Queue, true, false, 110, Published);
		TestEqual(TEXT("three confirmations are due at frame 110"), Published, 3);
		PostTick(Queue, true, false, 120, Published);
		TestEqual(TEXT("the frame-120 one publishes when its frame is reached"), Published, 4);
	}

	// The queue is bounded
	{
		FGSTN_VerbConfirmQueue Queue;
		for (int32 Index = 0; Index < FGSTN_VerbConfirmQueue::MaxPending + 10; ++Index)
		{
			Queue.Add(MakeRequest(7, 1000 + Index));
		}
		TestEqual(TEXT("pending is capped"), Queue.NumPending(), FGSTN_VerbConfirmQueue::MaxPending);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ModeVerbsRequestableGateTest, "GSTNModeVerbs.Core.RequestableGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_ModeVerbsRequestableGateTest::RunTest(const FString& Parameters)
{
	FGSTN_RequestableGate::FInput Input;
	Input.ActionableFrom = 200;
	Input.ToFrame = 400;
	Input.ReachCm = 200.f;
	Input.SlackCm = 60.f;
	Input.DistanceCm = 100.f;

	// ActionableFrom: armed ahead of time, so host and clients see the same first usable frame
	Input.Frame = 199;
	TestTrue(TEXT("one frame before ActionableFrom: not actionable"), FGSTN_RequestableGate::Evaluate(Input) == EGSTN_VerbRefusal::NotActionable);
	Input.Frame = 200;
	TestTrue(TEXT("on ActionableFrom: accepted"), FGSTN_RequestableGate::Evaluate(Input) == EGSTN_VerbRefusal::None);
	Input.Frame = 399;
	TestTrue(TEXT("the last live frame: accepted"), FGSTN_RequestableGate::Evaluate(Input) == EGSTN_VerbRefusal::None);
	Input.Frame = 400;
	TestTrue(TEXT("ToFrame is exclusive: removed"), FGSTN_RequestableGate::Evaluate(Input) == EGSTN_VerbRefusal::Removed);

	// Removed wins over everything else, then the arming stamp
	Input.DistanceCm = 5000.f;
	Input.bVerbAccepted = false;
	TestTrue(TEXT("an ended record is Removed even out of reach and with the wrong verb"), FGSTN_RequestableGate::Evaluate(Input) == EGSTN_VerbRefusal::Removed);
	Input.Frame = 100;
	TestTrue(TEXT("a not-yet-armed record is NotActionable before anything else"), FGSTN_RequestableGate::Evaluate(Input) == EGSTN_VerbRefusal::NotActionable);

	// The rest, with the record live
	Input.Frame = 250;
	TestTrue(TEXT("wrong verb"), FGSTN_RequestableGate::Evaluate(Input) == EGSTN_VerbRefusal::WrongVerb);
	Input.bVerbAccepted = true;
	TestTrue(TEXT("out of reach"), FGSTN_RequestableGate::Evaluate(Input) == EGSTN_VerbRefusal::OutOfRange);
	Input.DistanceCm = 260.f;
	TestTrue(TEXT("exactly reach plus slack is in"), FGSTN_RequestableGate::Evaluate(Input) == EGSTN_VerbRefusal::None);
	Input.DistanceCm = 260.5f;
	TestTrue(TEXT("a hair beyond reach plus slack is out"), FGSTN_RequestableGate::Evaluate(Input) == EGSTN_VerbRefusal::OutOfRange);
	Input.DistanceCm = 100.f;
	Input.UsesLeft = 0;
	TestTrue(TEXT("no uses left"), FGSTN_RequestableGate::Evaluate(Input) == EGSTN_VerbRefusal::Exhausted);
	Input.UsesLeft = 2;
	TestTrue(TEXT("uses left"), FGSTN_RequestableGate::Evaluate(Input) == EGSTN_VerbRefusal::None);

	// Reach is measured to the surface
	TestEqual(TEXT("distance to the surface"), FGSTN_RequestableGate::DistanceToSurfaceCm(FVector(100, 0, 0), FIntVector(0, 0, 0), 30), 70.f);
	TestEqual(TEXT("inside the radius is zero"), FGSTN_RequestableGate::DistanceToSurfaceCm(FVector(10, 0, 0), FIntVector(0, 0, 0), 30), 0.f);
	return true;
}

#endif
