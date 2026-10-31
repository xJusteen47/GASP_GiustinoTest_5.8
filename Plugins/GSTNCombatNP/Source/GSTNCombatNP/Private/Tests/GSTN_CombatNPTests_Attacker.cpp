// Automation tests of the attacker side of the combat tick (combat-m4-hits 6, M4 H8). They need no world: the snapshot is built by hand and the
// step is the pure function FGSTN_CombatAttacker::Tick over a sync core.

#include "Misc/AutomationTest.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Rules/GSTN_CombatRules.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "StructUtils/InstancedStruct.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** The attacker is entity 1 on team 1, standing at the origin facing +X with a run that started on frame 0 */
	struct FAttackerFixture
	{
		static constexpr int32 AttackerId = 1;

		FGSTN_CombatTimelineData Timeline;
		FGSTN_StepSnapshot Snapshot;
		FGSTN_CombatSyncCore Core;
		FGSTN_TimelineFrameResult Scratch;
		TArray<FGSTN_AttackerHit> Hits;
		TStrongObjectPtr<UGSTN_CombatRules> Rules;
		TArray<TStrongObjectPtr<UGSTN_HurtboxSetDefinition>> Sets;
		FTransform Root = FTransform::Identity;
		FGSTN_HitGroupMask Mask;
		FGenericTeamId AttackerTeam = FGenericTeamId(1);

		FAttackerFixture()
		{
			Rules.Reset(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
			Timeline.TotalFrames = 30;
			Timeline.FirstActionableFrame = 20;
			Timeline.OverrideFrames = 0;
			Core.ActionState = EGSTN_ActionState::InTimeline;
			Core.ActiveTimeline = 0;
			Core.TimelineStartFrame = 0;
			Core.Hit.ResetRun();
		}

		/** A sphere hitbox track over [Start, End) with one hitbox per entry of Specs */
		void AddTrack(int32 Start, int32 End, const TArray<FGSTN_HitboxSpec>& Specs)
		{
			FGSTN_TimelineTrack_Hitbox Track;
			Track.StartFrame = Start;
			Track.EndFrame = End;
			Track.Hitboxes = Specs;
			Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
		}

		static FGSTN_HitboxSpec MakeSphere(double Radius, uint8 Group, const FVector& Offset = FVector::ZeroVector)
		{
			FGSTN_HitboxSpec Spec;
			Spec.Shape.Type = EGSTN_ShapeType::Sphere;
			Spec.Shape.Extent = FVector(Radius, Radius, Radius);
			Spec.Shape.Local = FTransform(Offset);
			Spec.HitGroup = Group;
			Spec.Data.Damage = 10.f;
			Spec.Data.BaseKnockback = 600.f;
			Spec.Data.KnockbackGrowth = 20.f;
			return Spec;
		}

		/** An entity with Count sphere hurtboxes of the radius, stacked along Z 100 cm apart; returns its index in the snapshot entries (before the sort) */
		FGSTN_SnapshotEntry& AddEntity(int32 Id, uint8 Team, const FVector& Location, double HurtRadius = 40.0, int32 Count = 1)
		{
			UGSTN_HurtboxSetDefinition* Set = NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage());
			Sets.Emplace(Set);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FGSTN_Hurtbox Hurtbox;
				Hurtbox.Shape.Type = EGSTN_ShapeType::Sphere;
				Hurtbox.Shape.Extent = FVector(HurtRadius);
				Hurtbox.Shape.Local = FTransform(FVector(0.0, 0.0, 100.0 * Index));
				Set->Hurtboxes.Add(Hurtbox);
			}
			FGSTN_SnapshotEntry& Entry = Snapshot.Entries.AddDefaulted_GetRef();
			Entry.EntityId = Id;
			Entry.Team = FGenericTeamId(Team);
			Entry.Transform = FTransform(Location);
			Entry.Hurtboxes = Set;
			Entry.HurtStates.SetNum(Count);
			Entry.BoundRadius = static_cast<float>(UGSTN_CombatWorldSubsystem::ComputeBoundRadius(Set));
			return Entry;
		}

		void Finish()
		{
			Snapshot.Entries.Sort([](const FGSTN_SnapshotEntry& A, const FGSTN_SnapshotEntry& B) { return A.EntityId < B.EntityId; });
			Snapshot.BuildGrid(UGSTN_CombatWorldSubsystem::DefaultCellSize);
		}

		/** Runs the step on Frame; the timeline frame is the frame minus the start minus the paused frames, like the component computes it */
		void Run(int32 Frame)
		{
			FGSTN_AttackerTickInput In;
			In.Frame = Frame;
			In.AttackerId = AttackerId;
			In.AttackerTeam = AttackerTeam;
			In.Timeline = &Timeline;
			In.TimelineFrame = Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames;
			In.Snapshot = &Snapshot;
			In.Rules = Rules.Get();
			In.Root = Root;
			In.GroupMask = Mask;
			FGSTN_CombatAttacker::Tick(In, Core, Scratch, Hits);
		}

		bool HitVictim(int32 Id) const
		{
			return Hits.ContainsByPredicate([Id](const FGSTN_AttackerHit& Hit) { return Hit.VictimId == Id; });
		}
	};

	struct FAlwaysRicochet : IGSTN_AttackerOutcomeExtension
	{
		virtual FName GetName() const override { return TEXT("AttackerTestAlwaysRicochet"); }
		virtual EGSTN_AttackerHitOutcome Evaluate(const FGSTN_HitContextView&) const override { return EGSTN_AttackerHitOutcome::Ricochet; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttackerOncePerGroupTest, "GSTNCombat.NP.Hits.Attacker.OncePerGroupPerExecution", GSTN_NP_TEST_FLAGS)
bool FGSTN_AttackerOncePerGroupTest::RunTest(const FString& Parameters)
{
	FAttackerFixture Fixture;
	Fixture.AddTrack(0, 10, { FAttackerFixture::MakeSphere(60.0, 0, FVector(50.0, 0.0, 0.0)), FAttackerFixture::MakeSphere(60.0, 1, FVector(50.0, 0.0, 0.0)) });
	Fixture.AddEntity(2, 2, FVector(100.0, 0.0, 0.0));
	Fixture.Finish();

	Fixture.Run(0);
	TestEqual(TEXT("the first frame: one hit per group"), Fixture.Hits.Num(), 2);
	TestTrue(TEXT("both groups are on the ring"), Fixture.Core.Hit.HasHitTarget(2, 0, 0) && Fixture.Core.Hit.HasHitTarget(2, 0, 1));
	TestEqual(TEXT("two targets"), Fixture.Core.Hit.NumHitTargets(), 2);
	TestEqual(TEXT("the confirm flags carry both groups"), static_cast<int32>(Fixture.Core.Hit.HitConfirmFlags), 3);

	for (int32 Frame = 1; Frame < 6; ++Frame)
	{
		Fixture.Run(Frame);
		TestEqual(*FString::Printf(TEXT("frame %d: the victim is not hit again"), Frame), Fixture.Hits.Num(), 0);
	}

	// A new execution of the move (a new start frame) forgets the targets
	Fixture.Core.TimelineStartFrame = 4;
	Fixture.Core.Hit.ResetRun();
	Fixture.Run(5);
	TestEqual(TEXT("a new execution hits again"), Fixture.Hits.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttackerRingOverflowTest, "GSTNCombat.NP.Hits.Attacker.RingOverflowReplacesOldest", GSTN_NP_TEST_FLAGS)
bool FGSTN_AttackerRingOverflowTest::RunTest(const FString& Parameters)
{
	FAttackerFixture Fixture;
	// Group 0 reaches the five victims on the left, group 1 the four on the right: nine in all, the ring holds eight
	FGSTN_HitboxSpec Left = FAttackerFixture::MakeSphere(120.0, 0, FVector(0.0, -300.0, 0.0));
	FGSTN_HitboxSpec Right = FAttackerFixture::MakeSphere(120.0, 1, FVector(0.0, 300.0, 0.0));
	Fixture.AddTrack(0, 10, { Left, Right });
	for (int32 Index = 0; Index < 5; ++Index)
	{
		Fixture.AddEntity(10 + Index, 2, FVector(Index * 30.0, -300.0, 0.0), 20.0);
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Fixture.AddEntity(20 + Index, 2, FVector(Index * 30.0, 300.0, 0.0), 20.0);
	}
	Fixture.Finish();

	Fixture.Run(0);
	TestEqual(TEXT("nine hits"), Fixture.Hits.Num(), 9);
	TestEqual(TEXT("the ring holds eight"), Fixture.Core.Hit.NumHitTargets(), FGSTN_HitSyncState::MaxHitTargets);
	int32 Replaced = 0;
	for (const FGSTN_AttackerHit& Hit : Fixture.Hits)
	{
		Replaced += Hit.bReplacedTarget ? 1 : 0;
	}
	TestEqual(TEXT("one hit replaced a target"), Replaced, 1);
	// Entity order: 10..14 then 20..23, so the first written (10, group 0) is the oldest and goes
	TestFalse(TEXT("the oldest target is gone"), Fixture.Core.Hit.HasHitTarget(10, 0, 0));
	TestTrue(TEXT("the newest is on the ring"), Fixture.Core.Hit.HasHitTarget(23, 0, 1));
	TestTrue(TEXT("the second oldest stays"), Fixture.Core.Hit.HasHitTarget(11, 0, 0));

	// MaxTargetsPerGroup: a group of a hitbox that allows two takes the two lowest ids and no more
	FAttackerFixture Limited;
	FGSTN_HitboxSpec Wide = FAttackerFixture::MakeSphere(400.0, 0);
	Wide.MaxTargetsPerGroup = 2;
	Limited.AddTrack(0, 10, { Wide });
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Limited.AddEntity(30 + Index, 2, FVector(100.0 + Index * 10.0, 0.0, 0.0));
	}
	Limited.Finish();
	Limited.Run(0);
	TestEqual(TEXT("the limit: two hits"), Limited.Hits.Num(), 2);
	TestTrue(TEXT("the lowest ids win"), Limited.HitVictim(30) && Limited.HitVictim(31));
	Limited.Run(1);
	TestEqual(TEXT("and the others are never reached later in the same run"), Limited.Hits.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttackerMaskTest, "GSTNCombat.NP.Hits.Attacker.MaskSkipsGroup", GSTN_NP_TEST_FLAGS)
bool FGSTN_AttackerMaskTest::RunTest(const FString& Parameters)
{
	FAttackerFixture Fixture;
	FGSTN_HitboxSpec A = FAttackerFixture::MakeSphere(60.0, 0, FVector(50.0, 0.0, 0.0));
	A.Data.HitstopMode = EGSTN_HitstopMode::Override;
	A.Data.HitstopFramesOverride = 6;
	A.Data.AttackerHitstopFramesOverride = 6;
	Fixture.AddTrack(0, 10, { A });
	Fixture.AddEntity(2, 2, FVector(100.0, 0.0, 0.0));
	Fixture.Finish();

	Fixture.Mask.Set(0);
	Fixture.Run(0);
	TestEqual(TEXT("a masked group makes no hit"), Fixture.Hits.Num(), 0);
	TestEqual(TEXT("no confirm"), static_cast<int32>(Fixture.Core.Hit.HitConfirmFlags), 0);
	TestEqual(TEXT("no target"), Fixture.Core.Hit.NumHitTargets(), 0);
	TestFalse(TEXT("no hitstop"), Fixture.Core.IsInHitstop(1));
	TestTrue(TEXT("no freeze request"), Fixture.Core.Requests.IsEmpty());

	// The mask is the frame's: next frame it is empty again and the hit lands
	Fixture.Mask = FGSTN_HitGroupMask();
	Fixture.Run(1);
	TestEqual(TEXT("unmasked the group hits"), Fixture.Hits.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttackerIntangibleTest, "GSTNCombat.NP.Hits.Attacker.IntangibleNotHit", GSTN_NP_TEST_FLAGS)
bool FGSTN_AttackerIntangibleTest::RunTest(const FString& Parameters)
{
	FAttackerFixture Fixture;
	Fixture.AddTrack(0, 10, { FAttackerFixture::MakeSphere(60.0, 0, FVector(50.0, 0.0, 0.0)) });
	FGSTN_SnapshotEntry& Victim = Fixture.AddEntity(2, 2, FVector(100.0, 0.0, 0.0));
	Victim.HurtStates[0].State = EGSTN_HurtState::Intangible;
	Fixture.Finish();

	Fixture.Run(0);
	TestEqual(TEXT("an intangible hurtbox is passed through"), Fixture.Hits.Num(), 0);
	TestEqual(TEXT("nothing registered"), Fixture.Core.Hit.NumHitTargets(), 0);
	TestEqual(TEXT("no confirm"), static_cast<int32>(Fixture.Core.Hit.HitConfirmFlags), 0);

	// With a second hurtbox the hit lands on the one that is still hittable
	FAttackerFixture Two;
	FGSTN_HitboxSpec Tall = FAttackerFixture::MakeSphere(200.0, 0, FVector(50.0, 0.0, 50.0));
	Two.AddTrack(0, 10, { Tall });
	FGSTN_SnapshotEntry& Pair = Two.AddEntity(2, 2, FVector(100.0, 0.0, 0.0), 40.0, 2);
	Pair.HurtStates[0].State = EGSTN_HurtState::Intangible;
	Two.Finish();
	Two.Run(0);
	TestEqual(TEXT("one hit"), Two.Hits.Num(), 1);
	if (Two.Hits.Num() == 1)
	{
		TestEqual(TEXT("on the hurtbox that is not intangible"), static_cast<int32>(Two.Hits[0].Entry.HurtboxIndex), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttackerInvincibleTest, "GSTNCombat.NP.Hits.Attacker.InvincibleRegistersNoDelivery", GSTN_NP_TEST_FLAGS)
bool FGSTN_AttackerInvincibleTest::RunTest(const FString& Parameters)
{
	FAttackerFixture Fixture;
	FGSTN_HitboxSpec Spec = FAttackerFixture::MakeSphere(60.0, 0, FVector(50.0, 0.0, 0.0));
	Spec.Data.HitstopMode = EGSTN_HitstopMode::Override;
	Spec.Data.HitstopFramesOverride = 8;
	Spec.Data.AttackerHitstopFramesOverride = 5;
	Fixture.AddTrack(0, 10, { Spec });
	FGSTN_SnapshotEntry& Victim = Fixture.AddEntity(2, 2, FVector(100.0, 0.0, 0.0));
	Victim.HurtStates[0].State = EGSTN_HurtState::Invincible;
	Fixture.Finish();

	Fixture.Run(3);
	TestEqual(TEXT("the hit registers"), Fixture.Hits.Num(), 1);
	TestTrue(TEXT("the target is on the ring"), Fixture.Core.Hit.HasHitTarget(2, 0, 0));
	TestEqual(TEXT("the run is confirmed"), static_cast<int32>(Fixture.Core.Hit.HitConfirmFlags), 1);
	TestEqual(TEXT("the attacker's hitstop opens"), Fixture.Core.HitstopEndFrame - Fixture.Core.HitstopStartFrame, 5);
	TestNotNull(TEXT("with its freeze"), Fixture.Core.Requests.Find(EGSTN_RequestType::Freeze, 3));
	if (Fixture.Hits.Num() == 1)
	{
		// The victim is told, flagged: it resolves no damage and no reaction from an invincible entry
		TestEqual(TEXT("the inbox entry says invincible"), static_cast<int32>(Fixture.Hits[0].Entry.HurtState), static_cast<int32>(EGSTN_HurtState::Invincible));
		TestFalse(TEXT("and no ricochet"), Fixture.Hits[0].Entry.bRicochet);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttackerRelationshipVariantTest, "GSTNCombat.NP.Hits.Attacker.RelationshipVariant", GSTN_NP_TEST_FLAGS)
bool FGSTN_AttackerRelationshipVariantTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Ally = GSTNCombatHitTags::Relationship_Ally.GetTag();
	const FGameplayTag Enemy = GSTNCombatHitTags::Relationship_Enemy.GetTag();
	const FGameplayTag Neutral = GSTNCombatHitTags::Relationship_Neutral.GetTag();

	auto MakeSpec = [&]()
	{
		FGSTN_HitboxSpec Spec = FAttackerFixture::MakeSphere(60.0, 0, FVector(50.0, 0.0, 0.0));
		FGSTN_HitVariant& EnemyVariant = Spec.RelationshipVariants.Add(Enemy);
		EnemyVariant.Override.Fields = static_cast<int32>(EGSTN_HitDataField::Damage);
		EnemyVariant.Override.Values.Damage = 77.f;
		FGSTN_HitVariant& AllyVariant = Spec.RelationshipVariants.Add(Ally);
		AllyVariant.Override.Fields = static_cast<int32>(EGSTN_HitDataField::Damage);
		AllyVariant.Override.Values.Damage = 3.f;
		Spec.RelationshipVariants.Add(Neutral).bIgnore = true;
		return Spec;
	};

	// Enemy: the variant's data reaches the inbox entry
	{
		FAttackerFixture Fixture;
		Fixture.AddTrack(0, 10, { MakeSpec() });
		Fixture.AddEntity(2, 2, FVector(100.0, 0.0, 0.0));
		Fixture.Finish();
		Fixture.Run(0);
		TestEqual(TEXT("an enemy is hit"), Fixture.Hits.Num(), 1);
		if (Fixture.Hits.Num() == 1)
		{
			TestEqual(TEXT("with the variant's damage"), Fixture.Hits[0].Entry.Hit.Damage, 77.f);
			TestEqual(TEXT("relationship index Enemy"), static_cast<int32>(Fixture.Hits[0].Entry.Relationship), 2);
		}
	}
	// Neutral (no team): the variant ignores it
	{
		FAttackerFixture Fixture;
		Fixture.AddTrack(0, 10, { MakeSpec() });
		Fixture.AddEntity(2, 255, FVector(100.0, 0.0, 0.0));
		Fixture.Finish();
		Fixture.Run(0);
		TestEqual(TEXT("bIgnore: a neutral is passed through"), Fixture.Hits.Num(), 0);
	}
	// Ally: off by the rules, whatever the variant says; with friendly fire on the variant applies
	{
		FAttackerFixture Fixture;
		Fixture.AddTrack(0, 10, { MakeSpec() });
		Fixture.AddEntity(2, 1, FVector(100.0, 0.0, 0.0));
		Fixture.Finish();
		Fixture.Run(0);
		TestEqual(TEXT("friendly fire off: an ally is not hit"), Fixture.Hits.Num(), 0);

		Fixture.Rules->bFriendlyFire = true;
		Fixture.Run(1);
		TestEqual(TEXT("friendly fire on: the ally is hit"), Fixture.Hits.Num(), 1);
		if (Fixture.Hits.Num() == 1)
		{
			TestEqual(TEXT("with the ally variant"), Fixture.Hits[0].Entry.Hit.Damage, 3.f);
			TestEqual(TEXT("relationship index Ally"), static_cast<int32>(Fixture.Hits[0].Entry.Relationship), 1);
		}
	}
	// ShapeScale: a variant that shrinks the shape turns a touch into a miss
	{
		FAttackerFixture Fixture;
		FGSTN_HitboxSpec Spec = FAttackerFixture::MakeSphere(60.0, 0, FVector(50.0, 0.0, 0.0));
		Spec.RelationshipVariants.Add(Enemy).ShapeScale = 0.1f;
		Fixture.AddTrack(0, 10, { Spec });
		Fixture.AddEntity(2, 2, FVector(145.0, 0.0, 0.0));
		Fixture.Finish();
		Fixture.Run(0);
		TestEqual(TEXT("a shrunk shape misses"), Fixture.Hits.Num(), 0);
		FAttackerFixture Plain;
		Plain.AddTrack(0, 10, { FAttackerFixture::MakeSphere(60.0, 0, FVector(50.0, 0.0, 0.0)) });
		Plain.AddEntity(2, 2, FVector(145.0, 0.0, 0.0));
		Plain.Finish();
		Plain.Run(0);
		TestEqual(TEXT("and the plain shape hits the same victim"), Plain.Hits.Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttackerSelfTest, "GSTNCombat.NP.Hits.Attacker.SelfExcludedUnlessVariant", GSTN_NP_TEST_FLAGS)
bool FGSTN_AttackerSelfTest::RunTest(const FString& Parameters)
{
	{
		FAttackerFixture Fixture;
		Fixture.AddTrack(0, 10, { FAttackerFixture::MakeSphere(100.0, 0) });
		Fixture.AddEntity(FAttackerFixture::AttackerId, 1, FVector::ZeroVector);
		Fixture.Finish();
		Fixture.Run(0);
		TestEqual(TEXT("the attacker's own hurtbox is not hit"), Fixture.Hits.Num(), 0);
	}
	{
		FAttackerFixture Fixture;
		FGSTN_HitboxSpec Spec = FAttackerFixture::MakeSphere(100.0, 0);
		FGSTN_HitVariant& SelfVariant = Spec.RelationshipVariants.Add(GSTNCombatHitTags::Relationship_Self.GetTag());
		SelfVariant.Override.Fields = static_cast<int32>(EGSTN_HitDataField::Damage);
		SelfVariant.Override.Values.Damage = 5.f;
		Fixture.AddTrack(0, 10, { Spec });
		Fixture.AddEntity(FAttackerFixture::AttackerId, 1, FVector::ZeroVector);
		Fixture.Finish();
		Fixture.Run(0);
		TestEqual(TEXT("with a Self variant the attacker hits itself"), Fixture.Hits.Num(), 1);
		if (Fixture.Hits.Num() == 1)
		{
			TestEqual(TEXT("with the variant's data"), Fixture.Hits[0].Entry.Hit.Damage, 5.f);
			TestEqual(TEXT("relationship index Self"), static_cast<int32>(Fixture.Hits[0].Entry.Relationship), 0);
		}

		// A Self variant that ignores keeps it out
		FAttackerFixture Ignoring;
		FGSTN_HitboxSpec IgnoreSpec = FAttackerFixture::MakeSphere(100.0, 0);
		IgnoreSpec.RelationshipVariants.Add(GSTNCombatHitTags::Relationship_Self.GetTag()).bIgnore = true;
		Ignoring.AddTrack(0, 10, { IgnoreSpec });
		Ignoring.AddEntity(FAttackerFixture::AttackerId, 1, FVector::ZeroVector);
		Ignoring.Finish();
		Ignoring.Run(0);
		TestEqual(TEXT("bIgnore on Self keeps it out"), Ignoring.Hits.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttackerWorldAtMoveStartTest, "GSTNCombat.NP.Hits.Attacker.WorldAtMoveStartUsesOrigin", GSTN_NP_TEST_FLAGS)
bool FGSTN_AttackerWorldAtMoveStartTest::RunTest(const FString& Parameters)
{
	auto Make = [](EGSTN_HitboxSpace Space)
	{
		FAttackerFixture Fixture;
		FGSTN_HitboxSpec Spec = FAttackerFixture::MakeSphere(50.0, 0);
		Spec.Space = Space;
		Fixture.AddTrack(0, 10, { Spec });
		Fixture.AddEntity(2, 2, FVector(500.0, 0.0, 0.0));
		Fixture.Finish();
		return Fixture;
	};

	// The attacker has walked away from where the move started; the hitbox stays where it was thrown
	{
		FAttackerFixture Fixture = Make(EGSTN_HitboxSpace::WorldAtMoveStart);
		Fixture.Core.Hit.MoveOrigin = FGSTN_MoveOrigin::Make(FTransform(FVector(500.0, 0.0, 0.0)));
		Fixture.Root = FTransform(FVector(-800.0, 0.0, 0.0));
		Fixture.Run(0);
		TestEqual(TEXT("the origin of the move places the hitbox, not the attacker"), Fixture.Hits.Num(), 1);
	}
	{
		FAttackerFixture Fixture = Make(EGSTN_HitboxSpace::WorldAtMoveStart);
		Fixture.Core.Hit.MoveOrigin = FGSTN_MoveOrigin::Make(FTransform(FVector(-300.0, 0.0, 0.0)));
		Fixture.Root = FTransform(FVector(500.0, 0.0, 0.0));
		Fixture.Run(0);
		TestEqual(TEXT("an attacker standing on the victim does not hit through the origin"), Fixture.Hits.Num(), 0);
	}
	{
		// The origin is quantised (1/10 cm), the hitbox is placed on the quantised value
		const FGSTN_MoveOrigin Origin = FGSTN_MoveOrigin::Make(FTransform(FVector(500.04, 0.0, 0.0)));
		TestEqual(TEXT("quantised to a tenth of a centimetre"), Origin.PositionQ[0], 5000);
	}
	// Root space follows the attacker
	{
		FAttackerFixture Fixture = Make(EGSTN_HitboxSpace::Root);
		Fixture.Root = FTransform(FVector(-800.0, 0.0, 0.0));
		Fixture.Run(0);
		TestEqual(TEXT("a root hitbox moves with the attacker"), Fixture.Hits.Num(), 0);
		Fixture.Root = FTransform(FVector(480.0, 0.0, 0.0));
		Fixture.Run(1);
		TestEqual(TEXT("and hits where the attacker is"), Fixture.Hits.Num(), 1);
	}
	// A baked socket is placed relative to the root at the timeline frame
	{
		FAttackerFixture Fixture;
		FGSTN_HitboxSpec Spec = FAttackerFixture::MakeSphere(50.0, 0);
		Spec.Space = EGSTN_HitboxSpace::BakedSocket;
		Spec.Socket = TEXT("hand");
		Fixture.AddTrack(0, 10, { Spec });
		FGSTN_BakedSocketFrames Baked;
		Baked.Frames.Add(FTransform(FVector(0.0, 0.0, 0.0)));
		Baked.Frames.Add(FTransform(FVector(500.0, 0.0, 0.0)));
		Fixture.Timeline.BakedSockets.Add(TEXT("hand"), Baked);
		Fixture.AddEntity(2, 2, FVector(500.0, 0.0, 0.0));
		Fixture.Finish();
		Fixture.Run(0);
		TestEqual(TEXT("baked frame 0 is at the root: a miss"), Fixture.Hits.Num(), 0);
		Fixture.Run(1);
		TestEqual(TEXT("baked frame 1 is 500 cm out: a hit"), Fixture.Hits.Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttackerHitstopTest, "GSTNCombat.NP.Hits.Attacker.AttackerHitstopFromSnapshotView", GSTN_NP_TEST_FLAGS)
bool FGSTN_AttackerHitstopTest::RunTest(const FString& Parameters)
{
	auto RunWith = [](double DamagePercent, EGSTN_HitstopMode Mode, int32 AttackerOverride)
	{
		FAttackerFixture Fixture;
		FGSTN_HitboxSpec Spec = FAttackerFixture::MakeSphere(60.0, 0, FVector(50.0, 0.0, 0.0));
		Spec.Data.HitstopMode = Mode;
		Spec.Data.AttackerHitstopFramesOverride = AttackerOverride;
		Spec.Data.HitstopFramesOverride = 9;
		Fixture.AddTrack(0, 10, { Spec });
		FGSTN_SnapshotEntry& Victim = Fixture.AddEntity(2, 2, FVector(100.0, 0.0, 0.0));
		Victim.Reaction = FGSTN_VictimReactionState::Make(DamagePercent, 1.0, 0, FGSTN_StaggerThreshold());
		Fixture.Finish();
		Fixture.Run(7);
		return Fixture;
	};

	// An authored window is taken as it is, from the frame of the hit
	{
		FAttackerFixture Fixture = RunWith(0.0, EGSTN_HitstopMode::Override, 6);
		TestEqual(TEXT("one hit"), Fixture.Hits.Num(), 1);
		TestEqual(TEXT("the window starts on the frame of the hit"), Fixture.Core.HitstopStartFrame, 7);
		TestEqual(TEXT("and lasts the authored frames"), Fixture.Core.HitstopEndFrame, 13);
		const FGSTN_CombatRequest* Freeze = Fixture.Core.Requests.Find(EGSTN_RequestType::Freeze, 7);
		TestTrue(TEXT("the freeze request has the window's length"), Freeze && Freeze->Arg == 6);
		TestFalse(TEXT("the frame of the hit runs on"), Fixture.Core.IsInHitstop(6));
		TestTrue(TEXT("the next one stands still"), Fixture.Core.IsInHitstop(8));
	}

	// Computed from the model: the same hit against a battered victim, as the snapshot says it is, never gives the attacker less
	FAttackerFixture Fresh = RunWith(0.0, EGSTN_HitstopMode::Auto, -1);
	FAttackerFixture Battered = RunWith(900.0, EGSTN_HitstopMode::Auto, -1);
	FAttackerFixture Again = RunWith(0.0, EGSTN_HitstopMode::Auto, -1);
	TestEqual(TEXT("the model gives the attacker a hitstop"), Fresh.Hits.Num() == 1 && Fresh.Hits[0].AttackerHitstopFrames > 0, true);
	if (Fresh.Hits.Num() == 1 && Battered.Hits.Num() == 1 && Again.Hits.Num() == 1)
	{
		TestTrue(TEXT("a battered victim does not shorten it"), Battered.Hits[0].AttackerHitstopFrames >= Fresh.Hits[0].AttackerHitstopFrames);
		TestEqual(TEXT("the same snapshot gives the same window"), Again.Hits[0].AttackerHitstopFrames, Fresh.Hits[0].AttackerHitstopFrames);
		TestEqual(TEXT("window end"), Fresh.Core.HitstopEndFrame - Fresh.Core.HitstopStartFrame, Fresh.Hits[0].AttackerHitstopFrames);
	}

	// Two hits in one frame open one window, the longer one
	{
		FAttackerFixture Fixture;
		FGSTN_HitboxSpec Short = FAttackerFixture::MakeSphere(300.0, 0);
		Short.Data.HitstopMode = EGSTN_HitstopMode::Override;
		Short.Data.AttackerHitstopFramesOverride = 3;
		FGSTN_HitboxSpec Long = FAttackerFixture::MakeSphere(300.0, 1);
		Long.Data.HitstopMode = EGSTN_HitstopMode::Override;
		Long.Data.AttackerHitstopFramesOverride = 9;
		Fixture.AddTrack(0, 10, { Short, Long });
		Fixture.AddEntity(2, 2, FVector(100.0, 0.0, 0.0));
		Fixture.Finish();
		Fixture.Run(0);
		TestEqual(TEXT("two groups, two hits"), Fixture.Hits.Num(), 2);
		TestEqual(TEXT("one window, the longer"), Fixture.Core.HitstopEndFrame - Fixture.Core.HitstopStartFrame, 9);
	}

	// A ricochet: the victim is told, the attacker gets no confirm, no hitstop, no meter
	{
		FAlwaysRicochet Ricochet;
		FGSTN_AttackerOutcomeRegistry::Get().Register(&Ricochet);
		FAttackerFixture Fixture = RunWith(0.0, EGSTN_HitstopMode::Override, 6);
		FGSTN_AttackerOutcomeRegistry::Get().Unregister(&Ricochet);
		TestEqual(TEXT("the ricochet hit is delivered"), Fixture.Hits.Num(), 1);
		if (Fixture.Hits.Num() == 1)
		{
			TestTrue(TEXT("flagged ricochet"), Fixture.Hits[0].bRicochet && Fixture.Hits[0].Entry.bRicochet);
		}
		TestEqual(TEXT("no confirm"), static_cast<int32>(Fixture.Core.Hit.HitConfirmFlags), 0);
		TestFalse(TEXT("no hitstop"), Fixture.Core.IsInHitstop(8));
		TestTrue(TEXT("the target is still registered: one ricochet per group and run"), Fixture.Core.Hit.HasHitTarget(2, 0, 0));
	}
	return true;
}

#endif
