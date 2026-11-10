// Held objects, simulation side tests (GSTNHeld.Runtime.*): the holder's fragment and its inbox command.

#include "GSTN_HeldSim.h"
#include "Misc/AutomationTest.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Simulation/GSTN_CombatInbox.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldFragmentTest, "GSTNHeld.Runtime.HandFragment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldFragmentTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("fragment registered under Held"), FGSTN_SyncFragmentRegistry::FindStruct(GSTNFeatureIds::Held) == FGSTN_HeldFragment::StaticStruct());
	TestNotNull(TEXT("inbox kind registered"), FGSTN_InboxKindRegistry::Get().Find(GSTNHeldInboxKinds::Command));

	FGSTN_HeldFragment Hand;
	FGSTN_HeldCommandEntry Hold;
	Hold.bHold = true;
	Hold.ObjectRef = 3;
	Hold.Serial = 7;
	FGSTN_HeldInbox::Apply(Hand, Hold, 100);
	TestTrue(TEXT("hold fills the hand"), Hand.IsHolding() && Hand.ObjectRef == 3 && Hand.Serial == 7 && Hand.HoldFrame == 100);

	// A late release of an older hold of the same object leaves the newer hold alone
	FGSTN_HeldCommandEntry Stale = Hold;
	Stale.bHold = false;
	Stale.Serial = 6;
	FGSTN_HeldInbox::Apply(Hand, Stale, 101);
	TestTrue(TEXT("stale release ignored"), Hand.IsHolding());

	FGSTN_HeldCommandEntry Other = Hold;
	Other.bHold = false;
	Other.ObjectRef = 4;
	FGSTN_HeldInbox::Apply(Hand, Other, 101);
	TestTrue(TEXT("release of another object ignored"), Hand.IsHolding());

	FGSTN_HeldCommandEntry Release = Hold;
	Release.bHold = false;
	FGSTN_HeldInbox::Apply(Hand, Release, 102);
	TestTrue(TEXT("matching release empties the hand"), !Hand.IsHolding() && Hand.HoldFrame == INDEX_NONE && Hand.Serial == 7);

	// Wire round trip and reconcile
	FGSTN_HeldInbox::Apply(Hand, Hold, 110);
	FBufferArchive Writer;
	Hand.NetSerialize(Writer);
	FMemoryReader Reader(Writer);
	FGSTN_HeldFragment Read;
	Read.NetSerialize(Reader);
	TestTrue(TEXT("round trip"), Read == Hand);
	TestFalse(TEXT("equal: no reconcile"), Read.ShouldReconcile(Hand));
	Read.Serial = 8;
	TestTrue(TEXT("different serial: reconcile"), Read.ShouldReconcile(Hand));
	return true;
}

#endif
