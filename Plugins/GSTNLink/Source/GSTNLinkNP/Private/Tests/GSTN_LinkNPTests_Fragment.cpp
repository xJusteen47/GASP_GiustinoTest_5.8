// Automation tests of the link sync fragment (plan F10, m6 spec 7.1 and 10): the idle entity costs the list's flag bit, a fragment at its
// defaults costs the framing and no payload, two live channels stay within the spec's byte table, every field round-trips and reconciles
// exactly, and interpolation keeps the later sample. No world: plain data and bit archives.

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Serialization/MemoryReader.h"

using namespace GSTNLinkNPTest;

namespace
{
	FGSTN_LinkState MakeLiveState(uint8 TypeIndex, uint8 Role, uint8 Serial)
	{
		FGSTN_LinkState State;
		State.LinkTypeIndex = TypeIndex;
		State.Role = Role;
		State.Phase = EGSTN_LinkPhase::Active;
		State.PartnerId = FGSTN_EntityRef(GSTNEntityKinds::Fighter, 7, 2);
		State.Serial = Serial;
		State.LinkFrame = 1200;
		State.EndFrame = 1350;
		State.Flags = GSTNLinkFlags::PendingBreak;
		State.BreakReason = static_cast<uint8>(EGSTN_LinkBreakReason::Release);
		State.Payload = 3;
		State.LocalYawQ = -1234;
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFragmentIdleTest, "GSTNLink.NP.Fragment.RoundTripIdleOneBit", Flags)
bool FGSTN_LinkFragmentIdleTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	TestTrue(TEXT("the module registered the fragment under the link feature id"),
		FGSTN_SyncFragmentRegistry::FindStruct(GSTNFeatureIds::Link) == FGSTN_LinkFragment::StaticStruct());

	// An entity that never linked has no fragment: the list is its one flag bit, as without the plugin
	FGSTN_SyncFragmentArray None;
	TestEqual(TEXT("no fragment: one bit"), BitsOfList(None), static_cast<int64>(1));

	// A fragment at its defaults writes no payload: only the list framing (flag, count, id, length)
	const FGSTN_LinkFragment Idle;
	TestTrue(TEXT("a fresh fragment is the default"), Idle.IsDefault());
	TestEqual(TEXT("the idle payload is empty"), PayloadBytes(Idle), 0);
	FGSTN_SyncFragmentArray IdleList = ListOf(Idle);
	TestEqual(TEXT("idle framing is flag + count + id + length"), BitsOfList(IdleList), static_cast<int64>(1 + 8 + 8 + 8));
	bool bError = false;
	const FGSTN_SyncFragmentArray Loaded = ReadBack(IdleList, bError);
	TestFalse(TEXT("reads back to the end without error"), bError);
	const FGSTN_LinkFragment* Read = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Loaded);
	TestTrue(TEXT("the idle fragment arrives as the default"), Read && Read->IsDefault());

	// A link that ended keeps only its serials: the presence byte, the Body header and the serial
	FGSTN_LinkFragment Ended;
	Ended.Links.Body.Serial = 9;
	TestEqual(TEXT("an ended link is 3 bytes (presence, header, serial)"), PayloadBytes(Ended), 3);

	// Reading nothing into a live fragment clears it
	FGSTN_LinkFragment Live;
	Live.Links.Body = MakeLiveState(1, 2, 4);
	TArray<uint8> NoBytes;
	FMemoryReader Reader(NoBytes);
	Live.NetSerialize(Reader);
	TestFalse(TEXT("an empty payload reads without error"), Reader.IsError());
	TestTrue(TEXT("and gives the default"), Live.IsDefault());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFragmentTwoChannelsTest, "GSTNLink.NP.Fragment.TwoChannelsBytes", Flags)
bool FGSTN_LinkFragmentTwoChannelsTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();

	FGSTN_LinkFragment Both;
	Both.Links.Body = MakeLiveState(1, 2, 5);
	Both.Links.Hand = MakeLiveState(3, 1, 200);
	// The spec's table: 21 bytes of fields per channel (type, role, phase, partner 4, serial, frame 4, end 4, flags, reason, payload, yaw 2)
	// plus the state's header byte, and the fragment's presence byte
	const int32 Bytes = PayloadBytes(Both);
	TestEqual(TEXT("two live channels are 1 + 2 x 22 bytes"), Bytes, 1 + 2 * 22);
	TestTrue(TEXT("within the spec's raw estimate per channel (21 B + header) and the presence byte"), Bytes <= 2 * (21 + 1) + 1);

	FGSTN_SyncFragmentArray List = ListOf(Both);
	bool bError = false;
	const FGSTN_SyncFragmentArray Loaded = ReadBack(List, bError);
	TestFalse(TEXT("reads back without error"), bError);
	const FGSTN_LinkFragment* Read = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Loaded);
	TestTrue(TEXT("both channels round-trip exactly"), Read && *Read == Both);
	TestFalse(TEXT("an exact copy does not reconcile"), FGSTN_SyncFragmentList::ShouldReconcile(Loaded, List));

	// Every other group too: actions, immunity and the mash frame
	FGSTN_LinkFragment Everything = Both;
	Everything.Actions.LastRefusedSerial = 4;
	Everything.Actions.RefusedFrame = 1190;
	Everything.Actions.CooldownEndFrame = 1400;
	Everything.Actions.Exit.Frame = 1360;
	Everything.Actions.Exit.Position = FVector(100.0, -50.0, 90.0);
	Everything.Actions.Exit.Velocity = FVector(600.0, 0.0, 300.0);
	Everything.GrabImmunityEndFrame = 1390;
	Everything.MashProcessedFrame = 1349;
	FGSTN_SyncFragmentArray EverythingList = ListOf(Everything);
	const FGSTN_SyncFragmentArray EverythingLoaded = ReadBack(EverythingList, bError);
	const FGSTN_LinkFragment* EverythingRead = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(EverythingLoaded);
	TestFalse(TEXT("everything reads back without error"), bError);
	TestTrue(TEXT("everything round-trips exactly"), EverythingRead && *EverythingRead == Everything);

	// One serial apart is a reconcile: the serial is what tells two links apart
	FGSTN_LinkFragment Other = Both;
	Other.Links.Hand.Serial = 201;
	FGSTN_SyncFragmentArray OtherList = ListOf(Other);
	TestTrue(TEXT("a different serial reconciles"), FGSTN_SyncFragmentList::ShouldReconcile(OtherList, List));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFragmentInterpolateTest, "GSTNLink.NP.Fragment.Interpolate", Flags)
bool FGSTN_LinkFragmentInterpolateTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();

	FGSTN_LinkFragment From;
	From.Links.Body = MakeLiveState(1, 2, 5);
	FGSTN_LinkFragment To = From;
	To.Links.Body.Phase = EGSTN_LinkPhase::Exiting;
	To.Links.Body.LinkFrame = 1210;
	const FGSTN_SyncFragmentArray FromList = ListOf(From);
	const FGSTN_SyncFragmentArray ToList = ListOf(To);

	// Discrete state: an interpolated proxy shows the later sample whatever the blend
	for (const float Alpha : { 0.f, 0.3f, 0.99f })
	{
		FGSTN_SyncFragmentArray Out;
		FGSTN_SyncFragmentList::Interpolate(Out, &FromList, ToList, Alpha);
		const FGSTN_LinkFragment* Result = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Out);
		TestTrue(FString::Printf(TEXT("alpha %.2f gives the later sample"), Alpha), Result && *Result == To);
	}

	// A link that appears between the samples appears whole on the later one, and one that ends is gone on it
	FGSTN_SyncFragmentArray Appears;
	FGSTN_SyncFragmentList::Interpolate(Appears, nullptr, ToList, 0.5f);
	TestTrue(TEXT("no earlier sample: the later one"), FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Appears) != nullptr);
	FGSTN_SyncFragmentArray Gone;
	const FGSTN_SyncFragmentArray Empty;
	FGSTN_SyncFragmentList::Interpolate(Gone, &FromList, Empty, 0.5f);
	TestTrue(TEXT("a fragment absent from the later sample is absent"), FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Gone) == nullptr);
	return true;
}

#endif
