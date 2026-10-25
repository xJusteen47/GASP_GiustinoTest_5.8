// Automation tests of the parts tick extension and of the hurtbox policy of the snapshot contributor (P7, combat-parts.md §3.3, §3.5, §5.3).

#include "GSTN_PartsNPTestFixture.h"
#include "GSTN_PartsTags.h"
#include "Sim/GSTN_PartsSnapshotContributor.h"
#include "Sim/GSTN_PartsTickExtension.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTickExtension.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNPartsNPTest;

namespace
{
	/** Records the type id of each call at one hook point; changes nothing */
	class FRecordingExtension : public IGSTN_CombatTickExtension
	{
	public:
		FRecordingExtension(uint8 InTypeId, int32 InPriority, TArray<uint8>& InLog) : TypeId(InTypeId), Priority(InPriority), Log(InLog) {}

		virtual uint8 GetTypeId() const override { return TypeId; }
		virtual int32 GetPriority() const override { return Priority; }
		virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) override
		{
			if (Point == EGSTN_CombatHookPoint::BeforeCandidates)
			{
				Log.Add(TypeId);
			}
		}

	private:
		uint8 TypeId;
		int32 Priority;
		TArray<uint8>& Log;
	};

	/** Counts the calls the parts extension makes to the context's hit group mask */
	struct FMaskRecorder : IGSTN_CombatTickExtension
	{
		FGSTN_HitGroupMask Seen;
		virtual uint8 GetTypeId() const override { return 9; }
		virtual int32 GetPriority() const override { return 200; }
		virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) override
		{
			if (Point == EGSTN_CombatHookPoint::BeforeCandidates)
			{
				Seen = Context.HitGroupMask;
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsTickOrderTest, "GSTNParts.NP.TickExtension.OrderByPriority", GSTNPartsNPTest::Flags)
bool FGSTN_PartsTickOrderTest::RunTest(const FString& Parameters)
{
	FGSTN_PartsTickExtension Parts;
	TestEqual(TEXT("the type id is the parts feature id"), Parts.GetTypeId(), GSTNFeatureIds::Parts);
	TestEqual(TEXT("the priority is explicit"), Parts.GetPriority(), FGSTN_PartsTickExtension::ExtensionPriority);
	TestTrue(TEXT("and after the default priority"), FGSTN_PartsTickExtension::ExtensionPriority > 0);

	TArray<uint8> Log;
	FRecordingExtension Early(40, 0, Log);
	FRecordingExtension LateLowId(3, FGSTN_PartsTickExtension::ExtensionPriority, Log);
	FRecordingExtension After(1, 500, Log);
	// Registered in an order that is not the run order
	FGSTN_CombatTickExtensionRegistry Registry;
	TestTrue(TEXT("after registers"), Registry.Register(&After));
	TestTrue(TEXT("parts registers"), Registry.Register(&Parts));
	TestTrue(TEXT("early registers"), Registry.Register(&Early));
	TestTrue(TEXT("same priority as the parts, lower id"), Registry.Register(&LateLowId));

	FGSTN_CombatSyncState In, Out;
	FGSTN_CombatTickContext Context(1, In, Out);
	Registry.Run(EGSTN_CombatHookPoint::BeforeCandidates, Context);
	TestEqual(TEXT("early, then the priority of the parts by type id, then after"), Log, TArray<uint8>({ 40, 3, 1 }));

	// Nothing attached: the parts extension left the context alone
	TestTrue(TEXT("no set, no mask"), Context.HitGroupMask.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsTickTagsTest, "GSTNParts.NP.TickExtension.ProjectsTagsFromMask", GSTNPartsNPTest::Flags)
bool FGSTN_PartsTickTagsTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	FRig Rig;
	FGSTN_PartsAttach::ApplyPartSet(Rig.Owner, Fixture.Set.Get(), Rig.ASC);
	TestEqual(TEXT("the marker tag is granted at attach"), Rig.ASC->GetTagCount(GSTNPartsTags::Entity_Destructible), 1);

	FGSTN_PartsTickExtension Parts;
	FGSTN_CombatSyncState In, Out;
	SetFragment(Out.Fragments, MakeState(1u << Arm, 1), Fixture.Num());
	FGSTN_CombatTickContext Context(5, In, Out);
	Context.Owner = Rig.Owner;
	Context.AbilitySystem = Rig.ASC;

	Parts.OnCombatHook(EGSTN_CombatHookPoint::BeforeTagProjection, Context);
	TestEqual(TEXT("a broken part projects Part.Broken"), Rig.ASC->GetTagCount(GSTNPartsTags::Part_Broken), 1);
	TestEqual(TEXT("the gate (one broken part) is met: no GrabLocked"), Rig.ASC->GetTagCount(GSTNPartsTags::Part_GrabLocked), 0);
	TestEqual(TEXT("not all broken"), Rig.ASC->GetTagCount(GSTNPartsTags::Part_AllBroken), 0);

	// Projected again from the same state: nothing stacks
	Parts.OnCombatHook(EGSTN_CombatHookPoint::BeforeTagProjection, Context);
	TestEqual(TEXT("a second frame does not stack the tag"), Rig.ASC->GetTagCount(GSTNPartsTags::Part_Broken), 1);

	// A rollback to before the break: the state has no break and the tags go with it, with no memory of the previous frame
	Out.Fragments.Reset();
	Parts.OnCombatHook(EGSTN_CombatHookPoint::BeforeTagProjection, Context);
	TestEqual(TEXT("the restored state shows no break"), Rig.ASC->GetTagCount(GSTNPartsTags::Part_Broken), 0);
	TestEqual(TEXT("the whole entity is grab locked again"), Rig.ASC->GetTagCount(GSTNPartsTags::Part_GrabLocked), 1);

	// Every breakable part broken
	SetFragment(Out.Fragments, MakeState(0xFu, 4), Fixture.Num());
	Parts.OnCombatHook(EGSTN_CombatHookPoint::BeforeTagProjection, Context);
	TestEqual(TEXT("all parts broken"), Rig.ASC->GetTagCount(GSTNPartsTags::Part_AllBroken), 1);

	// No ability system in the context (a test or a machine part): the hook does nothing and does not crash
	Context.AbilitySystem = nullptr;
	Parts.OnCombatHook(EGSTN_CombatHookPoint::BeforeTagProjection, Context);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsTickHitGroupsTest, "GSTNParts.NP.TickExtension.HitGroupMaskFromBroken", GSTNPartsNPTest::Flags)
bool FGSTN_PartsTickHitGroupsTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	AActor* Owner = MakeOwner();
	FGSTN_PartsAttach::ApplyPartSet(Owner, Fixture.Set.Get(), nullptr);

	FGSTN_PartsTickExtension Parts;
	FMaskRecorder Recorder;
	FGSTN_CombatTickExtensionRegistry Registry;
	Registry.Register(&Recorder);
	Registry.Register(&Parts);

	FGSTN_CombatSyncState In, Out;
	FGSTN_CombatTickContext Context(8, In, Out);
	Context.Owner = Owner;

	// Pristine at the end of F-1: nothing is masked
	Registry.Run(EGSTN_CombatHookPoint::BeforeCandidates, Context);
	TestTrue(TEXT("whole: no group masked"), Recorder.Seen.IsEmpty());

	// The arm broke in F-1: its hit group stops, and only that one
	SetFragment(In.Fragments, MakeState(1u << Arm, 1), Fixture.Num());
	Context.HitGroupMask = FGSTN_HitGroupMask();
	Registry.Run(EGSTN_CombatHookPoint::BeforeCandidates, Context);
	TestTrue(TEXT("the arm's group is masked"), Recorder.Seen.Has(ArmHitGroup));
	TestFalse(TEXT("another group is not"), Recorder.Seen.Has(0));
	TestFalse(TEXT("nor the next one"), Recorder.Seen.Has(ArmHitGroup + 1));

	// A break only the frame being written knows about masks nothing yet: the mask is the end of F-1
	In.Fragments.Reset();
	SetFragment(Out.Fragments, MakeState(1u << Arm, 1), Fixture.Num());
	Context.HitGroupMask = FGSTN_HitGroupMask();
	Registry.Run(EGSTN_CombatHookPoint::BeforeCandidates, Context);
	TestTrue(TEXT("the state being written is not read"), Recorder.Seen.IsEmpty());

	// Another extension's bits are kept: the masks merge
	SetFragment(In.Fragments, MakeState(1u << Arm, 1), Fixture.Num());
	Context.HitGroupMask = FGSTN_HitGroupMask();
	Context.HitGroupMask.Set(5);
	Registry.Run(EGSTN_CombatHookPoint::BeforeCandidates, Context);
	TestTrue(TEXT("the union is masked: the other bit"), Recorder.Seen.Has(5));
	TestTrue(TEXT("the union is masked: ours"), Recorder.Seen.Has(ArmHitGroup));

	FGSTN_PartsAttach::Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsTickHiddenHurtboxTest, "GSTNParts.NP.TickExtension.HiddenHurtboxPolicy", GSTNPartsNPTest::Flags)
bool FGSTN_PartsTickHiddenHurtboxTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	AActor* Owner = MakeOwner();
	FGSTN_PartsAttach::ApplyPartSet(Owner, Fixture.Set.Get(), nullptr);

	FGSTN_PartsSnapshotContributor Contributor;
	FGSTN_SnapshotContributorRegistry Registry;
	Registry.Register(&Contributor);
	FGSTN_SnapshotContributorContext Context;
	Context.Frame = 3;
	Context.Owner = Owner;

	// The arm (policy Disable) and the hand (Keep) are broken
	FGSTN_SyncFragmentArray Fragments;
	SetFragment(Fragments, MakeState((1u << Arm) | (1u << Hand), 1), Fixture.Num());
	Context.Fragments = &Fragments;
	FGSTN_SnapshotEntry Entry = Fixture.MakeEntry();
	Registry.Fill(Context, Entry);

	TestEqual(TEXT("the broken arm's hurtbox is hidden"), Entry.HurtStates[Arm].State, EGSTN_HurtState::Intangible);
	TestEqual(TEXT("the Keep hand stays hittable"), Entry.HurtStates[Hand].State, EGSTN_HurtState::Normal);
	TestEqual(TEXT("an intact part stays hittable"), Entry.HurtStates[Head].State, EGSTN_HurtState::Normal);
	TestEqual(TEXT("so does the torso"), Entry.HurtStates[Torso].State, EGSTN_HurtState::Normal);

	// Whole again (a restore, a rollback to before the break): nothing is hidden, the state is derived and never stored
	FGSTN_SyncFragmentArray Whole;
	SetFragment(Whole, MakeState(0), Fixture.Num());
	Context.Fragments = &Whole;
	FGSTN_SnapshotEntry Again = Fixture.MakeEntry();
	Registry.Fill(Context, Again);
	TestEqual(TEXT("whole: the arm is hittable"), Again.HurtStates[Arm].State, EGSTN_HurtState::Normal);

	FGSTN_PartsAttach::Reset();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
