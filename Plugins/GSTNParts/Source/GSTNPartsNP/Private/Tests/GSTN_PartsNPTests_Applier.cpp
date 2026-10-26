// Automation tests of the parts outcome applier and of the rollback rules of combat-parts.md §5.4 (P7). The drain is the real
// FGSTN_CombatVictim::Resolve with the registered applier and contributor: no world, the snapshot is built by hand.

#include "GSTN_PartsNPTestFixture.h"
#include "Data/GSTN_PartHitData.h"
#include "Events/GSTN_PartEvents.h"
#include "GSTN_PartsTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Pipeline/GSTN_HitStep_PartResolve.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Rules/GSTN_CombatRules.h"
#include "Sim/GSTN_PartsOutcomeApplier.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "Simulation/GSTN_HitInbox.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNPartsNPTest;

namespace
{
	/** A victim entity with the parts fixture, the rules of the part step alone, and the hits an attacker's inbox would hold */
	struct FDrainFixture
	{
		FPartsFixture Parts;
		TStrongObjectPtr<UGSTN_CombatRules> Rules;
		FGSTN_SnapshotEntry Self;
		FGSTN_CombatSyncCore Core;
		FGSTN_SyncFragmentArray Fragments;
		FGSTN_VictimResult Result;
		TArray<FGSTN_HitInboxEntry> Entries;

		FDrainFixture()
		{
			Rules.Reset(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
			Rules->HitPipeline.Add(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_PartResolve::StaticClass()));
			Rules->ResolveHitPipeline();
			ResetSnapshot();
		}

		/** The snapshot of the victim as it was at the end of the previous frame: Fragments is what it is built from */
		void ResetSnapshot()
		{
			Self = Parts.MakeEntry();
			Self.Team = FGenericTeamId(2);
			Self.Reaction = FGSTN_VictimReactionState::Make(0.0, 1.0, 0, FGSTN_StaggerThreshold());
			FGSTN_PartsSnapshotExt Ext;
			Ext.PartSet = Parts.Set.Get();
			Ext.SetState(ReadState(Fragments));
			Self.Extension.Set(GSTNFeatureIds::Parts, Ext);
		}

		void AddHit(int32 HurtboxIndex, float Damage, int32 Penetration = 8)
		{
			FGSTN_HitInboxEntry Entry;
			Entry.Frame = 9;
			Entry.AttackerId = 1;
			Entry.AttackerTeam = 1;
			Entry.HurtboxIndex = static_cast<uint8>(HurtboxIndex);
			Entry.Hit.Damage = Damage;
			Entry.Hit.StaggerForceMode = EGSTN_StaggerForceMode::Override;
			Entry.Hit.StaggerForce = 50.f;
			FGSTN_PartHitData PartHit;
			PartHit.Penetration = static_cast<uint8>(Penetration);
			Entry.Hit.ExtensionData.Add(FInstancedStruct::Make(PartHit));
			Entry.HitLocation = FVector(-40.0, 0.0, 0.0);
			Entry.Relationship = GSTNHitRelationship::ToIndex(GSTNCombatHitTags::Relationship_Enemy.GetTag());
			Entries.Add(Entry);
		}

		void Run(int32 Frame = 10)
		{
			FGSTN_VictimTickInput In;
			In.Frame = Frame;
			In.Self = &Self;
			In.Rules = Rules.Get();
			In.Entries = Entries;
			In.Fragments = &Fragments;
			FGSTN_CombatVictim::Resolve(In, Core, Result);
		}

		const FGSTN_PartsSnapshotExt* SnapshotExt() const { return Self.Extension.Get<FGSTN_PartsSnapshotExt>(GSTNFeatureIds::Parts); }
	};

	const FGSTN_PartHitOutcome* OutcomeOf(const FGSTN_VictimHit& Hit)
	{
		for (const FInstancedStruct& Entry : Hit.OutcomeExtensions)
		{
			if (const FGSTN_PartHitOutcome* Found = Entry.GetPtr<FGSTN_PartHitOutcome>())
			{
				return Found;
			}
		}
		return nullptr;
	}

	FGSTN_SimEvent MakeEnvelope(AActor* Target)
	{
		FGSTN_SimEvent Envelope;
		Envelope.Frame = 10;
		Envelope.Target = Target;
		return Envelope;
	}

	/** The key of the first Broken event among the events, or an empty key */
	bool FindBrokenKey(const TArray<FGSTN_SimEvent>& Events, FGSTN_PartEventKey& OutKey)
	{
		for (const FGSTN_SimEvent& Event : Events)
		{
			if (Event.EventType == GSTNPartsTags::Event_Part_Broken.GetTag())
			{
				OutKey = FGSTN_PartEvents::MakeKey(Event);
				return true;
			}
		}
		return false;
	}

	/** The keys of the Broken events a drain's outcomes stand for, in order */
	TArray<FGSTN_PartEventKey> BrokenKeys(const FDrainFixture& Fixture, AActor* Target)
	{
		TArray<FGSTN_PartEventKey> Keys;
		for (const FGSTN_VictimHit& Hit : Fixture.Result.Hits)
		{
			const FGSTN_PartHitOutcome* Outcome = OutcomeOf(Hit);
			if (!Outcome)
			{
				continue;
			}
			TArray<FGSTN_SimEvent> Events;
			FGSTN_PartEvents::Make(*Outcome, MakeEnvelope(Target), Events);
			FGSTN_PartEventKey Key;
			if (FindBrokenKey(Events, Key))
			{
				Keys.Add(Key);
			}
		}
		return Keys;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsApplierWritesAtFPlusOneTest, "GSTNParts.NP.Applier.WritesAtFPlus1", GSTNPartsNPTest::TestFlags)
bool FGSTN_PartsApplierWritesAtFPlusOneTest::RunTest(const FString& Parameters)
{
	FDrainFixture Fixture;
	Fixture.AddHit(Head, 30.f);
	Fixture.Run();

	TestEqual(TEXT("one hit was drained"), Fixture.Result.Hits.Num(), 1);
	if (Fixture.Result.Hits.IsEmpty())
	{
		return false;
	}
	const FGSTN_PartsState After = ReadState(Fixture.Fragments);
	TestTrue(TEXT("the victim's fragment of F+1 holds the damage"), After.PartDamage[Head] > 0);
	TestEqual(TEXT("and no other part moved"), After.PartDamage[Torso], static_cast<uint8>(0));

	// The snapshot is the end of the previous frame and stays so: the step wrote nothing into it
	TestEqual(TEXT("the snapshot extension is untouched"), Fixture.SnapshotExt()->PartDamage[Head], static_cast<uint8>(0));
	const FGSTN_PartHitOutcome* Outcome = OutcomeOf(Fixture.Result.Hits[0]);
	TestTrue(TEXT("the hit's outcome is the source of the bytes"), Outcome && Outcome->GetResultState() == After);
	// The stored hit view never keeps a pointer into the drain's scratch
	TestNull(TEXT("no dangling extension pointer in the stored view"), Fixture.Result.Hits[0].View.Victim.SnapshotExtension);

	// Without a fragment array to write into the applier does nothing (a victim step run without fragments)
	FGSTN_PartsOutcomeApplier Applier;
	FGSTN_OutcomeApplyContext NoFragments;
	FGSTN_HitContextView View;
	Applier.Apply(Fixture.Result.Hits[0].OutcomeExtensions[0], View, NoFragments);
	TestNull(TEXT("nothing to write into"), NoFragments.VictimFragments);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsApplierDrainTest, "GSTNParts.NP.Applier.DrainRebuildsBetweenEntries", GSTNPartsNPTest::TestFlags)
bool FGSTN_PartsApplierDrainTest::RunTest(const FString& Parameters)
{
	// Two hits of 30 on a head that takes 40: the second breaks it only if it is resolved against what the first left
	FDrainFixture Single;
	Single.AddHit(Head, 30.f);
	Single.Run();
	TestEqual(TEXT("one hit does not break the head"), ReadState(Single.Fragments).BrokenMask, static_cast<uint16>(0));
	const uint8 AfterOne = ReadState(Single.Fragments).PartDamage[Head];
	TestTrue(TEXT("but hurts it"), AfterOne > 0 && AfterOne < 255);

	FDrainFixture Double;
	Double.AddHit(Head, 30.f);
	Double.AddHit(Head, 30.f);
	Double.Run();
	TestEqual(TEXT("both entries were drained"), Double.Result.Hits.Num(), 2);
	if (Double.Result.Hits.Num() != 2)
	{
		return false;
	}
	TestEqual(TEXT("the second one broke the head"), ReadState(Double.Fragments).BrokenMask, static_cast<uint16>(1u << Head));

	const FGSTN_PartHitOutcome* First = OutcomeOf(Double.Result.Hits[0]);
	const FGSTN_PartHitOutcome* Second = OutcomeOf(Double.Result.Hits[1]);
	if (!First || !Second)
	{
		AddError(TEXT("both entries carry a part outcome"));
		return false;
	}
	TestEqual(TEXT("the first leaves the head hurt"), First->NewlyBrokenMask, static_cast<uint16>(0));
	TestEqual(TEXT("the second starts from the first's damage"), Second->NewlyBrokenMask, static_cast<uint16>(1u << Head));
	TestEqual(TEXT("and the serial moved on once"), ReadState(Double.Fragments).BreakSerial, static_cast<uint8>(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsApplierRollbackTest, "GSTNParts.NP.Applier.RollbackRedeliveryCreatesBreakOnce", GSTNPartsNPTest::TestFlags)
bool FGSTN_PartsApplierRollbackTest::RunTest(const FString& Parameters)
{
	AActor* Target = MakeOwner();
	FDrainFixture Fixture;
	Fixture.AddHit(Head, 100.f);

	const FGSTN_SyncFragmentArray Restored = Fixture.Fragments;
	Fixture.Run();
	const FGSTN_PartsState First = ReadState(Fixture.Fragments);
	TestEqual(TEXT("the hit broke the head"), First.BrokenMask, static_cast<uint16>(1u << Head));
	const TArray<FGSTN_PartEventKey> FirstKeys = BrokenKeys(Fixture, Target);
	TestEqual(TEXT("one break event"), FirstKeys.Num(), 1);

	// The victim rolls back to before the hit and the attacker re-delivers the entry: same state, same key
	Fixture.Fragments = Restored;
	Fixture.ResetSnapshot();
	Fixture.Core = FGSTN_CombatSyncCore();
	Fixture.Run();
	TestTrue(TEXT("the redelivered hit lands on the same bytes"), ReadState(Fixture.Fragments) == First);
	const TArray<FGSTN_PartEventKey> SecondKeys = BrokenKeys(Fixture, Target);
	TestTrue(TEXT("and makes the same event key"), SecondKeys.Num() == 1 && FirstKeys.Num() == 1 && SecondKeys[0] == FirstKeys[0]);

	// A consumer that deduplicates by that key creates the break once
	FGSTN_PartEventDedupe Consumer;
	int32 Created = 0;
	for (const FGSTN_PartEventKey& Key : FirstKeys)
	{
		Created += Consumer.TryRegister(Key) ? 1 : 0;
	}
	for (const FGSTN_PartEventKey& Key : SecondKeys)
	{
		Created += Consumer.TryRegister(Key) ? 1 : 0;
	}
	TestEqual(TEXT("the presentation of the break is created once"), Created, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsApplierEventsKeyTest, "GSTNParts.NP.Applier.EventsOncePerKey", GSTNPartsNPTest::TestFlags)
bool FGSTN_PartsApplierEventsKeyTest::RunTest(const FString& Parameters)
{
	AActor* Target = MakeOwner();
	AActor* OtherTarget = MakeOwner();

	// The key is (entity, part, serial): the same break of two entities, or two breaks of one part, are different keys
	FGSTN_PartHitOutcome Outcome;
	Outcome.PartIndex = Head;
	Outcome.NewlyBrokenMask = static_cast<uint16>(1u << Head);
	Outcome.ResultBrokenMask = Outcome.NewlyBrokenMask;
	Outcome.ResultBreakSerial = 1;
	Outcome.Flags = GSTNPartOutcomeFlags::PartDamaged;

	TArray<FGSTN_SimEvent> Events;
	FGSTN_PartEvents::Make(Outcome, MakeEnvelope(Target), Events);
	FGSTN_PartEventKey Key;
	if (!FindBrokenKey(Events, Key))
	{
		AddError(TEXT("a Broken event"));
		return false;
	}

	FGSTN_PartEventDedupe Window;
	TestTrue(TEXT("the first sight of a key acts"), Window.TryRegister(Key));
	TestFalse(TEXT("a repeat does not"), Window.TryRegister(Key));

	FGSTN_PartHitOutcome Later = Outcome;
	Later.ResultBreakSerial = 2;
	TArray<FGSTN_SimEvent> LaterEvents;
	FGSTN_PartEvents::Make(Later, MakeEnvelope(Target), LaterEvents);
	FGSTN_PartEventKey LaterKey;
	TestTrue(TEXT("a Broken event of the later break"), FindBrokenKey(LaterEvents, LaterKey));
	TestTrue(TEXT("a restored and broken-again part has a new serial and acts again"), Window.TryRegister(LaterKey));

	TArray<FGSTN_SimEvent> OtherEvents;
	FGSTN_PartEvents::Make(Outcome, MakeEnvelope(OtherTarget), OtherEvents);
	FGSTN_PartEventKey OtherKey;
	TestTrue(TEXT("a Broken event of the other entity"), FindBrokenKey(OtherEvents, OtherKey));
	TestTrue(TEXT("another entity's break is its own"), Window.TryRegister(OtherKey));

	// The applier appends the same events the helper makes
	FGSTN_PartsOutcomeApplier Applier;
	FGSTN_SimEventOutbox Outbox;
	Applier.AppendEvents(FInstancedStruct::Make(Outcome), MakeEnvelope(Target), Outbox);
	TestEqual(TEXT("the outbox holds the events of the hit"), Outbox.Events.Num(), Events.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsApplierMonotonicTest, "GSTNParts.NP.Applier.MaskMonotonicWithinHit", GSTNPartsNPTest::TestFlags)
bool FGSTN_PartsApplierMonotonicTest::RunTest(const FString& Parameters)
{
	FPartsFixture Fixture;
	FGSTN_SyncFragmentArray Fragments;
	SetFragment(Fragments, MakeState(1u << Torso, 3), Fixture.Num());

	// An outcome built from an older view that did not know the torso was broken
	FGSTN_PartHitOutcome Outcome;
	Outcome.PartIndex = Head;
	Outcome.ResultBrokenMask = static_cast<uint16>(1u << Head);
	Outcome.NewlyBrokenMask = Outcome.ResultBrokenMask;
	Outcome.ResultBreakSerial = 4;

	FGSTN_PartsOutcomeApplier Applier;
	FGSTN_OutcomeApplyContext Context;
	Context.VictimFragments = &Fragments;
	FGSTN_HitContextView View;
	Applier.Apply(FInstancedStruct::Make(Outcome), View, Context);

	const FGSTN_PartsState After = ReadState(Fragments);
	TestEqual(TEXT("a break is never undone by a hit: both bits"), After.BrokenMask, static_cast<uint16>((1u << Torso) | (1u << Head)));
	TestEqual(TEXT("the part count is kept"), FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Fragments)->PartCount, static_cast<uint8>(Fixture.Num()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
