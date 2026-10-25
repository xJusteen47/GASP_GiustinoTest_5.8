// Automation tests of the snapshot extension seam (spec seam D9, M4 follow-up): the contributor registry, the extension the victim view
// points at, and the lag compensation history that carries it. No world: plain data and a transient subsystem.

#include "Misc/AutomationTest.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "Simulation/GSTN_LagCompensation.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	struct FTestExtensionPod
	{
		uint32 Value = 0;
	};

	/** Writes the frame number of the context into the blob */
	class FTestContributor : public IGSTN_SnapshotContributor
	{
	public:
		explicit FTestContributor(uint8 InTypeId) : TypeId(InTypeId) {}

		virtual uint8 GetTypeId() const override { return TypeId; }

		virtual void Contribute(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionBlob& Blob) const override
		{
			FTestExtensionPod Pod;
			Pod.Value = static_cast<uint32>(Context.Frame);
			Blob.Set(TypeId, Pod);
		}

	private:
		uint8 TypeId;
	};

	bool SnapExtNoPose(int32 EntityId, FTransform& OutPose)
	{
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SnapshotExtNeutralTest, "GSTNCombat.NP.SnapshotExt.NeutralWithoutContributor", GSTN_NP_TEST_FLAGS)
bool FGSTN_SnapshotExtNeutralTest::RunTest(const FString& Parameters)
{
	FGSTN_SnapshotContributorRegistry Registry;
	FGSTN_SnapshotEntry Entry;
	Registry.Fill(FGSTN_SnapshotContributorContext(), Entry);
	TestFalse(TEXT("nothing registered: the entry has no extension"), Entry.Extension.IsSet());
	TestNull(TEXT("and the victim view points at nothing"), FGSTN_CombatVictim::MakeVictimView(Entry, nullptr).SnapshotExtension);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SnapshotExtViewTest, "GSTNCombat.NP.SnapshotExt.ViewPointsAtTheEntryBlob", GSTN_NP_TEST_FLAGS)
bool FGSTN_SnapshotExtViewTest::RunTest(const FString& Parameters)
{
	FGSTN_SnapshotContributorRegistry Registry;
	FTestContributor Contributor(GSTNFeatureIds::TestA);
	TestTrue(TEXT("a contributor registers"), Registry.Register(&Contributor));
	TestFalse(TEXT("the same one twice is refused"), Registry.Register(&Contributor));

	FGSTN_SnapshotContributorContext Context;
	Context.Frame = 42;
	FGSTN_SnapshotEntry Entry;
	Registry.Fill(Context, Entry);
	TestTrue(TEXT("the entry carries the extension"), Entry.Extension.IsSet());

	const FGSTN_HitVictimView View = FGSTN_CombatVictim::MakeVictimView(Entry, nullptr);
	const FTestExtensionPod* Pod = static_cast<const FTestExtensionPod*>(View.SnapshotExtension);
	TestNotNull(TEXT("the view points at the blob"), Pod);
	TestEqual(TEXT("which holds what the contributor wrote"), Pod ? Pod->Value : 0u, 42u);
	TestNull(TEXT("a typed read of another id is empty"), Entry.Extension.Get<FTestExtensionPod>(GSTNFeatureIds::TestB));

	// A rebuild (the victim drain) writes the same blob again from the later state
	Context.Frame = 43;
	Registry.Rebuild(Context, Entry.Extension);
	TestEqual(TEXT("the rebuild overwrote it in place"), Pod ? Pod->Value : 0u, 43u);

	Registry.Unregister(&Contributor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SnapshotExtOneTypeTest, "GSTNCombat.NP.SnapshotExt.OneTypeAtATime", GSTN_NP_TEST_FLAGS)
bool FGSTN_SnapshotExtOneTypeTest::RunTest(const FString& Parameters)
{
	FGSTN_SnapshotContributorRegistry Registry;
	FTestContributor A(GSTNFeatureIds::TestA);
	FTestContributor Invalid(GSTNFeatureIds::Invalid);
	TestFalse(TEXT("id 0 is refused"), Registry.Register(&Invalid));
	TestFalse(TEXT("null is refused"), Registry.Register(nullptr));
	TestTrue(TEXT("one contributor registers"), Registry.Register(&A));
	TestEqual(TEXT("one is registered"), Registry.Num(), 1);
	Registry.Unregister(&A);
	TestEqual(TEXT("and leaves again"), Registry.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SnapshotExtHistoryTest, "GSTNCombat.NP.SnapshotExt.HistoryRewindsTheExtension", GSTN_NP_TEST_FLAGS)
bool FGSTN_SnapshotExtHistoryTest::RunTest(const FString& Parameters)
{
	constexpr int32 SnapExtVictimId = 7;
	constexpr int32 SnapExtAttackerId = 3;
	constexpr float StepMs = 16.f;
	UGSTN_CombatWorldSubsystem* Subsystem = NewObject<UGSTN_CombatWorldSubsystem>(GetTransientPackage());
	Subsystem->RequestHistory(400);

	// Frames 10..14: the extension value is the frame, so the rewound entry must hold the value of the frame it was rewound to
	for (int32 Frame = 10; Frame <= 14; ++Frame)
	{
		FGSTN_StepSnapshot Snapshot;
		Snapshot.Frame = Frame;
		for (const int32 Id : { SnapExtAttackerId, SnapExtVictimId })
		{
			FGSTN_SnapshotEntry& Entry = Snapshot.Entries.AddDefaulted_GetRef();
			Entry.EntityId = Id;
			FTestExtensionPod Pod;
			Pod.Value = static_cast<uint32>(Frame);
			Entry.Extension.Set(GSTNFeatureIds::TestA, Pod);
		}
		Subsystem->RecordHistory(Snapshot, Frame * StepMs, StepMs);
	}

	FGSTN_StepSnapshot Present;
	Present.Frame = 14;
	for (const int32 Id : { SnapExtAttackerId, SnapExtVictimId })
	{
		FGSTN_SnapshotEntry& Entry = Present.Entries.AddDefaulted_GetRef();
		Entry.EntityId = Id;
		FTestExtensionPod Pod;
		Pod.Value = 14;
		Entry.Extension.Set(GSTNFeatureIds::TestA, Pod);
	}
	FGSTN_StepSnapshot Overlay;
	GSTNLagComp::BuildOverlay(Present, SnapExtAttackerId, 12 * StepMs, *Subsystem, &SnapExtNoPose, Overlay);

	const FGSTN_SnapshotEntry* Victim = Overlay.Find(SnapExtVictimId);
	const FGSTN_SnapshotEntry* Attacker = Overlay.Find(SnapExtAttackerId);
	TestNotNull(TEXT("the overlay holds the victim"), Victim);
	TestNotNull(TEXT("and the attacker"), Attacker);
	if (!Victim || !Attacker)
	{
		return false;
	}
	const FTestExtensionPod* Rewound = Victim->Extension.Get<FTestExtensionPod>(GSTNFeatureIds::TestA);
	TestNotNull(TEXT("the rewound victim carries an extension"), Rewound);
	TestEqual(TEXT("the one of the rewound frame, not the current one"), Rewound ? Rewound->Value : 0u, 12u);
	const FTestExtensionPod* Own = Attacker->Extension.Get<FTestExtensionPod>(GSTNFeatureIds::TestA);
	TestEqual(TEXT("the attacker itself is not rewound"), Own ? Own->Value : 0u, 14u);
	TestEqual(TEXT("the shared snapshot is untouched"), Present.Find(SnapExtVictimId)->Extension.Get<FTestExtensionPod>(GSTNFeatureIds::TestA)->Value, 14u);
	return true;
}

#endif
