// Automation tests of the dome pushing back a blocked attacker (M11 L7d): the push data through the fragment (wire included) and the snapshot
// blob, and the push verdicts of FGSTN_BackpackDomePush on a plain snapshot. No world.

#include "Misc/AutomationTest.h"
#include "GSTN_BackpackDomePush.h"
#include "GSTN_BackpackDomeSnapshot.h"
#include "GSTN_BackpackSim.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Sync/GSTN_SyncFragment.h"
#include "World/GSTN_StepSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_BACKPACK_PUSH_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNBackpackPushTest
{
	/** Entry Id at Location; with a dome of Radius there (push Speed / Frames / BackAlong), its shell as extra 0 */
	FGSTN_SnapshotEntry MakeEntry(int32 Id, const FVector& Location, float Radius = 0.f, uint8 SpeedQ = 70, uint8 Frames = 12, bool bBackAlong = false)
	{
		FGSTN_SnapshotEntry Entry;
		Entry.EntityId = Id;
		Entry.Transform = FTransform(Location);
		if (Radius > 0.f)
		{
			FGSTN_BackpackSnapshotExt Ext;
			Ext.bHasDome = 1;
			Ext.DomeCenter[0] = static_cast<float>(Location.X);
			Ext.DomeCenter[1] = static_cast<float>(Location.Y);
			Ext.DomeCenter[2] = static_cast<float>(Location.Z);
			Ext.DomeRadiusCm = Radius;
			Ext.PushSpeedQ = SpeedQ;
			Ext.PushPacked = static_cast<uint8>(Frames | (bBackAlong ? 0x80 : 0));
			Ext.AddInside(Id);
			Entry.Extension.Set(GSTNFeatureIds::Backpack, Ext);
			Entry.ExtraHurtboxes.Add(FGSTN_BackpackSnapshotContributor::MakeDomeHurtbox(Location, Radius, Entry.Transform));
		}
		return Entry;
	}

	/** Entry Id at Location standing in the domes listed */
	FGSTN_SnapshotEntry MakeInsideEntry(int32 Id, const FVector& Location, std::initializer_list<int32> Domes)
	{
		FGSTN_SnapshotEntry Entry;
		Entry.EntityId = Id;
		Entry.Transform = FTransform(Location);
		FGSTN_BackpackSnapshotExt Ext;
		for (const int32 Dome : Domes)
		{
			Ext.AddInside(Dome);
		}
		Entry.Extension.Set(GSTNFeatureIds::Backpack, Ext);
		return Entry;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackDomePushDataTest, "GSTNBackpack.NP.Dome.PushData", GSTN_BACKPACK_PUSH_TEST_FLAGS)
bool FGSTNBackpackDomePushDataTest::RunTest(const FString& Parameters)
{
	// The track's push goes into the fragment at the raise, quantised
	FGSTN_SyncFragmentArray Fragments;
	FGSTN_BackpackFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_BackpackFragment>(Fragments);
	FGSTN_BackpackDomeTrack Track;
	Track.Lifetime = EGSTN_DomeLifetime::Deploy;
	Track.PushbackSpeed = 704.f;
	Track.PushbackFrames = 15;
	Track.PushbackDirection = EGSTN_DomePushDirection::BackAlongAttack;
	FGSTN_BackpackStep::RaiseDome(Fragment, 10, Track, FVector(500.0, 0.0, 0.0), 0, 500);
	TestEqual(TEXT("speed in 10 cm/s steps"), static_cast<int32>(Fragment.DomePushSpeedQ), 70);
	TestEqual(TEXT("frames"), static_cast<int32>(Fragment.DomePushFrames), 15);
	TestTrue(TEXT("direction"), Fragment.DomePushDirection == EGSTN_DomePushDirection::BackAlongAttack);

	// The wire keeps all three
	FBitWriter Writer(0, true);
	Fragment.NetSerialize(Writer);
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_BackpackFragment Read;
	Read.NetSerialize(Reader);
	TestTrue(TEXT("the wire round trip is exact"), Read == Fragment);

	// The snapshot blob carries them in its padding
	FGSTN_SnapshotContributorContext Context;
	Context.Frame = 11;
	Context.Fragments = &Fragments;
	Context.Transform = FTransform(FVector(500.0, 0.0, 0.0));
	FGSTN_SnapshotExtensionBlob Blob;
	FGSTN_BackpackSnapshotContributor().Contribute(Context, Blob);
	const FGSTN_BackpackSnapshotExt* Ext = Blob.Get<FGSTN_BackpackSnapshotExt>(GSTNFeatureIds::Backpack);
	TestTrue(TEXT("the blob has the push"), Ext && Ext->PushSpeedQ == 70 && Ext->PushPacked == (15 | 0x80));

	// A dome that falls clears them, so an idle fragment is still the default
	FGSTN_BackpackStep::AfterClocks(Fragment, 1000, false);
	TestTrue(TEXT("down: back to the default"), Fragment.IsDefault());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackDomePushVerdictTest, "GSTNBackpack.NP.Dome.PushVerdicts", GSTN_BACKPACK_PUSH_TEST_FLAGS)
bool FGSTNBackpackDomePushVerdictTest::RunTest(const FString& Parameters)
{
	using namespace GSTNBackpackPushTest;
	const FGSTN_BackpackDomePush Push;
	FVector Velocity;
	int32 Frames = 0;

	// Carrier 1 with a 400 cm dome at the origin (700 cm/s, 12 frames, out of the dome); 2 stands inside it; 4 carries a dome that does not push
	FGSTN_StepSnapshot Snapshot;
	Snapshot.Entries = { MakeEntry(1, FVector::ZeroVector, 400.f), MakeInsideEntry(2, FVector(100.0, 0.0, 0.0), { 1 }),
		MakeEntry(4, FVector(3000.0, 0.0, 0.0), 400.f, 0) };

	// Attacker outside at (500, 0, 300), facing down-left: a hit on the shell pushes it straight out on its plane
	FGSTN_AttackerPushQuery Query(Snapshot);
	Query.VictimId = 1;
	Query.HurtboxIndex = GSTNHurtboxIndex::ExtraIndexBase;
	Query.AttackerRoot = FVector(500.0, 0.0, 300.0);
	Query.AttackerFacing = FVector(-0.6, -0.8, 0.0);
	if (TestTrue(TEXT("the shell pushes"), Push.GetPush(Query, Velocity, Frames)))
	{
		TestTrue(TEXT("out of the dome, flat"), Velocity.Equals(FVector(700.0, 0.0, 0.0), 0.01));
		TestEqual(TEXT("for the dome's frames"), Frames, 12);
	}

	// A hit on the body of 2, inside dome 1, from outside: the same dome pushes
	Query.VictimId = 2;
	Query.HurtboxIndex = 0;
	TestTrue(TEXT("a body inside a dome, hit from outside: pushed"), Push.GetPush(Query, Velocity, Frames) && Velocity.Equals(FVector(700.0, 0.0, 0.0), 0.01));

	// The attacker inside the dome: that hit was never blocked, nothing pushes
	Query.AttackerRoot = FVector(300.0, 0.0, 0.0);
	TestFalse(TEXT("an attacker inside is not pushed"), Push.GetPush(Query, Velocity, Frames));

	// A body hit on an entity in no dome: not the backpack's ricochet
	Query.VictimId = 1;
	Snapshot.Entries[0].Extension = FGSTN_SnapshotExtensionSet();
	Query.AttackerRoot = FVector(500.0, 0.0, 0.0);
	TestFalse(TEXT("not a dome's ricochet"), Push.GetPush(Query, Velocity, Frames));

	// Back along the attack: minus the facing, whatever side it stands on
	Snapshot.Entries[0] = MakeEntry(1, FVector::ZeroVector, 400.f, 70, 12, true);
	Query.HurtboxIndex = GSTNHurtboxIndex::ExtraIndexBase;
	TestTrue(TEXT("back along the attack"), Push.GetPush(Query, Velocity, Frames) && Velocity.Equals(FVector(420.0, 560.0, 0.0), 0.01));

	// A dome whose data has no push: the attacker only recoils
	Query.VictimId = 4;
	Query.AttackerRoot = FVector(3500.0, 0.0, 0.0);
	TestFalse(TEXT("zero speed: no push"), Push.GetPush(Query, Velocity, Frames));
	return true;
}

#endif
