// Automation tests of the defence snapshot contributor (M6 F7, spec 5.3 "Contact" and 9): the bubble is contributed as an extra hurtbox
// while it exists, never when the shield is unavailable, broken or the victim is in a hit reaction, and the extension carries each
// entity's rules index. No world: the contributor reads a context and the attach table, the snapshot is plain data.

#include "GSTN_DefenceNPTestFixture.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Math/GSTN_DefenceMath.h"
#include "Sim/GSTN_DefenceSnapshotContributor.h"
#include "Sim/GSTN_DefenceSnapshotExt.h"
#include "Simulation/GSTN_CombatVictim.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNDefenceNPTest;

namespace
{
	const FGSTN_DefenceSnapshotExt* DefExtOf(const FGSTN_SnapshotEntry& Entry)
	{
		return Entry.Extension.Get<FGSTN_DefenceSnapshotExt>(GSTNFeatureIds::Defence);
	}

	/** Fills an entry for Owner at Frame from Fragments through a registry holding the defence contributor alone */
	FGSTN_SnapshotEntry DefFill(const AActor* Owner, int32 Frame, const FGSTN_SyncFragmentArray& Fragments, EGSTN_ActionState ActionState = EGSTN_ActionState::Actionable)
	{
		FGSTN_DefenceSnapshotContributor Contributor;
		FGSTN_SnapshotContributorRegistry Registry;
		Registry.Register(&Contributor);
		FGSTN_SnapshotContributorContext Context;
		Context.Frame = Frame;
		Context.Owner = Owner;
		Context.Fragments = &Fragments;
		Context.ActionState = ActionState;
		FGSTN_SnapshotEntry Entry;
		Entry.EntityId = 2;
		Registry.Fill(Context, Entry);
		return Entry;
	}

	FGameplayTagContainer DefIgnore()
	{
		FGameplayTagContainer Ignore;
		Ignore.AddTag(GSTNCombatHitTags::HitType_Grab.GetTag());
		Ignore.AddTag(GSTNCombatHitTags::HitType_Unblockable.GetTag());
		return Ignore;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceSnapshotBubbleTest, "GSTNDefence.NP.Snapshot.BubbleContributed", TestFlags)
bool FGSTN_DefenceSnapshotBubbleTest::RunTest(const FString& Parameters)
{
	FGSTN_DefenceAttach::Reset();
	UGSTN_DefenceRules* Rules = MakeRules();
	AActor* Owner = MakeOwner();
	TestTrue(TEXT("the rules attach"), FGSTN_DefenceAttach::Apply(Owner, Rules, true, DefIgnore()));

	// Raised at 100, snapshot of 112: the bubble exists, blocks (past the one startup frame) and the value drained 12 frames at 0.4
	FGSTN_SyncFragmentArray Fragments;
	SetFragment(Fragments, MakeRaised(100));
	const FGSTN_SnapshotEntry Entry = DefFill(Owner, 112, Fragments);
	const FGSTN_DefenceSnapshotExt* Ext = DefExtOf(Entry);
	TestNotNull(TEXT("the entity carries the defence extension"), Ext);
	if (!Ext)
	{
		return false;
	}
	TestTrue(TEXT("shield available: mode and kit"), Ext->IsShieldAvailable());
	TestTrue(TEXT("raised"), Ext->Has(GSTNDefenceSnapshotFlags::Raised));
	TestTrue(TEXT("blocks"), Ext->Has(GSTNDefenceSnapshotFlags::Blocks));
	TestTrue(TEXT("the bubble exists"), Ext->Has(GSTNDefenceSnapshotFlags::Bubble));
	TestEqual(TEXT("press frame"), Ext->ShieldPressFrame, 100);
	TestEqual(TEXT("still held"), Ext->ShieldReleaseFrame, INDEX_NONE);
	TestEqual(TEXT("the analytic value at the snapshot frame: 100 - 12 * 0.4 = 95.2, in quarter points"), Ext->GetShieldValue(), 95.25);
	TestEqual(TEXT("the rules index of the attachment"), static_cast<int32>(Ext->RulesIndex), static_cast<int32>(FGSTN_DefenceAttach::IndexOf(Rules)));

	TestEqual(TEXT("one extra hurtbox: the bubble"), Entry.ExtraHurtboxes.Num(), 1);
	if (Entry.ExtraHurtboxes.Num() == 1)
	{
		const FGSTN_ExtraHurtbox& Bubble = Entry.ExtraHurtboxes[0];
		const FGSTN_ExtraHurtbox Expected = UGSTN_DefenceMath::MakeShieldBubbleHurtbox(Rules->Shield, DefIgnore());
		TestEqual(TEXT("a sphere"), Bubble.Shape.Type, EGSTN_ShapeType::Sphere);
		TestEqual(TEXT("of the rules' radius, whatever the value"), Bubble.Shape.Extent.X, static_cast<double>(Rules->Shield.RadiusCm));
		TestEqual(TEXT("centred on the local up at the rules' height"), Bubble.Shape.Local.GetLocation(), Expected.Shape.Local.GetLocation());
		TestEqual(TEXT("the defence feature as its source"), static_cast<int32>(Bubble.Source), static_cast<int32>(GSTNFeatureIds::Defence));
		TestTrue(TEXT("grabs pass through it"), Bubble.IgnoreHitTypes.HasTagExact(GSTNCombatHitTags::HitType_Grab.GetTag()));
		TestTrue(TEXT("so do unblockable hits"), Bubble.IgnoreHitTypes.HasTagExact(GSTNCombatHitTags::HitType_Unblockable.GetTag()));
		TestEqual(TEXT("priority above the body (a tie goes to the extra)"), static_cast<int32>(Bubble.Priority), static_cast<int32>(Expected.Priority));
	}

	// The same radius at a low value: fixed bubble
	FGSTN_SyncFragmentArray Low;
	SetFragment(Low, MakeRaised(100, 3.0));
	const FGSTN_SnapshotEntry LowEntry = DefFill(Owner, 101, Low);
	TestEqual(TEXT("a nearly empty shield has the same bubble"), LowEntry.ExtraHurtboxes.Num(), 1);
	if (LowEntry.ExtraHurtboxes.Num() == 1)
	{
		TestEqual(TEXT("same radius"), LowEntry.ExtraHurtboxes[0].Shape.Extent.X, static_cast<double>(Rules->Shield.RadiusCm));
	}

	// The startup frame: the bubble exists from the press, blocks one frame later
	const FGSTN_SnapshotEntry OnPress = DefFill(Owner, 100, Fragments);
	TestTrue(TEXT("on the press frame the bubble exists"), DefExtOf(OnPress) && DefExtOf(OnPress)->Has(GSTNDefenceSnapshotFlags::Bubble));
	TestFalse(TEXT("but does not block yet"), DefExtOf(OnPress) && DefExtOf(OnPress)->Has(GSTNDefenceSnapshotFlags::Blocks));
	TestEqual(TEXT("the extra is contributed from the press frame"), OnPress.ExtraHurtboxes.Num(), 1);

	// Released at 120: the bubble outlasts the release by six frames, the value is the stamped one until the regen delay
	FGSTN_SyncFragmentArray Released;
	FGSTN_DefenceFragment Dropped = MakeRaised(100, 92.0);
	Dropped.ShieldStampFrame = 120;
	Dropped.ShieldFlags = 0;
	Dropped.ShieldReleaseFrame = 120;
	SetFragment(Released, Dropped);
	TestEqual(TEXT("five frames after the release the bubble is still there"), DefFill(Owner, 125, Released).ExtraHurtboxes.Num(), 1);
	TestEqual(TEXT("six frames after it is gone"), DefFill(Owner, 126, Released).ExtraHurtboxes.Num(), 0);
	const FGSTN_SnapshotEntry AfterRelease = DefFill(Owner, 125, Released);
	TestTrue(TEXT("the extension stays (the parry step reads the release frame)"), DefExtOf(AfterRelease) != nullptr);
	TestEqual(TEXT("with the release frame"), DefExtOf(AfterRelease) ? DefExtOf(AfterRelease)->ShieldReleaseFrame : 0, 120);
	TestFalse(TEXT("not raised"), DefExtOf(AfterRelease) && DefExtOf(AfterRelease)->Has(GSTNDefenceSnapshotFlags::Raised));

	// The victim view hands the step the same bytes
	const FGSTN_HitVictimView View = FGSTN_CombatVictim::MakeVictimView(Entry, nullptr);
	TestTrue(TEXT("the victim view resolves the extension by feature id"), FGSTN_DefenceSnapshotExt::FromView(View) == Ext);
	TestEqual(TEXT("and copies the extras"), View.ExtraHurtboxes.Num(), 1);

	// No fragment yet (the entity never shielded): the extension is there with the full value and no bubble
	FGSTN_SyncFragmentArray None;
	const FGSTN_SnapshotEntry Fresh = DefFill(Owner, 50, None);
	TestNotNull(TEXT("a fresh entity with defence carries the extension"), DefExtOf(Fresh));
	TestEqual(TEXT("full value"), DefExtOf(Fresh) ? DefExtOf(Fresh)->GetShieldValue() : 0.0, 100.0);
	TestEqual(TEXT("no bubble"), Fresh.ExtraHurtboxes.Num(), 0);

	// An entity without defence contributes nothing at all
	const FGSTN_SnapshotEntry Other = DefFill(MakeOwner(), 112, Fragments);
	TestFalse(TEXT("no attachment: no extension"), Other.Extension.IsSet());
	TestEqual(TEXT("no attachment: no bubble"), Other.ExtraHurtboxes.Num(), 0);

	FGSTN_DefenceAttach::Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceSnapshotNoBubbleTest, "GSTNDefence.NP.Snapshot.NoBubbleWhenUnavailableOrHitstunOrBroken", TestFlags)
bool FGSTN_DefenceSnapshotNoBubbleTest::RunTest(const FString& Parameters)
{
	FGSTN_DefenceAttach::Reset();
	FGSTN_SyncFragmentArray Raised;
	SetFragment(Raised, MakeRaised(100));

	// The mode disables the shield (PvE): the extension is contributed (a move's parry window needs it), the bubble is not
	{
		AActor* Owner = MakeOwner();
		FGSTN_DefenceAttach::Apply(Owner, MakeRules(false), true, DefIgnore());
		const FGSTN_SnapshotEntry Entry = DefFill(Owner, 110, Raised);
		TestNotNull(TEXT("PvE: the extension is there"), DefExtOf(Entry));
		TestFalse(TEXT("PvE: the shield is not available"), DefExtOf(Entry) && DefExtOf(Entry)->IsShieldAvailable());
		TestFalse(TEXT("PvE: no Blocks flag"), DefExtOf(Entry) && DefExtOf(Entry)->Has(GSTNDefenceSnapshotFlags::Blocks));
		TestEqual(TEXT("PvE: no bubble"), Entry.ExtraHurtboxes.Num(), 0);
	}

	// The kit has no shield: same
	{
		AActor* Owner = MakeOwner();
		FGSTN_DefenceAttach::Apply(Owner, MakeRules(true), false, DefIgnore());
		const FGSTN_SnapshotEntry Entry = DefFill(Owner, 110, Raised);
		TestFalse(TEXT("kit without a shield: not available"), DefExtOf(Entry) && DefExtOf(Entry)->IsShieldAvailable());
		TestEqual(TEXT("kit without a shield: no bubble"), Entry.ExtraHurtboxes.Num(), 0);
	}

	// A victim in a hit reaction has no bubble (the hit that staggered it ended the shield), whatever the fragment still says
	AActor* Owner = MakeOwner();
	FGSTN_DefenceAttach::Apply(Owner, MakeRules(true), true, DefIgnore());
	TestEqual(TEXT("actionable: the bubble"), DefFill(Owner, 110, Raised).ExtraHurtboxes.Num(), 1);
	TestEqual(TEXT("in hitstun: none"), DefFill(Owner, 110, Raised, EGSTN_ActionState::Hitstun).ExtraHurtboxes.Num(), 0);
	TestEqual(TEXT("in tumble: none"), DefFill(Owner, 110, Raised, EGSTN_ActionState::Tumble).ExtraHurtboxes.Num(), 0);
	TestEqual(TEXT("in a timeline (the shield's own): the bubble"), DefFill(Owner, 110, Raised, EGSTN_ActionState::InTimeline).ExtraHurtboxes.Num(), 1);

	// A broken shield has no bubble and does not block, however the press and release frames read
	FGSTN_SyncFragmentArray Broken;
	FGSTN_DefenceFragment Breaking = MakeRaised(100, 0.0);
	Breaking.ShieldFlags = GSTNShieldFlags::Breaking;
	Breaking.BreakEndFrame = 260;
	SetFragment(Broken, Breaking);
	const FGSTN_SnapshotEntry BrokenEntry = DefFill(Owner, 110, Broken);
	TestTrue(TEXT("broken: the flag"), DefExtOf(BrokenEntry) && DefExtOf(BrokenEntry)->Has(GSTNDefenceSnapshotFlags::Breaking));
	TestFalse(TEXT("broken: no Blocks"), DefExtOf(BrokenEntry) && DefExtOf(BrokenEntry)->Has(GSTNDefenceSnapshotFlags::Blocks));
	TestEqual(TEXT("broken: no bubble"), BrokenEntry.ExtraHurtboxes.Num(), 0);

	// Shield stunned after a release: the bubble stays up through the stun
	FGSTN_SyncFragmentArray Stunned;
	FGSTN_DefenceFragment Stun = MakeRaised(100, 80.0);
	Stun.ShieldStunEndFrame = 130;
	SetFragment(Stunned, Stun);
	TestEqual(TEXT("shield stunned: the bubble stays"), DefFill(Owner, 128, Stunned).ExtraHurtboxes.Num(), 1);

	// Before the press frame of a fragment that was never raised (a release long ago): nothing
	FGSTN_SyncFragmentArray Old;
	FGSTN_DefenceFragment Past = MakeRaised(10, 50.0);
	Past.ShieldFlags = 0;
	Past.ShieldReleaseFrame = 20;
	SetFragment(Old, Past);
	TestEqual(TEXT("long after a release: no bubble"), DefFill(Owner, 110, Old).ExtraHurtboxes.Num(), 0);

	FGSTN_DefenceAttach::Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceSnapshotRulesIndexTest, "GSTNDefence.NP.Snapshot.RulesIndexPerEntity", TestFlags)
bool FGSTN_DefenceSnapshotRulesIndexTest::RunTest(const FString& Parameters)
{
	FGSTN_DefenceAttach::Reset();
	UGSTN_DefenceRules* PvP = MakeRules(true);
	UGSTN_DefenceRules* PvE = MakeRules(false);
	AActor* First = MakeOwner();
	AActor* Second = MakeOwner();
	AActor* Third = MakeOwner();
	FGSTN_DefenceAttach::Apply(First, PvP, true, DefIgnore());
	FGSTN_DefenceAttach::Apply(Second, PvE, true, DefIgnore());
	FGSTN_DefenceAttach::Apply(Third, PvP, false, DefIgnore());
	TestEqual(TEXT("three entities attached"), FGSTN_DefenceAttach::Num(), 3);

	FGSTN_SyncFragmentArray None;
	const FGSTN_SnapshotEntry A = DefFill(First, 10, None);
	const FGSTN_SnapshotEntry B = DefFill(Second, 10, None);
	const FGSTN_SnapshotEntry C = DefFill(Third, 10, None);
	const FGSTN_DefenceSnapshotExt* ExtA = DefExtOf(A);
	const FGSTN_DefenceSnapshotExt* ExtB = DefExtOf(B);
	const FGSTN_DefenceSnapshotExt* ExtC = DefExtOf(C);
	TestTrue(TEXT("every entity carries an extension"), ExtA && ExtB && ExtC);
	if (!ExtA || !ExtB || !ExtC)
	{
		return false;
	}

	// Two assets, two indices; the same asset shares its index; the table resolves them back
	TestNotEqual(TEXT("different rules, different indices"), static_cast<int32>(ExtA->RulesIndex), static_cast<int32>(ExtB->RulesIndex));
	TestEqual(TEXT("the same rules share an index"), static_cast<int32>(ExtA->RulesIndex), static_cast<int32>(ExtC->RulesIndex));
	TestTrue(TEXT("index A resolves to the PvP asset"), FGSTN_DefenceAttach::RulesAt(ExtA->RulesIndex) == PvP);
	TestTrue(TEXT("index B resolves to the PvE asset"), FGSTN_DefenceAttach::RulesAt(ExtB->RulesIndex) == PvE);
	TestTrue(TEXT("an unknown index resolves to nothing"), FGSTN_DefenceAttach::RulesAt(200) == nullptr);
	TestEqual(TEXT("a null asset has the null index"), static_cast<int32>(FGSTN_DefenceAttach::IndexOf(nullptr)), 255);

	// The flags follow the attachment, not the asset alone
	TestTrue(TEXT("A: enabled and kit shield"), ExtA->IsShieldAvailable());
	TestTrue(TEXT("B: the mode disables it"), ExtB->Has(GSTNDefenceSnapshotFlags::KitHasShield) && !ExtB->Has(GSTNDefenceSnapshotFlags::ShieldEnabled));
	TestTrue(TEXT("C: the kit has none"), ExtC->Has(GSTNDefenceSnapshotFlags::ShieldEnabled) && !ExtC->Has(GSTNDefenceSnapshotFlags::KitHasShield));

	// Detaching removes the entity; the table keeps the index (a history record may still hold it)
	FGSTN_DefenceAttach::Clear(First);
	TestEqual(TEXT("two left"), FGSTN_DefenceAttach::Num(), 2);
	TestTrue(TEXT("the index still resolves after a detach"), FGSTN_DefenceAttach::RulesAt(ExtA->RulesIndex) == PvP);
	TestFalse(TEXT("the detached entity contributes nothing"), DefFill(First, 11, None).Extension.IsSet());

	// A rebuild inside the victim drain has no owner: the earlier blob carries the index and the flags forward
	FGSTN_DefenceSnapshotContributor Contributor;
	FGSTN_SnapshotContributorRegistry Registry;
	Registry.Register(&Contributor);
	FGSTN_SnapshotContributorContext Rebuild;
	Rebuild.Frame = 11;
	Rebuild.Fragments = &None;
	FGSTN_SnapshotExtensionSet Set = B.Extension;
	Registry.Rebuild(Rebuild, Set);
	const FGSTN_DefenceSnapshotExt* Rebuilt = Set.Get<FGSTN_DefenceSnapshotExt>(GSTNFeatureIds::Defence);
	TestTrue(TEXT("the rebuild keeps the rules index"), Rebuilt && Rebuilt->RulesIndex == ExtB->RulesIndex);
	TestTrue(TEXT("and the availability flags"), Rebuilt && Rebuilt->Flags == ExtB->Flags);

	FGSTN_DefenceAttach::Reset();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
