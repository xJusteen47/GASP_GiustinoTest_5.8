// Automation tests of the parts snapshot contributor (P7, combat-parts.md §5.2). No world: the contributor reads a context, the
// snapshot and the history are plain data.

#include "GSTN_PartsNPTestFixture.h"
#include "Sim/GSTN_PartsSnapshotContributor.h"
#include "Simulation/GSTN_LagCompensation.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "World/GSTN_CombatWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNPartsNPTest;

namespace
{
	bool NoPose(int32 EntityId, FTransform& OutPose)
	{
		return false;
	}

	const FGSTN_PartsSnapshotExt* ExtOf(const FGSTN_SnapshotEntry& Entry)
	{
		return Entry.Extension.Get<FGSTN_PartsSnapshotExt>(GSTNFeatureIds::Parts);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsSnapshotEndOfFrameMinusOneTest, "GSTNParts.NP.Snapshot.ReadsEndOfFrameMinusOne", GSTNPartsNPTest::Flags)
bool FGSTN_PartsSnapshotEndOfFrameMinusOneTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	AActor* Owner = MakeOwner();
	FGSTN_PartsAttach::ApplyPartSet(Owner, Fixture.Set.Get(), nullptr);

	// The snapshot of frame F is read from the fragments of the state that ended F-1; the state frame F is writing is a different array
	FGSTN_SyncFragmentArray EndOfPrevious;
	SetFragment(EndOfPrevious, MakeState(1u << Arm, 2, { { Arm, 255 }, { Head, 100 } }), Fixture.Num());
	FGSTN_SyncFragmentArray BeingWritten;
	SetFragment(BeingWritten, MakeState((1u << Arm) | (1u << Head), 3, { { Arm, 255 }, { Head, 255 } }), Fixture.Num());

	FGSTN_PartsSnapshotContributor Contributor;
	FGSTN_SnapshotContributorRegistry Registry;
	TestTrue(TEXT("the contributor registers"), Registry.Register(&Contributor));

	FGSTN_SnapshotContributorContext Context;
	Context.Frame = 10;
	Context.Owner = Owner;
	Context.Fragments = &EndOfPrevious;
	FGSTN_SnapshotEntry Entry = Fixture.MakeEntry();
	Registry.Fill(Context, Entry);

	const FGSTN_PartsSnapshotExt* Ext = ExtOf(Entry);
	TestNotNull(TEXT("an entity with a set carries the extension"), Ext);
	if (!Ext)
	{
		return false;
	}
	TestEqual(TEXT("mask of the end of the previous frame"), Ext->BrokenMask, static_cast<uint16>(1u << Arm));
	TestEqual(TEXT("its serial"), Ext->BreakSerial, static_cast<uint8>(2));
	TestEqual(TEXT("its damage"), Ext->PartDamage[Head], static_cast<uint8>(100));
	TestTrue(TEXT("and the set"), Ext->PartSet == Fixture.Set.Get());
	TestFalse(TEXT("the gate (one broken part) is met: the entity can be grabbed, not locked"), Ext->IsGrabLocked());
	TestTrue(TEXT("the victim view points at the same bytes"), FGSTN_PartsSnapshotExt::FromView(FGSTN_CombatVictim::MakeVictimView(Entry, nullptr)) == Ext);

	// A pristine entity with a set and no fragment still carries the extension (the first hit resolves against its parts)
	FGSTN_SyncFragmentArray None;
	Context.Fragments = &None;
	FGSTN_SnapshotEntry Fresh = Fixture.MakeEntry();
	Registry.Fill(Context, Fresh);
	TestNotNull(TEXT("no fragment yet: a pristine extension"), ExtOf(Fresh));
	TestEqual(TEXT("empty mask"), ExtOf(Fresh) ? ExtOf(Fresh)->BrokenMask : 1, static_cast<uint16>(0));
	TestTrue(TEXT("whole: the gate is unmet, grab locked"), ExtOf(Fresh) && ExtOf(Fresh)->IsGrabLocked());

	// An entity without a set contributes nothing
	FGSTN_SnapshotEntry Other = Fixture.MakeEntry(5);
	Context.Owner = MakeOwner();
	Registry.Fill(Context, Other);
	TestFalse(TEXT("no set: no extension"), Other.Extension.IsSet());

	FGSTN_PartsAttach::Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsSnapshotBytesTest, "GSTNParts.NP.Snapshot.BytesWithinBudget", GSTNPartsNPTest::Flags)
bool FGSTN_PartsSnapshotBytesTest::RunTest(const FString& Parameters)
{
	// The spec budget (§5.2) is 20 bytes of state per entity: the type word and the set pointer are the plumbing around it
	constexpr int32 State = static_cast<int32>(STRUCT_OFFSET(FGSTN_PartsSnapshotExt, PartSet)) - static_cast<int32>(sizeof(uint32));
	TestTrue(TEXT("mask, 16 damage bytes, serial and the derived flags are at most 20 bytes"), State <= 20);
	TestTrue(TEXT("the whole extension fits the blob"), sizeof(FGSTN_PartsSnapshotExt) <= FGSTN_SnapshotExtensionBlob::MaxBytes);
	TestTrue(TEXT("the blob with its header stays under 48 bytes per entity per frame"), sizeof(FGSTN_SnapshotExtensionBlob) <= 48);
	TestTrue(TEXT("trivially copyable"), TIsTriviallyCopyAssignable<FGSTN_PartsSnapshotExt>::Value);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsSnapshotLagHistoryTest, "GSTNParts.NP.Snapshot.LagHistoryMemory", GSTNPartsNPTest::Flags)
bool FGSTN_PartsSnapshotLagHistoryTest::RunTest(const FString& Parameters)
{
	constexpr int32 VictimId = 7;
	constexpr int32 AttackerId = 3;
	constexpr float StepMs = 16.f;
	FPartsFixture Fixture;
	UGSTN_CombatWorldSubsystem* Subsystem = NewObject<UGSTN_CombatWorldSubsystem>(GetTransientPackage());
	Subsystem->RequestHistory(400);

	// The arm breaks at frame 12: the extension of frame 11 is whole, of 12 on it is broken
	for (int32 Frame = 10; Frame <= 14; ++Frame)
	{
		FGSTN_StepSnapshot Snapshot;
		Snapshot.Frame = Frame;
		for (const int32 Id : { AttackerId, VictimId })
		{
			FGSTN_SnapshotEntry& Entry = Snapshot.Entries.Add_GetRef(Fixture.MakeEntry(Id));
			if (Id == VictimId)
			{
				FGSTN_PartsSnapshotExt Ext;
				Ext.PartSet = Fixture.Set.Get();
				Ext.SetState(MakeState(static_cast<uint16>(Frame >= 12 ? (1u << Arm) : 0u), static_cast<uint8>(Frame >= 12 ? 1 : 0)));
				Entry.Extension.Set(GSTNFeatureIds::Parts, Ext);
			}
		}
		Subsystem->RecordHistory(Snapshot, Frame * StepMs, StepMs);
	}

	FGSTN_StepSnapshot Present;
	Present.Frame = 14;
	for (const int32 Id : { AttackerId, VictimId })
	{
		FGSTN_SnapshotEntry& Entry = Present.Entries.Add_GetRef(Fixture.MakeEntry(Id));
		if (Id == VictimId)
		{
			FGSTN_PartsSnapshotExt Ext;
			Ext.PartSet = Fixture.Set.Get();
			Ext.SetState(MakeState(1u << Arm, 1));
			Entry.Extension.Set(GSTNFeatureIds::Parts, Ext);
		}
	}

	FGSTN_StepSnapshot Rewound;
	GSTNLagComp::BuildOverlay(Present, AttackerId, 11 * StepMs, *Subsystem, &NoPose, Rewound);
	const FGSTN_SnapshotEntry* Victim = Rewound.Find(VictimId);
	TestNotNull(TEXT("the overlay holds the victim"), Victim);
	if (!Victim || !ExtOf(*Victim))
	{
		return false;
	}
	TestEqual(TEXT("an attacker rewound to frame 11 hits the arm whole"), ExtOf(*Victim)->BrokenMask, static_cast<uint16>(0));
	TestTrue(TEXT("with the set of the record"), ExtOf(*Victim)->PartSet == Fixture.Set.Get());

	FGSTN_StepSnapshot Now;
	GSTNLagComp::BuildOverlay(Present, AttackerId, 14 * StepMs, *Subsystem, &NoPose, Now);
	TestEqual(TEXT("and one that is not rewound hits it broken"), ExtOf(*Now.Find(VictimId))->BrokenMask, static_cast<uint16>(1u << Arm));
	TestEqual(TEXT("the shared snapshot is never touched"), ExtOf(*Present.Find(VictimId))->BrokenMask, static_cast<uint16>(1u << Arm));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
