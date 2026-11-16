// Automation tests of the backpack's pure simulation (M11 L7b): memory slots (spend, refills, empty tags), the dome's lifetimes and the
// fragment's wire form. No world.

#include "Misc/AutomationTest.h"
#include "GSTN_BackpackSim.h"
#include "GSTN_BackpackTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags BackpackSimTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FGSTN_BackpackChargeTrack MakeCharge(int32 Slot, int32 Charges, EGSTN_BackpackRefill Refill, int32 RefillFrames = 10)
	{
		FGSTN_BackpackChargeTrack Track;
		Track.Slot = Slot;
		Track.Charges = Charges;
		Track.Refill = Refill;
		Track.RefillFrames = RefillFrames;
		return Track;
	}

	bool HasTag(const FGSTN_BackpackFragment& Fragment, const FGameplayTag& Tag)
	{
		FGameplayTagContainer Tags;
		FGSTN_BackpackStep::GatherStateTags(&Fragment, Tags);
		return Tags.HasTagExact(Tag);
	}

	FGSTN_BackpackFragment RoundTrip(const FGSTN_BackpackFragment& In, int32* OutBytes = nullptr)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		FGSTN_BackpackFragment Sent = In;
		Sent.NetSerialize(Writer);
		if (OutBytes)
		{
			*OutBytes = Bytes.Num();
		}
		FMemoryReader Reader(Bytes);
		FGSTN_BackpackFragment Received;
		Received.NetSerialize(Reader);
		return Received;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackMemoryOverTimeTest, "GSTNBackpack.Core.Memory.OverTime", BackpackSimTestFlags)
bool FGSTNBackpackMemoryOverTimeTest::RunTest(const FString& Parameters)
{
	FGSTN_BackpackFragment Fragment;
	const FGSTN_BackpackChargeTrack Track = MakeCharge(0, 2, EGSTN_BackpackRefill::OverTime, 10);
	TestTrue(TEXT("first spend has a charge"), FGSTN_BackpackStep::Spend(Fragment, 100, Track));
	TestEqual(TEXT("one left"), FGSTN_BackpackStep::ChargesLeft(Fragment, 0), 1);
	TestFalse(TEXT("not empty"), HasTag(Fragment, GSTNBackpackTags::State_Empty_Slot0.GetTag()));
	TestTrue(TEXT("second spend has a charge"), FGSTN_BackpackStep::Spend(Fragment, 102, Track));
	TestTrue(TEXT("empty after two"), HasTag(Fragment, GSTNBackpackTags::State_Empty_Slot0.GetTag()));
	TestFalse(TEXT("a third spend finds it empty (the gate missed)"), FGSTN_BackpackStep::Spend(Fragment, 103, Track));
	TestEqual(TEXT("never below zero"), FGSTN_BackpackStep::ChargesLeft(Fragment, 0), 0);

	FGSTN_BackpackStep::AfterClocks(Fragment, 109, false);
	TestEqual(TEXT("no charge back before RefillFrames"), FGSTN_BackpackStep::ChargesLeft(Fragment, 0), 0);
	FGSTN_BackpackStep::AfterClocks(Fragment, 110, false);
	TestEqual(TEXT("one back after RefillFrames (from the first spend)"), FGSTN_BackpackStep::ChargesLeft(Fragment, 0), 1);
	FGSTN_BackpackStep::AfterClocks(Fragment, 119, false);
	TestEqual(TEXT("the next counts from the last refill"), FGSTN_BackpackStep::ChargesLeft(Fragment, 0), 1);
	FGSTN_BackpackStep::AfterClocks(Fragment, 120, false);
	TestTrue(TEXT("full again: the slot forgets its rule and the fragment is the default"), Fragment.IsDefault());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackMemoryLandingNeverTest, "GSTNBackpack.Core.Memory.LandingAndNever", BackpackSimTestFlags)
bool FGSTNBackpackMemoryLandingNeverTest::RunTest(const FString& Parameters)
{
	FGSTN_BackpackFragment Fragment;
	FGSTN_BackpackStep::Spend(Fragment, 10, MakeCharge(1, 1, EGSTN_BackpackRefill::OnLanding));
	FGSTN_BackpackStep::Spend(Fragment, 10, MakeCharge(2, 3, EGSTN_BackpackRefill::Never));
	TestTrue(TEXT("the landing slot is empty"), HasTag(Fragment, GSTNBackpackTags::State_Empty_Slot1.GetTag()));
	TestEqual(TEXT("the supply slot has two left"), FGSTN_BackpackStep::ChargesLeft(Fragment, 2), 2);
	FGSTN_BackpackStep::AfterClocks(Fragment, 5000, false);
	TestEqual(TEXT("time gives nothing back to either"), FGSTN_BackpackStep::ChargesLeft(Fragment, 2) + FGSTN_BackpackStep::ChargesLeft(Fragment, 1), 2);
	FGSTN_BackpackStep::Landed(Fragment);
	TestEqual(TEXT("landing refills the landing slot (back to unused)"), FGSTN_BackpackStep::ChargesLeft(Fragment, 1), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("landing does not touch the supply"), FGSTN_BackpackStep::ChargesLeft(Fragment, 2), 2);
	TestEqual(TEXT("an unused slot reads unknown"), FGSTN_BackpackStep::ChargesLeft(Fragment, 3), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackDomeTest, "GSTNBackpack.Core.Dome", BackpackSimTestFlags)
bool FGSTNBackpackDomeTest::RunTest(const FString& Parameters)
{
	FGSTN_BackpackDomeTrack Held;
	Held.RadiusCm = 400;
	FGSTN_BackpackFragment Fragment;
	FGSTN_BackpackStep::RaiseDome(Fragment, 50, Held, FVector(100.4, 0, 0), 7, 600);
	TestTrue(TEXT("up"), Fragment.HasDome() && HasTag(Fragment, GSTNBackpackTags::State_Dome.GetTag()));
	TestTrue(TEXT("covers a point inside"), Fragment.DomeCovers(FVector(400, 0, 0)));
	TestFalse(TEXT("not a point outside"), Fragment.DomeCovers(FVector(600, 0, 0)));
	FGSTN_BackpackStep::AfterClocks(Fragment, 60, true);
	TestTrue(TEXT("held: stays while its move runs"), Fragment.HasDome());
	FGSTN_BackpackStep::AfterClocks(Fragment, 61, false);
	TestFalse(TEXT("held: falls when its move ends (release)"), Fragment.HasDome());
	TestTrue(TEXT("back to the default"), Fragment.IsDefault());

	FGSTN_BackpackStep::RaiseDome(Fragment, 50, Held, FVector::ZeroVector, 7, 20);
	FGSTN_BackpackStep::AfterClocks(Fragment, 70, true);
	TestFalse(TEXT("held: the timeline's length caps it"), Fragment.HasDome());

	FGSTN_BackpackDomeTrack Deploy;
	Deploy.Lifetime = EGSTN_DomeLifetime::Deploy;
	Deploy.DurationFrames = 30;
	FGSTN_BackpackStep::RaiseDome(Fragment, 100, Deploy, FVector::ZeroVector, 3, 10);
	FGSTN_BackpackStep::AfterClocks(Fragment, 129, false);
	TestTrue(TEXT("deployed: stays after its move ends"), Fragment.HasDome());
	FGSTN_BackpackStep::AfterClocks(Fragment, 130, false);
	TestFalse(TEXT("deployed: falls after DurationFrames"), Fragment.HasDome());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackFragmentWireTest, "GSTNBackpack.Core.FragmentWire", BackpackSimTestFlags)
bool FGSTNBackpackFragmentWireTest::RunTest(const FString& Parameters)
{
	int32 Bytes = 0;
	const FGSTN_BackpackFragment Default;
	TestTrue(TEXT("the default round trips"), RoundTrip(Default, &Bytes) == Default);
	TestTrue(TEXT("the default is one byte at most"), Bytes <= 1);

	FGSTN_BackpackFragment Full;
	FGSTN_BackpackStep::Spend(Full, 1000, MakeCharge(0, 2, EGSTN_BackpackRefill::OverTime, 1250));
	FGSTN_BackpackStep::Spend(Full, 1000, MakeCharge(3, 5, EGSTN_BackpackRefill::Never));
	FGSTN_BackpackDomeTrack Dome;
	Dome.Lifetime = EGSTN_DomeLifetime::Deploy;
	FGSTN_BackpackStep::RaiseDome(Full, 1000, Dome, FVector(-1234.6, 55.0, 98765.2), 12, 625);
	const FGSTN_BackpackFragment Received = RoundTrip(Full);
	TestTrue(TEXT("slots and dome round trip"), Received == Full);
	TestFalse(TEXT("equal states do not reconcile"), Received.ShouldReconcile(Full));
	FGSTN_BackpackFragment Other = Full;
	Other.Memory[0].Spent = 2;
	TestTrue(TEXT("a different charge count reconciles"), Other.ShouldReconcile(Full));
	return true;
}

#endif
