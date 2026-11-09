// Automation tests of the parts snapshot contributor (P7, combat-parts.md §5.2). No world: the contributor reads a context, the
// snapshot and the history are plain data.

#include "GSTN_PartsNPTestFixture.h"
#include "Math/GSTN_PartMath.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsSnapshotEndOfFrameMinusOneTest, "GSTNParts.NP.Snapshot.ReadsEndOfFrameMinusOne", GSTNPartsNPTest::TestFlags)
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
	TestEqual(TEXT("and no wound"), ExtOf(Fresh) ? ExtOf(Fresh)->WoundedMask : 1, static_cast<uint16>(0));

	// An entity without a set contributes nothing
	FGSTN_SnapshotEntry Other = Fixture.MakeEntry(5);
	Context.Owner = MakeOwner();
	Registry.Fill(Context, Other);
	TestFalse(TEXT("no set: no extension"), Other.Extension.IsSet());

	FGSTN_PartsAttach::Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsSnapshotBytesTest, "GSTNParts.NP.Snapshot.BytesWithinBudget", GSTNPartsNPTest::TestFlags)
bool FGSTN_PartsSnapshotBytesTest::RunTest(const FString& Parameters)
{
	// The spec budget (§5.2) was 20 bytes of state per entity, 22 with the wounded mask (wounds, 3.4): the type word and the set pointer are
	// the plumbing around it. The type word is 16 bits so that the two more bytes still fit the 32 byte blob
	constexpr int32 State = static_cast<int32>(STRUCT_OFFSET(FGSTN_PartsSnapshotExt, PartSet)) - static_cast<int32>(sizeof(FGSTN_PartsSnapshotExt::Type));
	TestTrue(TEXT("two masks, 16 damage bytes, serial and the derived flags are at most 22 bytes"), State <= 22);
	TestTrue(TEXT("the whole extension fits the blob"), sizeof(FGSTN_PartsSnapshotExt) <= FGSTN_SnapshotExtensionBlob::MaxBytes);
	TestTrue(TEXT("the blob with its header stays under 48 bytes per entity per frame"), sizeof(FGSTN_SnapshotExtensionBlob) <= 48);
	TestTrue(TEXT("trivially copyable"), TIsTriviallyCopyAssignable<FGSTN_PartsSnapshotExt>::Value);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsSnapshotWoundCopyTest, "GSTNParts.NP.Snapshot.CarriesWoundedMask", GSTNPartsNPTest::TestFlags)
bool FGSTN_PartsSnapshotWoundCopyTest::RunTest(const FString& Parameters)
{
	// The snapshot reads the wounded mask of the fragment of the end of F-1 with the rest of the state (wounds, combat-parts.md 3.4)
	FPartsFixture Fixture;
	AActor* Owner = MakeOwner();
	FGSTN_PartsAttach::ApplyPartSet(Owner, Fixture.Set.Get(), nullptr);

	FGSTN_PartsState Wounded = MakeState(0, 0);
	Wounded.WoundedMask = 1u << Arm;
	FGSTN_SyncFragmentArray Fragments;
	SetFragment(Fragments, Wounded, Fixture.Num());

	FGSTN_PartsSnapshotContributor Contributor;
	FGSTN_SnapshotContributorRegistry Registry;
	TestTrue(TEXT("the contributor registers"), Registry.Register(&Contributor));
	FGSTN_SnapshotContributorContext Context;
	Context.Frame = 10;
	Context.Owner = Owner;
	Context.Fragments = &Fragments;
	FGSTN_SnapshotEntry Entry = Fixture.MakeEntry();
	Registry.Fill(Context, Entry);

	const FGSTN_PartsSnapshotExt* Ext = ExtOf(Entry);
	TestNotNull(TEXT("the extension is there"), Ext);
	if (Ext)
	{
		TestEqual(TEXT("with the wounded mask"), Ext->WoundedMask, static_cast<uint16>(1u << Arm));
		TestEqual(TEXT("and it reaches the part math through ToState"), Ext->ToState().WoundedMask, static_cast<uint16>(1u << Arm));
	}
	FGSTN_PartsAttach::Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsSnapshotRewoundWoundTest, "GSTNParts.NP.Snapshot.RewoundHitReadsWoundStateOfItsFrame", GSTNPartsNPTest::TestFlags)
bool FGSTN_PartsSnapshotRewoundWoundTest::RunTest(const FString& Parameters)
{
	// The part state rides in the lag history as the extension memory of the snapshot (P7), so a rewound attacker reads the wounds its frame had.
	// What is NOT in that history is the hurtbox states (the open residue of P7 part 1): level 1 does not need them
	constexpr int32 VictimId = 7;
	constexpr int32 AttackerId = 3;
	constexpr float StepMs = 16.f;
	FPartsFixture Fixture;
	Fixture.Set->Parts[Arm].WoundedHardness = 0;
	UGSTN_CombatWorldSubsystem* Subsystem = NewObject<UGSTN_CombatWorldSubsystem>(GetTransientPackage());
	Subsystem->RequestHistory(400);

	// The arm (hardness 4) is wounded at frame 12: the extension of frame 11 is whole, of 12 on it carries the bit
	auto MakeSnapshot = [&](int32 Frame)
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
				FGSTN_PartsState State = MakeState(0, 0);
				State.WoundedMask = static_cast<uint16>(Frame >= 12 ? (1u << Arm) : 0u);
				Ext.SetState(State);
				Entry.Extension.Set(GSTNFeatureIds::Parts, Ext);
			}
		}
		return Snapshot;
	};
	for (int32 Frame = 10; Frame <= 14; ++Frame)
	{
		Subsystem->RecordHistory(MakeSnapshot(Frame), Frame * StepMs, StepMs);
	}
	const FGSTN_StepSnapshot Present = MakeSnapshot(14);

	FGSTN_StepSnapshot Rewound;
	GSTNLagComp::BuildOverlay(Present, AttackerId, 11 * StepMs, *Subsystem, &NoPose, Rewound);
	FGSTN_StepSnapshot Now;
	GSTNLagComp::BuildOverlay(Present, AttackerId, 14 * StepMs, *Subsystem, &NoPose, Now);
	const FGSTN_SnapshotEntry* RewoundVictim = Rewound.Find(VictimId);
	const FGSTN_SnapshotEntry* NowVictim = Now.Find(VictimId);
	if (!RewoundVictim || !ExtOf(*RewoundVictim) || !NowVictim || !ExtOf(*NowVictim))
	{
		AddError(TEXT("the overlays hold the victim with its parts extension"));
		return false;
	}
	TestEqual(TEXT("an attacker rewound to frame 11 meets the arm unwounded"), ExtOf(*RewoundVictim)->WoundedMask, static_cast<uint16>(0));
	TestEqual(TEXT("one that is not rewound meets it wounded"), ExtOf(*NowVictim)->WoundedMask, static_cast<uint16>(1u << Arm));

	// The same hit (penetration 2 on the arm, hardness 4 whole, 0 wounded) ricochets on the first frame and lands on the second
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	FGSTN_PartsState RewoundState = ExtOf(*RewoundVictim)->ToState();
	FGSTN_PartsState NowState = ExtOf(*NowVictim)->ToState();
	FGSTN_PartHitInput Hit;
	Hit.Damage = 10.f;
	Hit.PartIndex = Arm;
	Hit.Penetration = 2;
	TestTrue(TEXT("the rewound hit ricochets as it did at its frame"), UGSTN_PartMath::ApplyHit(Fixture.Set->GetParts(), Bands, RewoundState, Hit).bRicochet);
	TestFalse(TEXT("the same hit against the present state lands"), UGSTN_PartMath::ApplyHit(Fixture.Set->GetParts(), Bands, NowState, Hit).bRicochet);
	TestEqual(TEXT("the shared snapshot is never touched"), ExtOf(*Present.Find(VictimId))->WoundedMask, static_cast<uint16>(1u << Arm));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsSnapshotLagHistoryTest, "GSTNParts.NP.Snapshot.LagHistoryMemory", GSTNPartsNPTest::TestFlags)
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
