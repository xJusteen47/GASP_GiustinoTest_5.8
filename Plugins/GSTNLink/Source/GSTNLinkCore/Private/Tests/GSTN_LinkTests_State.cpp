// Automation tests of the link plugin: link state, mash and break evaluation (plan F5b).

#include "Data/GSTN_GrabRules.h"
#include "GSTN_CombatCoreTags.h"
#include "GSTN_LinkTags.h"
#include "GSTN_LinkTestUtils.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_LinkRule_Grab.h"
#include "Rules/GSTN_LinkRule_Latch.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "State/GSTN_LinkMath.h"
#include "State/GSTN_LinkState.h"
#include "Sync/GSTN_SyncFragment.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<uint8> Write(const FGSTN_LinkState& State)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		FGSTN_LinkState Copy = State;
		bool bSuccess = false;
		Copy.NetSerialize(Writer, nullptr, bSuccess);
		return Bytes;
	}

	FGSTN_LinkState Read(const TArray<uint8>& Bytes)
	{
		FMemoryReader Reader(Bytes);
		FGSTN_LinkState State;
		bool bSuccess = false;
		State.NetSerialize(Reader, nullptr, bSuccess);
		return State;
	}

	FGSTN_LinkState MakeLiveState()
	{
		FGSTN_LinkState State;
		State.LinkTypeIndex = 3;
		State.Role = 2;
		State.Phase = EGSTN_LinkPhase::Active;
		State.PartnerId = FGSTN_EntityRef(GSTNEntityKinds::Fighter, 5, 2);
		State.Serial = 9;
		State.LinkFrame = 1234;
		State.EndFrame = 1500;
		State.Flags = GSTNLinkFlags::Predicted | GSTNLinkFlags::PendingBreak;
		State.BreakReason = static_cast<uint8>(EGSTN_LinkBreakReason::Release);
		State.Payload = 7;
		State.LocalYawQ = -1200;
		return State;
	}

	FGSTN_LinkBreakInput MakeBreakInput()
	{
		FGSTN_LinkBreakInput Input;
		Input.Frame = 100;
		Input.LinkFrame = 50;
		Input.OwnRoleTag = GSTNLinkTags::Role_Grabbed.GetTag();
		return Input;
	}
}

// ---------------------------------------------------------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkStateRoundTripTest, "GSTNLink.Core.State.RoundTrip", GSTNLinkTest::Flags)
bool FGSTN_LinkStateRoundTripTest::RunTest(const FString& Parameters)
{
	const FGSTN_LinkState Live = MakeLiveState();
	const TArray<uint8> LiveBytes = Write(Live);
	const FGSTN_LinkState Back = Read(LiveBytes);
	TestTrue(TEXT("A live state survives a round trip"), Back == Live);
	TestTrue(TEXT("Equal states have equal bytes"), Write(Back) == LiveBytes);
	TestFalse(TEXT("A live state is not idle"), Live.IsIdle());
	TestTrue(TEXT("A role in an active link is linked"), Live.IsLinked());

	// Idle costs one byte (about one bit in the fragment's bit stream); a kept serial costs one more
	const FGSTN_LinkState Idle;
	const TArray<uint8> IdleBytes = Write(Idle);
	TestEqual(TEXT("An idle state is a single byte"), IdleBytes.Num(), 1);
	TestTrue(TEXT("An idle state survives a round trip"), Read(IdleBytes) == Idle);
	TestTrue(TEXT("An idle state is idle"), Idle.IsIdle());
	TestFalse(TEXT("An idle state is not linked"), Idle.IsLinked());

	FGSTN_LinkState Ended = Live;
	Ended.Reset();
	TestTrue(TEXT("A ended link is idle again"), Ended.IsIdle());
	TestEqual(TEXT("Reset keeps the serial, so a late entry of the old link stays old"), static_cast<int32>(Ended.Serial), 9);
	const TArray<uint8> EndedBytes = Write(Ended);
	TestEqual(TEXT("An idle state with a serial is two bytes"), EndedBytes.Num(), 2);
	TestTrue(TEXT("It survives a round trip"), Read(EndedBytes) == Ended);

	// Every field is carried: change one at a time and see the difference survive
	for (int32 Field = 0; Field < 10; ++Field)
	{
		FGSTN_LinkState Changed = Live;
		switch (Field)
		{
		case 0: Changed.LinkTypeIndex = 4; break;
		case 1: Changed.Role = 1; break;
		case 2: Changed.Phase = EGSTN_LinkPhase::Exiting; break;
		case 3: Changed.PartnerId = FGSTN_EntityRef(GSTNEntityKinds::Object, 8, 1); break;
		case 4: Changed.LinkFrame = 99; break;
		case 5: Changed.EndFrame = 0; break;
		case 6: Changed.Flags = 0; break;
		case 7: Changed.BreakReason = 3; break;
		case 8: Changed.Payload = 0; break;
		default: Changed.LocalYawQ = 32767; break;
		}
		TestTrue(FString::Printf(TEXT("Changing field %d survives"), Field), Read(Write(Changed)) == Changed);
		TestFalse(FString::Printf(TEXT("Changing field %d is seen"), Field), Changed == Live);
	}

	// The generic action block is idle by default
	FGSTN_LinkActions Actions;
	TestTrue(TEXT("A fresh action block is idle"), Actions.IsIdle());
	Actions.Exit.Frame = 12;
	TestFalse(TEXT("An exit makes it busy"), Actions.IsIdle());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkStateSerialIdempotenceTest, "GSTNLink.Core.State.SerialIdempotence", GSTNLinkTest::Flags)
bool FGSTN_LinkStateSerialIdempotenceTest::RunTest(const FString& Parameters)
{
	// The serial circle: later means within half a turn ahead
	TestTrue(TEXT("5 is later than 4"), FGSTN_LinkState::IsSerialNewer(5, 4));
	TestFalse(TEXT("4 is not later than 5"), FGSTN_LinkState::IsSerialNewer(4, 5));
	TestFalse(TEXT("A serial is not later than itself"), FGSTN_LinkState::IsSerialNewer(5, 5));
	TestTrue(TEXT("It wraps: 3 is later than 250"), FGSTN_LinkState::IsSerialNewer(3, 250));
	TestFalse(TEXT("It wraps: 250 is not later than 3"), FGSTN_LinkState::IsSerialNewer(250, 3));

	// A new link takes one after the newer serial and never 0
	TestEqual(TEXT("From nothing"), static_cast<int32>(FGSTN_LinkState::NextSerial(0, 0)), 1);
	TestEqual(TEXT("One after the newer of two"), static_cast<int32>(FGSTN_LinkState::NextSerial(3, 5)), 6);
	TestEqual(TEXT("The order of the arguments does not matter"), static_cast<int32>(FGSTN_LinkState::NextSerial(5, 3)), 6);
	TestEqual(TEXT("It skips 0 when the circle wraps"), static_cast<int32>(FGSTN_LinkState::NextSerial(255, 255)), 1);
	TestEqual(TEXT("It follows the circle, not the number"), static_cast<int32>(FGSTN_LinkState::NextSerial(250, 3)), 4);

	// A formation entry applies once: the state then carries its serial and the same entry is ignored
	FGSTN_LinkState State;
	State.Serial = 4;
	TestTrue(TEXT("A newer formation entry applies"), State.IsFormationNew(5));
	State.Serial = 5;
	State.Phase = EGSTN_LinkPhase::Active;
	State.Role = 2;
	TestFalse(TEXT("The same entry applied twice does nothing"), State.IsFormationNew(5));
	TestFalse(TEXT("An older entry is ignored"), State.IsFormationNew(4));
	TestTrue(TEXT("The entry after it applies"), State.IsFormationNew(6));

	// Rollback: the state is restored to the older link while the newer entry re-arrives from the re-simulated sender
	FGSTN_LinkState Restored;
	Restored.Serial = 4;
	Restored.Phase = EGSTN_LinkPhase::Active;
	TestTrue(TEXT("A restored older state processes the newer entry again"), Restored.IsFormationNew(5));

	// A break applies only to the live link of that very serial
	TestTrue(TEXT("A break for the live link matches"), State.MatchesBreak(5));
	TestFalse(TEXT("A break for an older link is ignored"), State.MatchesBreak(4));
	TestFalse(TEXT("A break for a newer link is ignored"), State.MatchesBreak(6));
	State.Reset();
	TestFalse(TEXT("A break for a link that already ended is ignored"), State.MatchesBreak(5));
	TestFalse(TEXT("And the old formation stays old"), State.IsFormationNew(5));

	// Wraparound end to end
	FGSTN_LinkState Wrapped;
	Wrapped.Serial = 255;
	TestTrue(TEXT("After 255 the next formation is new"), Wrapped.IsFormationNew(FGSTN_LinkState::NextSerial(255, 255)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkStateTwoChannelsTest, "GSTNLink.Core.State.TwoChannels", GSTNLinkTest::Flags)
bool FGSTN_LinkStateTwoChannelsTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkStates States;
	States.Body = MakeLiveState();
	States.Get(EGSTN_LinkChannel::Hand).LinkTypeIndex = 8;
	States.Get(EGSTN_LinkChannel::Hand).Role = 1;
	States.Get(EGSTN_LinkChannel::Hand).Phase = EGSTN_LinkPhase::Active;

	TestEqual(TEXT("The Body slot is the Body slot"), static_cast<int32>(States.Get(EGSTN_LinkChannel::Body).LinkTypeIndex), 3);
	TestEqual(TEXT("The Hand slot is the Hand slot"), static_cast<int32>(States.Get(EGSTN_LinkChannel::Hand).LinkTypeIndex), 8);

	// The slots are independent: ending one leaves the other
	States.Get(EGSTN_LinkChannel::Hand).Reset();
	TestTrue(TEXT("The Hand slot is idle"), States.Hand.IsIdle());
	TestTrue(TEXT("The Body slot is untouched"), States.Body == MakeLiveState());
	FGSTN_LinkStates Other = States;
	TestTrue(TEXT("Equal pairs are equal"), Other == States);
	Other.Hand.Payload = 1;
	TestFalse(TEXT("A difference in either slot is seen"), Other == States);

	// Which type may coexist with which is data (m6 spec 7.0 item 2), decided by the shared request checks
	UGSTN_LinkTypeDefinition* Grab = GSTNLinkTest::MakeGrabDefinition();
	UGSTN_LinkTypeDefinition* Hand = GSTNLinkTest::MakeLatchDefinition();
	Hand->Channel = EGSTN_LinkChannel::Hand;
	Hand->InitiatorRole.bParked = false;

	// A fighter already in a grab may start a Hand link
	FGSTN_LinkRequestContext HandContext = GSTNLinkTest::MakeContext(Hand);
	HandContext.Initiator.Links.Body = MakeLiveState();
	HandContext.Initiator.OtherChannelDefinition = Grab;
	TestTrue(TEXT("A Hand link coexists with a Body link by default"), UGSTN_LinkRule::CheckCommon(HandContext).bAccepted);

	// ... unless the Hand type forbids that Body type
	Hand->ForbiddenWithBody.Add(GSTNLinkTags::Type_Grab.GetTag());
	const FGSTN_LinkRuleResult Forbidden = UGSTN_LinkRule::CheckCommon(HandContext);
	TestFalse(TEXT("It is refused when the Hand type forbids the Body type"), Forbidden.bAccepted);
	TestEqual(TEXT("The reason is the coexistence"), static_cast<int32>(Forbidden.Refusal), static_cast<int32>(EGSTN_LinkRefusal::CoexistForbidden));

	// The Hand link occupies its own slot only: a grab on that fighter is refused by the type's list when seen from the Body side too
	FGSTN_LinkRequestContext GrabContext = GSTNLinkTest::MakeContext(Grab);
	GrabContext.Target.Links.Hand = States.Body;
	GrabContext.Target.Links.Hand.Phase = EGSTN_LinkPhase::Active;
	GrabContext.Target.OtherChannelDefinition = Hand;
	TestFalse(TEXT("A Body request on a fighter whose Hand link forbids it is refused"), UGSTN_LinkRule::CheckCommon(GrabContext).bAccepted);
	GrabContext.Target.OtherChannelDefinition = nullptr;
	TestTrue(TEXT("Without that link it is accepted"), UGSTN_LinkRule::CheckCommon(GrabContext).bAccepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkStateParkedInitiatorTransformTest, "GSTNLink.Core.State.ParkedInitiatorTransform", GSTNLinkTest::Flags)
bool FGSTN_LinkStateParkedInitiatorTransformTest::RunTest(const FString& Parameters)
{
	const FTransform Offset(FRotator(0.0, 90.0, 0.0), FVector(0.0, -30.0, 50.0));
	const FTransform Partner(FRotator(0.0, 45.0, 0.0), FVector(1000.0, 200.0, 0.0));
	const FTransform Socket(FRotator(0.0, 0.0, 0.0), FVector(10.0, 0.0, 120.0));

	// A latch parks role 1, the initiator, on the enemy
	UGSTN_LinkTypeDefinition* Latch = GSTNLinkTest::MakeLatchDefinition();
	Latch->InitiatorRole.SocketOffset = Offset;
	FTransform Parked;
	TestTrue(TEXT("The latch's role 1 is the parked one"), UGSTN_LinkMath::TryGetParkedTransform(*Latch, 1, Partner, FTransform::Identity, Parked));
	TestTrue(TEXT("Its placement is the offset in the partner's space"), Parked.Equals(Offset * Partner, 0.001));
	TestTrue(TEXT("The two-argument form is the same"), UGSTN_LinkMath::ParkedTransform(Partner, Latch->InitiatorRole).Equals(Parked, 0.0));
	FTransform Unused;
	TestFalse(TEXT("The enemy (role 2) is not parked"), UGSTN_LinkMath::TryGetParkedTransform(*Latch, 2, Partner, FTransform::Identity, Unused));

	// A grab parks role 2: the same derivation for the other role
	UGSTN_LinkTypeDefinition* Grab = GSTNLinkTest::MakeGrabDefinition();
	Grab->TargetRole.SocketOffset = Offset;
	FTransform GrabParked;
	TestTrue(TEXT("The grab's role 2 is the parked one"), UGSTN_LinkMath::TryGetParkedTransform(*Grab, 2, Partner, FTransform::Identity, GrabParked));
	TestFalse(TEXT("The grabber (role 1) is not parked"), UGSTN_LinkMath::TryGetParkedTransform(*Grab, 1, Partner, FTransform::Identity, Unused));
	TestTrue(TEXT("Role 1 of a latch and role 2 of a grab with one offset are placed alike"), GSTNLinkTest::ExactlyEqual(Parked, GrabParked));
	TestFalse(TEXT("Role numbers outside 1 and 2 are not parked"), UGSTN_LinkMath::TryGetParkedTransform(*Grab, 0, Partner, FTransform::Identity, Unused));
	TestFalse(TEXT("Role numbers outside 1 and 2 are not parked (3)"), UGSTN_LinkMath::TryGetParkedTransform(*Grab, 3, Partner, FTransform::Identity, Unused));

	// With the partner's baked socket: offset in socket space, socket in root space, root in the world
	FTransform OnSocket;
	TestTrue(TEXT("A parked role is placed on a baked socket"), UGSTN_LinkMath::TryGetParkedTransform(*Latch, 1, Partner, Socket, OnSocket));
	TestTrue(TEXT("Offset x socket x partner"), OnSocket.Equals(Offset * Socket * Partner, 0.001));
	TestFalse(TEXT("The socket moves the placement"), OnSocket.Equals(Parked, 0.001));

	// The placement is a function of the partner's transform and the definition only
	const FTransform Again = UGSTN_LinkMath::ParkedTransformOnSocket(Partner, Socket, Latch->InitiatorRole);
	TestTrue(TEXT("Equal inputs give bit-equal outputs"), GSTNLinkTest::ExactlyEqual(Again, OnSocket));
	const FTransform Moved(Partner.GetRotation(), Partner.GetLocation() + FVector(0.0, 0.0, 10.0));
	FTransform MovedParked;
	UGSTN_LinkMath::TryGetParkedTransform(*Latch, 1, Moved, FTransform::Identity, MovedParked);
	TestTrue(TEXT("It follows the partner"), FMath::IsNearlyEqual(MovedParked.GetLocation().Z - Parked.GetLocation().Z, 10.0, 0.001));
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Mash
// ---------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkMashInitialFramesTest, "GSTNLink.Core.Mash.InitialFramesByDamage", GSTNLinkTest::Flags)
bool FGSTN_LinkMashInitialFramesTest::RunTest(const FString& Parameters)
{
	FGSTN_MashRule Rule;
	Rule.BaseFrames = 100;
	Rule.FramesPerDamagePercent = 1.f;
	Rule.MinFrames = 60;
	Rule.MaxFrames = 300;

	TestEqual(TEXT("No damage: the base"), UGSTN_LinkMath::MashInitialFrames(Rule, 0.f), 100);
	TestEqual(TEXT("50 percent adds 50 frames"), UGSTN_LinkMath::MashInitialFrames(Rule, 50.f), 150);
	TestEqual(TEXT("A lot of damage is capped"), UGSTN_LinkMath::MashInitialFrames(Rule, 400.f), 300);
	TestEqual(TEXT("Negative damage counts as none"), UGSTN_LinkMath::MashInitialFrames(Rule, -40.f), 100);

	// More damage never shortens the grab
	int32 Previous = 0;
	bool bMonotonic = true;
	for (int32 Damage = 0; Damage <= 500; Damage += 10)
	{
		const int32 Frames = UGSTN_LinkMath::MashInitialFrames(Rule, static_cast<float>(Damage));
		bMonotonic &= Frames >= Previous;
		Previous = Frames;
	}
	TestTrue(TEXT("The duration never falls as damage rises"), bMonotonic);

	// The floor
	Rule.BaseFrames = 10;
	TestEqual(TEXT("The minimum holds"), UGSTN_LinkMath::MashInitialFrames(Rule, 0.f), 60);

	// The resistance scales the clamped value
	Rule.BaseFrames = 100;
	TestEqual(TEXT("Resistance 2 doubles the grab"), UGSTN_LinkMath::MashInitialFrames(Rule, 0.f, 2.f), 200);
	TestEqual(TEXT("Resistance scales the capped value too"), UGSTN_LinkMath::MashInitialFrames(Rule, 400.f, 2.f), 600);
	TestEqual(TEXT("Resistance 0.5 halves it"), UGSTN_LinkMath::MashInitialFrames(Rule, 0.f, 0.5f), 50);
	TestEqual(TEXT("A grab is never shorter than a frame"), UGSTN_LinkMath::MashInitialFrames(Rule, 0.f, 0.f), 1);

	// An inverted range does not break the math: the maximum rises to the minimum
	Rule.MinFrames = 200;
	Rule.MaxFrames = 100;
	TestEqual(TEXT("Max below min is read as min"), UGSTN_LinkMath::MashInitialFrames(Rule, 0.f), 200);

	// Equal inputs, equal output
	TestEqual(TEXT("Deterministic"), UGSTN_LinkMath::MashInitialFrames(Rule, 37.5f, 1.25f), UGSTN_LinkMath::MashInitialFrames(Rule, 37.5f, 1.25f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkMashProgressTest, "GSTNLink.Core.Mash.ProgressAndCap", GSTNLinkTest::Flags)
bool FGSTN_LinkMashProgressTest::RunTest(const FString& Parameters)
{
	FGSTN_MashRule Rule;
	Rule.ReducePerPress = 6;

	TestEqual(TEXT("One press moves the end 6 frames earlier"), UGSTN_LinkMath::MashProgress(Rule, 200, 50, 1, 0), 194);
	TestEqual(TEXT("Five presses move it 30"), UGSTN_LinkMath::MashProgress(Rule, 200, 50, 5, 0), 170);
	TestEqual(TEXT("No press leaves it"), UGSTN_LinkMath::MashProgress(Rule, 200, 50, 0, 0), 200);

	// The cap: presses never pull the end before the current frame
	const int32 Capped = UGSTN_LinkMath::MashProgress(Rule, 200, 50, 100, 0);
	TestEqual(TEXT("A huge burst stops at the current frame"), Capped, 50);
	TestTrue(TEXT("... which is an escape now"), UGSTN_LinkMath::MashEscaped(Capped, 50));
	TestFalse(TEXT("A pull to the frame before is not before the frame"), UGSTN_LinkMath::MashProgress(Rule, 200, 50, 100, 0) < 50);

	// An end frame that is already past does not move
	TestEqual(TEXT("A past end stays"), UGSTN_LinkMath::MashProgress(Rule, 40, 50, 3, 0), 40);

	// No end frame, no mash
	TestEqual(TEXT("0 means no mash end and stays 0"), UGSTN_LinkMath::MashProgress(Rule, 0, 50, 9, 0), 0);

	// Nonsense input is harmless
	TestEqual(TEXT("Negative presses count as none"), UGSTN_LinkMath::MashProgress(Rule, 200, 50, -4, 0), 200);
	Rule.ReducePerPress = 0;
	TestEqual(TEXT("A rule that reduces nothing leaves the end"), UGSTN_LinkMath::MashProgress(Rule, 200, 50, 9, 0), 200);
	Rule.ReducePerPress = 6;
	TestEqual(TEXT("A huge count does not overflow"), UGSTN_LinkMath::MashProgress(Rule, 200, 50, MAX_int32, 0), 50);

	// Escaping
	TestFalse(TEXT("One frame before the end"), UGSTN_LinkMath::MashEscaped(100, 99));
	TestTrue(TEXT("On the end frame"), UGSTN_LinkMath::MashEscaped(100, 100));
	TestTrue(TEXT("After the end frame"), UGSTN_LinkMath::MashEscaped(100, 150));
	TestFalse(TEXT("No end, no escape"), UGSTN_LinkMath::MashEscaped(0, 999));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkMashFlicksTest, "GSTNLink.Core.Mash.FlicksCount", GSTNLinkTest::Flags)
bool FGSTN_LinkMashFlicksTest::RunTest(const FString& Parameters)
{
	FGSTN_MashRule Rule;
	Rule.ReducePerPress = 6;
	Rule.bCountFlicks = true;

	TestEqual(TEXT("A flick counts like a press"), UGSTN_LinkMath::MashProgress(Rule, 200, 50, 0, 1), 194);
	TestEqual(TEXT("Presses and flicks add up"), UGSTN_LinkMath::MashProgress(Rule, 200, 50, 1, 2), 182);
	TestEqual(TEXT("Flicks alone move the end"), UGSTN_LinkMath::MashProgress(Rule, 200, 50, 0, 5), 170);

	Rule.bCountFlicks = false;
	TestEqual(TEXT("Without flicks only presses count"), UGSTN_LinkMath::MashProgress(Rule, 200, 50, 1, 2), 194);
	TestEqual(TEXT("Flicks alone do nothing"), UGSTN_LinkMath::MashProgress(Rule, 200, 50, 0, 7), 200);

	// The flicks are capped like presses
	Rule.bCountFlicks = true;
	TestEqual(TEXT("A burst of flicks stops at the current frame"), UGSTN_LinkMath::MashProgress(Rule, 200, 50, 0, 1000), 50);

	// The buttons are data: tags, empty = any button, compiled to a mask by the host
	TestTrue(TEXT("A new rule counts any button"), FGSTN_MashRule().Buttons.IsEmpty());
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Break
// ---------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkBreakTimeoutTest, "GSTNLink.Core.Break.Timeout", GSTNLinkTest::Flags)
bool FGSTN_LinkBreakTimeoutTest::RunTest(const FString& Parameters)
{
	const FGSTN_LinkBreakRules Rules;
	FGSTN_LinkBreakInput Input = MakeBreakInput();

	// The link formed on frame 50 and may last 60 frames
	Input.Frame = 109;
	TestFalse(TEXT("59 frames in: no timeout"), UGSTN_LinkMath::EvaluateBreak(Rules, 60, Input).bBreak);
	Input.Frame = 110;
	const FGSTN_LinkBreakDecision Decision = UGSTN_LinkMath::EvaluateBreak(Rules, 60, Input);
	TestTrue(TEXT("60 frames in: timeout"), Decision.bBreak);
	TestEqual(TEXT("The reason is Timeout"), static_cast<int32>(Decision.Reason), static_cast<int32>(EGSTN_LinkBreakReason::Timeout));
	Input.Frame = 5000;
	TestFalse(TEXT("MaxFrames 0 never times out"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);

	// The mash end is Escape, and it wins over the timeout when both are due
	Input.Frame = 110;
	Input.EndFrame = 110;
	TestEqual(TEXT("The mash end is an Escape"), static_cast<int32>(UGSTN_LinkMath::EvaluateBreak(Rules, 60, Input).Reason), static_cast<int32>(EGSTN_LinkBreakReason::Escape));
	Input.EndFrame = 111;
	TestEqual(TEXT("Before the mash end the timeout still applies"), static_cast<int32>(UGSTN_LinkMath::EvaluateBreak(Rules, 60, Input).Reason), static_cast<int32>(EGSTN_LinkBreakReason::Timeout));

	// A quiet link is not broken
	Input = MakeBreakInput();
	TestFalse(TEXT("Nothing due, nothing breaks"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkBreakDistanceTest, "GSTNLink.Core.Break.Distance", GSTNLinkTest::Flags)
bool FGSTN_LinkBreakDistanceTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkBreakRules Rules;
	Rules.MaxDistanceCm = 300.f;
	FGSTN_LinkBreakInput Input = MakeBreakInput();

	Input.PartnerDistanceCm = 300.f;
	TestFalse(TEXT("Exactly at the limit: no break"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);
	Input.PartnerDistanceCm = 300.5f;
	const FGSTN_LinkBreakDecision Decision = UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input);
	TestTrue(TEXT("Beyond the limit: break"), Decision.bBreak);
	TestEqual(TEXT("The reason is Distance"), static_cast<int32>(Decision.Reason), static_cast<int32>(EGSTN_LinkBreakReason::Distance));

	Rules.MaxDistanceCm = 0.f;
	Input.PartnerDistanceCm = 100000.f;
	TestFalse(TEXT("MaxDistanceCm 0 never breaks by distance"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkBreakHitByRoleTest, "GSTNLink.Core.Break.HitByRole", GSTNLinkTest::Flags)
bool FGSTN_LinkBreakHitByRoleTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkBreakRules Rules;
	FGSTN_RoleHitBreak& HitBreak = Rules.HitBreaks.AddDefaulted_GetRef();
	HitBreak.RoleTag = GSTNLinkTags::Role_Grabbed.GetTag();
	HitBreak.MinKnockbackToBreak = 200.f;

	FGSTN_LinkBreakInput Input = MakeBreakInput();
	Input.Hit.bHit = true;

	Input.Hit.Knockback = 250.f;
	const FGSTN_LinkBreakDecision Decision = UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input);
	TestTrue(TEXT("A strong hit on the role breaks the link"), Decision.bBreak);
	TestEqual(TEXT("The reason is Hit"), static_cast<int32>(Decision.Reason), static_cast<int32>(EGSTN_LinkBreakReason::Hit));
	Input.Hit.Knockback = 200.f;
	TestTrue(TEXT("A hit exactly at the threshold breaks it"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);
	Input.Hit.Knockback = 150.f;
	TestFalse(TEXT("A weak hit does not"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);

	// A role with no rule is never broken by a hit
	Input.Hit.Knockback = 9999.f;
	Input.OwnRoleTag = GSTNLinkTags::Role_Grabber.GetTag();
	TestFalse(TEXT("The grabber has no hit rule: no break"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);

	// No hit, no break
	Input.OwnRoleTag = GSTNLinkTags::Role_Grabbed.GetTag();
	Input.Hit.bHit = false;
	TestFalse(TEXT("Knockback without a hit is nothing"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);

	// The same rule serves either role of another type
	FGSTN_LinkBreakRules LatchRules;
	FGSTN_RoleHitBreak& LatchBreak = LatchRules.HitBreaks.AddDefaulted_GetRef();
	LatchBreak.RoleTag = GSTNLinkTags::Role_Latcher.GetTag();
	FGSTN_LinkBreakInput LatchInput = MakeBreakInput();
	LatchInput.OwnRoleTag = GSTNLinkTags::Role_Latcher.GetTag();
	LatchInput.Hit.bHit = true;
	LatchInput.Hit.Knockback = 1.f;
	TestTrue(TEXT("Any hit on a latcher breaks a rule with threshold 0"), UGSTN_LinkMath::EvaluateBreak(LatchRules, 0, LatchInput).bBreak);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkBreakPartnerHitsNeverBreakTest, "GSTNLink.Core.Break.PartnerHitsNeverBreak", GSTNLinkTest::Flags)
bool FGSTN_LinkBreakPartnerHitsNeverBreakTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkBreakRules Rules;
	FGSTN_RoleHitBreak& HitBreak = Rules.HitBreaks.AddDefaulted_GetRef();
	HitBreak.RoleTag = GSTNLinkTags::Role_Grabbed.GetTag();
	HitBreak.MinKnockbackToBreak = 0.f;

	FGSTN_LinkBreakInput Input = MakeBreakInput();
	Input.Hit.bHit = true;
	Input.Hit.Knockback = 9999.f;
	Input.Hit.bFromPartner = true;

	TestFalse(TEXT("The grabber's own hit, however strong, keeps the link (a pummel)"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);
	Input.Hit.bFromPartner = false;
	TestTrue(TEXT("The same hit from a third party breaks it"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);

	// Unless the rule opts in
	HitBreak.bPartnerHitsBreak = true;
	Input.Hit.bFromPartner = true;
	TestTrue(TEXT("With bPartnerHitsBreak the partner's hit breaks it"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);
	Input.Hit.Knockback = 0.f;
	TestTrue(TEXT("... at threshold 0 even with no knockback"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);

	// A partner's hit does not stop another reason from applying
	HitBreak.bPartnerHitsBreak = false;
	Input.Hit.Knockback = 9999.f;
	Input.bPartnerDead = true;
	TestEqual(TEXT("Other reasons still apply"), static_cast<int32>(UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).Reason), static_cast<int32>(EGSTN_LinkBreakReason::PartnerDeath));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkBreakPartnerStateTest, "GSTNLink.Core.Break.PartnerState", GSTNLinkTest::Flags)
bool FGSTN_LinkBreakPartnerStateTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkBreakRules Rules;
	Rules.PartnerMustNot.AddTag(GSTNCombatTags::State_Hitstun);

	FGSTN_LinkBreakInput Input = MakeBreakInput();
	TestFalse(TEXT("A partner in no forbidden state: no break"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);

	Input.PartnerStateTags.AddTag(GSTNCombatTags::State_Busy);
	TestFalse(TEXT("A state the rule does not name: no break"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);

	Input.PartnerStateTags.AddTag(GSTNCombatTags::State_Hitstun);
	const FGSTN_LinkBreakDecision Decision = UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input);
	TestTrue(TEXT("A partner in hitstun: break"), Decision.bBreak);
	TestEqual(TEXT("The reason is PartnerState"), static_cast<int32>(Decision.Reason), static_cast<int32>(EGSTN_LinkBreakReason::PartnerState));

	Rules.PartnerMustNot.Reset();
	TestFalse(TEXT("A rule that forbids nothing never breaks"), UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).bBreak);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkBreakForcedTest, "GSTNLink.Core.Break.Forced", GSTNLinkTest::Flags)
bool FGSTN_LinkBreakForcedTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkBreakRules Rules;
	Rules.MaxDistanceCm = 100.f;
	Rules.PartnerMustNot.AddTag(GSTNCombatTags::State_Hitstun);
	FGSTN_RoleHitBreak& HitBreak = Rules.HitBreaks.AddDefaulted_GetRef();
	HitBreak.RoleTag = GSTNLinkTags::Role_Grabbed.GetTag();

	// A pending break without a reason is Forced; with one, it keeps its reason (Release, Escape...)
	FGSTN_LinkBreakInput Input = MakeBreakInput();
	Input.bPendingBreak = true;
	TestEqual(TEXT("No reason: Forced"), static_cast<int32>(UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).Reason), static_cast<int32>(EGSTN_LinkBreakReason::Forced));
	Input.PendingReason = EGSTN_LinkBreakReason::Release;
	TestEqual(TEXT("A reason is kept"), static_cast<int32>(UGSTN_LinkMath::EvaluateBreak(Rules, 0, Input).Reason), static_cast<int32>(EGSTN_LinkBreakReason::Release));

	// The order of the reasons when several are due at once, peeled from the top
	Input.PendingReason = EGSTN_LinkBreakReason::Forced;
	Input.bOwnDead = true;
	Input.bPartnerDead = true;
	Input.PartnerStateTags.AddTag(GSTNCombatTags::State_Hitstun);
	Input.Hit.bHit = true;
	Input.PartnerDistanceCm = 500.f;
	Input.EndFrame = 100;
	TArray<EGSTN_LinkBreakReason> Order;
	for (int32 Step = 0; Step < 8; ++Step)
	{
		const FGSTN_LinkBreakDecision Decision = UGSTN_LinkMath::EvaluateBreak(Rules, 10, Input);
		if (!Decision.bBreak)
		{
			break;
		}
		Order.Add(Decision.Reason);
		switch (Step)
		{
		case 0: Input.bPendingBreak = false; break;
		case 1: Input.bOwnDead = false; break;
		case 2: Input.bPartnerDead = false; break;
		case 3: Input.PartnerStateTags.Reset(); break;
		case 4: Input.Hit.bHit = false; break;
		case 5: Input.PartnerDistanceCm = 0.f; break;
		default: Input.EndFrame = 0; break;
		}
	}
	const TArray<EGSTN_LinkBreakReason> Expected = {
		EGSTN_LinkBreakReason::Forced, EGSTN_LinkBreakReason::OwnDeath, EGSTN_LinkBreakReason::PartnerDeath, EGSTN_LinkBreakReason::PartnerState,
		EGSTN_LinkBreakReason::Hit, EGSTN_LinkBreakReason::Distance, EGSTN_LinkBreakReason::Escape, EGSTN_LinkBreakReason::Timeout };
	TestTrue(TEXT("Forced, OwnDeath, PartnerDeath, PartnerState, Hit, Distance, Escape, Timeout"), Order == Expected);

	// Death can be switched off per rule
	FGSTN_LinkBreakRules Immortal;
	Immortal.bBreakOnOwnDeath = false;
	Immortal.bBreakOnPartnerDeath = false;
	FGSTN_LinkBreakInput Dead = MakeBreakInput();
	Dead.bOwnDead = true;
	Dead.bPartnerDead = true;
	TestFalse(TEXT("A rule can ignore deaths"), UGSTN_LinkMath::EvaluateBreak(Immortal, 0, Dead).bBreak);

	// The reasons have tags and come back from them; a plugin's own reason has none here
	for (uint8 Index = static_cast<uint8>(EGSTN_LinkBreakReason::ShakeOff); Index <= static_cast<uint8>(EGSTN_LinkBreakReason::Forced); ++Index)
	{
		const EGSTN_LinkBreakReason Reason = static_cast<EGSTN_LinkBreakReason>(Index);
		const FGameplayTag Tag = UGSTN_LinkMath::BreakReasonTag(Reason);
		TestTrue(FString::Printf(TEXT("Reason %d has a tag"), Index), Tag.IsValid());
		TestEqual(FString::Printf(TEXT("Reason %d comes back from its tag"), Index), static_cast<int32>(UGSTN_LinkMath::BreakReasonFromTag(Tag)), static_cast<int32>(Reason));
	}
	TestFalse(TEXT("None has no tag"), UGSTN_LinkMath::BreakReasonTag(EGSTN_LinkBreakReason::None).IsValid());
	TestFalse(TEXT("A plugin's reason has no tag here"), UGSTN_LinkMath::BreakReasonTag(EGSTN_LinkBreakReason::FirstPlugin).IsValid());
	TestEqual(TEXT("An unrelated tag is no reason"), static_cast<int32>(UGSTN_LinkMath::BreakReasonFromTag(GSTNLinkTags::Role_Grabber.GetTag())), static_cast<int32>(EGSTN_LinkBreakReason::None));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
