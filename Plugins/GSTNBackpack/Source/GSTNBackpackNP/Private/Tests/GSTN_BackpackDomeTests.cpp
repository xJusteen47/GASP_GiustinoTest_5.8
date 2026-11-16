// Automation tests of the dome blocking hits (M11 L7c): the snapshot contributor (blob, shell, inside lists), the BackpackDome step's
// verdicts, the attacker outcome and the content helper. No world: plain data, the step's pure function and a transient rules object.

#include "Misc/AutomationTest.h"
#include "GSTN_BackpackContentLibrary.h"
#include "GSTN_BackpackDomeOutcome.h"
#include "GSTN_BackpackDomeSnapshot.h"
#include "GSTN_BackpackSim.h"
#include "GSTN_BackpackTypes.h"
#include "GSTN_HitStep_BackpackDome.h"
#include "Hits/GSTN_HitSteps.h"
#include "Rules/GSTN_CombatRules.h"
#include "Sync/GSTN_SyncFragment.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_BACKPACK_DOME_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNBackpackDomeTest
{
	/** A fragment array holding a dome of Radius at Center, raised on Frame (Deploy for DurationFrames, so it has an end) */
	FGSTN_SyncFragmentArray MakeDomeFragments(const FVector& Center, int32 Radius, int32 Frame, int32 DurationFrames = 100)
	{
		FGSTN_SyncFragmentArray Fragments;
		FGSTN_BackpackFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_BackpackFragment>(Fragments);
		FGSTN_BackpackDomeTrack Track;
		Track.RadiusCm = Radius;
		Track.Lifetime = EGSTN_DomeLifetime::Deploy;
		Track.DurationFrames = DurationFrames;
		FGSTN_BackpackStep::RaiseDome(Fragment, Frame, Track, Center, 0, 500);
		return Fragments;
	}

	/** A snapshot entry of Id at Location, with the blob the contributor would write for a dome there (none when Radius is 0) */
	FGSTN_SnapshotEntry MakeEntry(int32 Id, const FVector& Location, float Radius = 0.f)
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
			Entry.Extension.Set(GSTNFeatureIds::Backpack, Ext);
		}
		return Entry;
	}

	FGSTN_BackpackSnapshotExt MakeInside(std::initializer_list<int32> Domes)
	{
		FGSTN_BackpackSnapshotExt Ext;
		for (const int32 Dome : Domes)
		{
			Ext.AddInside(Dome);
		}
		return Ext;
	}

	/** A hit on victim 9's body (hurtbox 0) by attacker 3; the pointers into the extensions live as long as the caller's structs */
	FGSTN_HitContextView MakeView(const FGSTN_BackpackSnapshotExt* Victim, const FGSTN_BackpackSnapshotExt* Attacker)
	{
		FGSTN_HitContextView View;
		View.Victim.EntityId = 9;
		View.Incoming.AttackerEntityId = 3;
		View.HurtboxIndex = 0;
		if (Victim)
		{
			FGSTN_HitVictimView::FSnapshotExtensionRef Ref;
			Ref.TypeId = GSTNFeatureIds::Backpack;
			Ref.Data = Victim;
			View.Victim.SetSnapshotExtensions(&Ref, 1);
		}
		if (Attacker)
		{
			View.Incoming.AttackerExtensions[0].TypeId = GSTNFeatureIds::Backpack;
			View.Incoming.AttackerExtensions[0].Data = Attacker;
		}
		return View;
	}

	const FGSTN_BackpackSnapshotExt* GetExt(const FGSTN_SnapshotEntry& Entry)
	{
		return Entry.Extension.Get<FGSTN_BackpackSnapshotExt>(GSTNFeatureIds::Backpack);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackDomeContributorTest, "GSTNBackpack.NP.Dome.ContributorBlobAndShell", GSTN_BACKPACK_DOME_TEST_FLAGS)
bool FGSTNBackpackDomeContributorTest::RunTest(const FString& Parameters)
{
	using namespace GSTNBackpackDomeTest;
	const FGSTN_BackpackSnapshotContributor Contributor;
	TestEqual(TEXT("the backpack feature id"), static_cast<int32>(Contributor.GetTypeId()), static_cast<int32>(GSTNFeatureIds::Backpack));

	// A dome of 400 cm at (500, 0, 0) raised on frame 10 for 100 frames; the carrier has walked to (600, 0, 0) since
	const FGSTN_SyncFragmentArray Fragments = MakeDomeFragments(FVector(500.0, 0.0, 0.0), 400, 10);
	FGSTN_SnapshotContributorContext Context;
	Context.Frame = 11;
	Context.Fragments = &Fragments;
	Context.Transform = FTransform(FVector(600.0, 0.0, 0.0));

	FGSTN_SnapshotExtensionBlob Blob;
	Contributor.Contribute(Context, Blob);
	const FGSTN_BackpackSnapshotExt* Ext = Blob.Get<FGSTN_BackpackSnapshotExt>(GSTNFeatureIds::Backpack);
	if (!TestNotNull(TEXT("a blob while the dome is up"), Ext))
	{
		return false;
	}
	TestTrue(TEXT("with the dome"), Ext->HasDome());
	TestEqual(TEXT("its centre"), Ext->GetDomeCenter(), FVector(500.0, 0.0, 0.0));
	TestEqual(TEXT("its radius"), Ext->DomeRadiusCm, 400.f);
	TestEqual(TEXT("nobody inside yet (the finalize pass fills it)"), static_cast<int32>(Ext->NumInside), 0);

	FGSTN_ExtraHurtboxList Extras;
	Contributor.AppendExtraHurtboxes(Context, Extras);
	if (TestEqual(TEXT("one extra: the shell"), Extras.Num(), 1))
	{
		const FGSTN_ExtraHurtbox& Shell = Extras[0];
		TestTrue(TEXT("a sphere"), Shell.Shape.Type == EGSTN_ShapeType::Sphere);
		TestEqual(TEXT("of the dome's radius"), Shell.Shape.Extent.X, 400.0);
		TestEqual(TEXT("root-relative: the world centre seen from the carrier"), Shell.Shape.Local.GetLocation(), FVector(-100.0, 0.0, 0.0));
		TestEqual(TEXT("above every body hurtbox"), static_cast<int32>(Shell.Priority), 255);
		TestEqual(TEXT("from the backpack"), static_cast<int32>(Shell.Source), static_cast<int32>(GSTNFeatureIds::Backpack));
		TestTrue(TEXT("outside only"), Shell.bOutsideOnly);
		TestTrue(TEXT("lets no hit type through (grabs too)"), Shell.IgnoreHitTypes.IsEmpty());
		TestTrue(TEXT("the carrier's root is inside its shell"), Shell.IsSkippedFrom(FVector(600.0, 0.0, 0.0), Context.Transform));
	}

	// The frame the deployed dome ends on reads it down
	Context.Frame = 110;
	Contributor.Contribute(Context, Blob);
	TestFalse(TEXT("no blob once the dome is down"), Blob.IsSet());
	Extras.Reset();
	Contributor.AppendExtraHurtboxes(Context, Extras);
	TestEqual(TEXT("and no shell"), Extras.Num(), 0);

	// No fragment at all: nothing, at the cost of one lookup
	FGSTN_SnapshotContributorContext Bare;
	Bare.Frame = 11;
	Contributor.Contribute(Bare, Blob);
	TestFalse(TEXT("no fragment, no blob"), Blob.IsSet());

	// A rebuild in the victim drain keeps the inside list of the build (it cannot see the other entries)
	FGSTN_SnapshotExtensionBlob Built;
	Built.Set(GSTNFeatureIds::Backpack, MakeInside({ 4 }));
	FGSTN_SnapshotContributorContext Rebuild;
	Rebuild.Frame = 11;
	Rebuild.Previous = &Built;
	Contributor.Contribute(Rebuild, Blob);
	const FGSTN_BackpackSnapshotExt* Kept = Blob.Get<FGSTN_BackpackSnapshotExt>(GSTNFeatureIds::Backpack);
	TestTrue(TEXT("the rebuild keeps who it stands in"), Kept && Kept->NumInside == 1 && Kept->InsideDomes[0] == 4 && !Kept->HasDome());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackDomeFinalizeTest, "GSTNBackpack.NP.Dome.FinalizeInsideLists", GSTN_BACKPACK_DOME_TEST_FLAGS)
bool FGSTNBackpackDomeFinalizeTest::RunTest(const FString& Parameters)
{
	using namespace GSTNBackpackDomeTest;
	const FGSTN_BackpackSnapshotContributor Contributor;

	// No dome anywhere: the pass leaves every entry as it was
	TArray<FGSTN_SnapshotEntry> Quiet = { MakeEntry(1, FVector::ZeroVector), MakeEntry(2, FVector(100.0, 0.0, 0.0)) };
	Contributor.FinalizeSnapshot(20, Quiet);
	TestFalse(TEXT("no dome: no blob added"), Quiet[0].Extension.IsSet() || Quiet[1].Extension.IsSet());

	// Domes of 1 (at the origin) and 4 (at 600), 400 cm each; 2 stands between them, 3 far away
	TArray<FGSTN_SnapshotEntry> Entries = {
		MakeEntry(1, FVector::ZeroVector, 400.f),
		MakeEntry(2, FVector(300.0, 0.0, 0.0)),
		MakeEntry(3, FVector(1500.0, 0.0, 0.0)),
		MakeEntry(4, FVector(600.0, 0.0, 0.0), 400.f) };
	Contributor.FinalizeSnapshot(20, Entries);

	const FGSTN_BackpackSnapshotExt* One = GetExt(Entries[0]);
	TestTrue(TEXT("a carrier stands in its own dome only"), One && One->HasDome() && One->NumInside == 1 && One->InsideDomes[0] == 1);
	const FGSTN_BackpackSnapshotExt* Two = GetExt(Entries[1]);
	TestTrue(TEXT("an entity without a backpack gets a blob: in both domes, ascending"), Two && !Two->HasDome() && Two->NumInside == 2 && Two->InsideDomes[0] == 1 && Two->InsideDomes[1] == 4);
	TestNull(TEXT("an entity outside every dome gets nothing"), GetExt(Entries[2]));
	const FGSTN_BackpackSnapshotExt* Four = GetExt(Entries[3]);
	TestTrue(TEXT("the second carrier keeps its dome and is in its own"), Four && Four->HasDome() && Four->NumInside == 1 && Four->InsideDomes[0] == 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackDomeStepTest, "GSTNBackpack.NP.Dome.StepVerdicts", GSTN_BACKPACK_DOME_TEST_FLAGS)
bool FGSTNBackpackDomeStepTest::RunTest(const FString& Parameters)
{
	using namespace GSTNBackpackDomeTest;
	const UGSTN_HitStep_BackpackDome* Step = GetDefault<UGSTN_HitStep_BackpackDome>();

	// The shell: a Backpack-source extra of the victim, hit (so from outside) whatever the snapshot lists say
	{
		FGSTN_HitContextView View = MakeView(nullptr, nullptr);
		FGSTN_ExtraHurtbox& Shell = View.Victim.ExtraHurtboxes.AddDefaulted_GetRef();
		Shell.Source = GSTNFeatureIds::Backpack;
		Shell.bOutsideOnly = true;
		View.HurtboxIndex = GSTNHurtboxIndex::FromExtraPosition(0);
		FGSTN_StepResult Out;
		TestTrue(TEXT("a shell hit stops the pipeline"), Step->Resolve_Implementation(View, Out) == EGSTN_StepResult::Stop);
		TestTrue(TEXT("blocked, no damage, no flinch"), Out.bBlocked && Out.bNoDamage && Out.bNoFlinch);
		TestTrue(TEXT("no reaction and no hitstop"), Out.bHasKnockback && !Out.Knockback.bStaggered && Out.Knockback.HitstopFrames == 0 && Out.Knockback.AttackerHitstopFrames == 0);
		const FGSTN_BackpackDomeBlock* Block = Out.OutcomeExtensions.Num() == 1 ? Out.OutcomeExtensions[0].GetPtr<FGSTN_BackpackDomeBlock>() : nullptr;
		TestTrue(TEXT("the block names the shell's owner"), Block && Block->DomeOwnerId == 9);

		// Another plugin's extra (a shield bubble) is not the dome's
		View.Victim.ExtraHurtboxes[0].Source = GSTNFeatureIds::Defence;
		TestEqual(TEXT("a bubble is not a dome"), UGSTN_HitStep_BackpackDome::FindBlockingDome(View), static_cast<int32>(INDEX_NONE));
	}

	// The body of a victim inside dome 1, by an attacker outside it (no blob, or a blob without it)
	const FGSTN_BackpackSnapshotExt InOne = MakeInside({ 1 });
	const FGSTN_BackpackSnapshotExt InNone = MakeInside({});
	TestEqual(TEXT("an attacker without a blob is outside"), UGSTN_HitStep_BackpackDome::FindBlockingDome(MakeView(&InOne, nullptr)), 1);
	TestEqual(TEXT("an attacker in no dome is outside"), UGSTN_HitStep_BackpackDome::FindBlockingDome(MakeView(&InOne, &InNone)), 1);

	// Both inside the same dome: an ordinary hit
	{
		const FGSTN_HitContextView View = MakeView(&InOne, &InOne);
		FGSTN_StepResult Out;
		TestTrue(TEXT("both inside: the hit goes on"), Step->Resolve_Implementation(View, Out) == EGSTN_StepResult::Continue);
		TestTrue(TEXT("untouched"), !Out.bBlocked && !Out.bNoDamage && Out.OutcomeExtensions.IsEmpty());
	}

	// Two domes: the victim in 1 and 4, the attacker in 1 only, so it hits from outside 4
	const FGSTN_BackpackSnapshotExt InBoth = MakeInside({ 1, 4 });
	TestEqual(TEXT("two domes: blocked by the one the attacker is not in"), UGSTN_HitStep_BackpackDome::FindBlockingDome(MakeView(&InBoth, &InOne)), 4);
	TestEqual(TEXT("the attacker in both: not blocked"), UGSTN_HitStep_BackpackDome::FindBlockingDome(MakeView(&InBoth, &InBoth)), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("an attacker inside, a victim outside: not the dome's business"), UGSTN_HitStep_BackpackDome::FindBlockingDome(MakeView(&InNone, &InOne)), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("no dome anywhere: nothing"), UGSTN_HitStep_BackpackDome::FindBlockingDome(MakeView(nullptr, nullptr)), static_cast<int32>(INDEX_NONE));

	// A hit an earlier step already refused stays refused and untouched
	{
		FGSTN_HitContextView View = MakeView(&InOne, nullptr);
		View.Outcome.bHit = false;
		FGSTN_StepResult Out;
		TestTrue(TEXT("a refused hit is not blocked again"), Step->Resolve_Implementation(View, Out) == EGSTN_StepResult::Continue && Out.OutcomeExtensions.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackDomeOutcomeTest, "GSTNBackpack.NP.Dome.AttackerOutcomeAndEvent", GSTN_BACKPACK_DOME_TEST_FLAGS)
bool FGSTNBackpackDomeOutcomeTest::RunTest(const FString& Parameters)
{
	const FGSTN_BackpackDomeAttackerOutcome Outcome;
	FGSTN_HitContextView View;
	TestTrue(TEXT("no extension: Normal"), Outcome.Evaluate(View) == EGSTN_AttackerHitOutcome::Normal);
	View.OutcomeExtensions.Add(FInstancedStruct::Make(FGSTN_HitVictimStat()));
	TestTrue(TEXT("another struct: Normal"), Outcome.Evaluate(View) == EGSTN_AttackerHitOutcome::Normal);
	FGSTN_BackpackDomeBlock Block;
	Block.DomeOwnerId = 9;
	View.OutcomeExtensions.Add(FInstancedStruct::Make(Block));
	TestTrue(TEXT("a dome block: Ricochet"), Outcome.Evaluate(View) == EGSTN_AttackerHitOutcome::Ricochet);
	TestTrue(TEXT("and the module registered it"), FGSTN_AttackerOutcomeRegistry::Get().Evaluate(nullptr, View) == EGSTN_AttackerHitOutcome::Ricochet);

	const FGSTN_BackpackDomeEventApplier Applier;
	TestTrue(TEXT("the applier owns the block"), Applier.GetExtensionStruct() == FGSTN_BackpackDomeBlock::StaticStruct());
	FGSTN_SimEvent Envelope;
	Envelope.Frame = 33;
	FGSTN_SimEventOutbox Outbox;
	Applier.AppendEvents(FInstancedStruct::Make(Block), Envelope, Outbox);
	TestTrue(TEXT("one DomeBlocked event in the hit's envelope"), Outbox.Events.Num() == 1 && Outbox.Events[0].EventType == GSTNBackpackTags::Event_DomeBlocked.GetTag() && Outbox.Events[0].Frame == 33);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackDomeRulesTest, "GSTNBackpack.NP.Dome.AddStepToRules", GSTN_BACKPACK_DOME_TEST_FLAGS)
bool FGSTNBackpackDomeRulesTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_CombatRules> Rules(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
	const FSoftObjectPath Dome(UGSTN_HitStep_BackpackDome::StaticClass());
	const FSoftObjectPath TeamFilter(UGSTN_HitStep_TeamFilter::StaticClass());
	auto IndexOf = [&Rules](const FSoftObjectPath& Path)
	{
		return Rules->HitPipeline.IndexOfByPredicate([&Path](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step) { return Step.ToSoftObjectPath() == Path; });
	};
	TestTrue(TEXT("added"), UGSTN_BackpackContentLibrary::AddDomeStepToRules(Rules.Get()));
	TestTrue(TEXT("right after TeamFilter"), IndexOf(TeamFilter) != INDEX_NONE && IndexOf(Dome) == IndexOf(TeamFilter) + 1);
	TestTrue(TEXT("and resolved"), Rules->GetResolvedHitPipeline().ContainsByPredicate([](const TSubclassOf<UGSTN_HitResolutionStep>& Step) { return Step.Get() == UGSTN_HitStep_BackpackDome::StaticClass(); }));
	const int32 Num = Rules->HitPipeline.Num();
	TestTrue(TEXT("a second call holds it already"), UGSTN_BackpackContentLibrary::AddDomeStepToRules(Rules.Get()));
	TestEqual(TEXT("without adding it twice"), Rules->HitPipeline.Num(), Num);
	TestTrue(TEXT("removed"), UGSTN_BackpackContentLibrary::RemoveDomeStepFromRules(Rules.Get()));
	TestFalse(TEXT("removed once"), UGSTN_BackpackContentLibrary::RemoveDomeStepFromRules(Rules.Get()));
	TestFalse(TEXT("no rules, nothing"), UGSTN_BackpackContentLibrary::AddDomeStepToRules(nullptr));
	return true;
}

#endif
