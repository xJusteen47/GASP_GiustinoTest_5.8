// Automation tests of attaching a part set to an entity (P7, combat-parts.md §2, §5.1, §5.5b). No OnRep anywhere: the host and the client
// run the same function.

#include "GSTN_PartsNPTestFixture.h"
#include "Data/GSTN_PartsKitExtension.h"
#include "GSTN_PartsTags.h"
#include "Sim/GSTN_PartsSnapshotContributor.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNPartsNPTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsAttachHostTest, "GSTNParts.NP.Attach.HostWithoutOnRep", GSTNPartsNPTest::Flags)
bool FGSTN_PartsAttachHostTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	FRig Rig;

	// The host attaches straight from the loadout event: no replication callback is involved, the function is the same everywhere
	TestFalse(TEXT("nothing attached yet"), FGSTN_PartsAttach::FindSet(Rig.Owner) != nullptr);
	TestTrue(TEXT("the set attaches"), FGSTN_PartsAttach::ApplyPartSet(Rig.Owner, Fixture.Set.Get(), Rig.ASC));
	TestTrue(TEXT("the owner has the set"), FGSTN_PartsAttach::FindSet(Rig.Owner) == Fixture.Set.Get());
	TestEqual(TEXT("and the marker tag"), Rig.ASC->GetTagCount(GSTNPartsTags::Entity_Destructible), 1);

	// A repeated call (a second loadout event) is the same attachment, not a second marker
	TestTrue(TEXT("again"), FGSTN_PartsAttach::ApplyPartSet(Rig.Owner, Fixture.Set.Get(), Rig.ASC));
	TestEqual(TEXT("the marker is a count of one"), Rig.ASC->GetTagCount(GSTNPartsTags::Entity_Destructible), 1);
	TestEqual(TEXT("one entity"), FGSTN_PartsAttach::Num(), 1);

	// A kit that names no set detaches
	FGSTN_PartsAttach::ApplyPartSet(Rig.Owner, nullptr, Rig.ASC);
	TestTrue(TEXT("detached"), FGSTN_PartsAttach::FindSet(Rig.Owner) == nullptr);
	TestEqual(TEXT("the marker is gone"), Rig.ASC->GetTagCount(GSTNPartsTags::Entity_Destructible), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsAttachClientTest, "GSTNParts.NP.Attach.ClientAfterRep", GSTNPartsNPTest::Flags)
bool FGSTN_PartsAttachClientTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	AActor* Owner = MakeOwner();

	// A client's sync state can arrive before its kit does: the fragment already holds a broken arm when the set is applied
	FGSTN_SyncFragmentArray Received;
	SetFragment(Received, MakeState(1u << Arm, 2, { { Arm, 255 } }), Fixture.Num());
	const FGSTN_PartsFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Received);
	TestTrue(TEXT("a compatible set attaches over the received state"), FGSTN_PartsAttach::ApplyPartSet(Owner, Fixture.Set.Get(), nullptr, Fragment));

	// And from then on the snapshot reads the received state with the set's meaning
	FGSTN_PartsSnapshotContributor Contributor;
	FGSTN_SnapshotContributorContext Context;
	Context.Owner = Owner;
	Context.Fragments = &Received;
	FGSTN_SnapshotExtensionBlob Blob;
	Contributor.Contribute(Context, Blob);
	const FGSTN_PartsSnapshotExt* Ext = Blob.Get<FGSTN_PartsSnapshotExt>(GSTNFeatureIds::Parts);
	TestNotNull(TEXT("the extension exists after the set arrived"), Ext);
	TestEqual(TEXT("with the received mask"), Ext ? Ext->BrokenMask : 0, static_cast<uint16>(1u << Arm));

	// A kit extension resolves to the set (the soft reference to an object that is alive)
	FGSTN_PartsKitExtension KitExtension;
	KitExtension.PartSet = Fixture.Set.Get();
	FGSTN_KitExtensionList List;
	List.ExtensionData.Add(FInstancedStruct::Make(KitExtension));
	TestTrue(TEXT("the kit names the set"), FGSTN_PartsAttach::ResolveKitSet(List) == Fixture.Set.Get());
	TestTrue(TEXT("a kit without the extension names none"), FGSTN_PartsAttach::ResolveKitSet(FGSTN_KitExtensionList()) == nullptr);

	FGSTN_PartsAttach::Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsAttachMismatchTest, "GSTNParts.NP.Attach.SetMismatchRefused", GSTNPartsNPTest::Flags)
bool FGSTN_PartsAttachMismatchTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	AActor* Owner = MakeOwner();
	AddExpectedError(TEXT("the machines disagree on the set"), EAutomationExpectedErrorFlags::Contains, 3);

	// N is not on the wire: a fragment that already holds damage on part 6 cannot belong to a set of four parts
	FGSTN_SyncFragmentArray Received;
	SetFragment(Received, MakeState(0, 0, { { 6, 90 } }), 8);
	const FGSTN_PartsFragment* TooMany = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Received);
	TestFalse(TEXT("the state is not compatible"), FGSTN_PartsAttach::IsPartCountCompatible(*TooMany, *Fixture.Set));
	TestFalse(TEXT("the set is refused"), FGSTN_PartsAttach::ApplyPartSet(Owner, Fixture.Set.Get(), nullptr, TooMany));
	TestTrue(TEXT("and nothing is attached"), FGSTN_PartsAttach::FindSet(Owner) == nullptr);

	// The same when the fragment recorded another count than the set has
	FGSTN_SyncFragmentArray Counted;
	SetFragment(Counted, MakeState(1u << Torso, 1), 6);
	const FGSTN_PartsFragment* OtherCount = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Counted);
	TestFalse(TEXT("another recorded count is refused"), FGSTN_PartsAttach::ApplyPartSet(Owner, Fixture.Set.Get(), nullptr, OtherCount));

	// A previously attached set is dropped by a refusal: a wrong set is worse than none
	TestTrue(TEXT("a good attach"), FGSTN_PartsAttach::ApplyPartSet(Owner, Fixture.Set.Get(), nullptr));
	TestFalse(TEXT("a refusal"), FGSTN_PartsAttach::ApplyPartSet(Owner, Fixture.Set.Get(), nullptr, TooMany));
	TestTrue(TEXT("detaches"), FGSTN_PartsAttach::FindSet(Owner) == nullptr);

	// An empty set and a set of too many parts are refused as well
	TStrongObjectPtr<UGSTN_PartSetDefinition> Empty(NewObject<UGSTN_PartSetDefinition>(GetTransientPackage()));
	AddExpectedError(TEXT("has 0 parts"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("no parts"), FGSTN_PartsAttach::ApplyPartSet(Owner, Empty.Get(), nullptr));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
