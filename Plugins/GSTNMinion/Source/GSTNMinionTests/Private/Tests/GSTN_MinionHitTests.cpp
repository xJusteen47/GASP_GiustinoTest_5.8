// Automation tests of the minion hit flows (M8 G5): the victim view through the pipeline steps, the health fraction model, the reaction
// tiers with and without the stub part step, the inbox order and generation drop, the minion's attacker hitstop, the attack tokens and
// the lag compensation history. They drive TickBatch directly in a transient game world; rules, hurtboxes, timelines and a stub fighter
// are built in code (no content).

#include "Components/SkeletalMeshComponent.h"
#include "Data/GSTN_MinionDefinition.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Presentation/GSTN_HitPresentationComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Sync/GSTN_SyncFragment.h"
#include "Engine/World.h"
#include "GenericTeamAgentInterface.h"
#include "GSTN_MinionTags.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hits/GSTN_KnockbackModel.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_CombatRules.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Simulation/GSTN_LagCompensation.h"
#include "Spawn/GSTN_MinionSpawner.h"
#include "GSTN_MinionTestSteps.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "World/GSTN_CombatWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_MINION_HIT_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A transient game world with an activated minion subsystem, every slot in Combat and no gravity (minions stay where they spawn) */
	struct FHitRig
	{
		FHitRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNMinionHitTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());

			Minions = World->GetSubsystem<UGSTN_MinionCombatSubsystem>();
			Combat = UGSTN_CombatWorldSubsystem::Get(World);
			if (Minions)
			{
				Minions->ActivateForTests();
				Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Combat));
				Minions->SetGravitySource([](const FVector&) { return FVector::ZeroVector; });
			}
		}

		~FHitRig()
		{
			if (Minions)
			{
				Minions->SetForcedLODTier(INDEX_NONE);
				Minions->SetGravitySource(nullptr);
				Minions->SetExtraVictimStep(nullptr);
				Minions->OnOutcomeApplied.Clear();
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		FGSTN_MinionHandle Spawn(UGSTN_MinionDefinition* Definition, const FVector& Location = FVector::ZeroVector)
		{
			FGSTN_MinionSpawnParams Params;
			Params.bPlaySpawnTimeline = false;
			return Minions->EnqueueSpawn(Definition, FTransform(Location), Params);
		}

		UWorld* World = nullptr;
		UGSTN_MinionCombatSubsystem* Minions = nullptr;
		UGSTN_CombatWorldSubsystem* Combat = nullptr;
	};

	/** Standard rules with tiers the tests can aim at: push back from 300, launch from 1000, no tumble, every hit staggers, flat weight */
	TStrongObjectPtr<UGSTN_CombatRules> MakeRules(EGSTN_DamageMode Mode = EGSTN_DamageMode::Percent)
	{
		TStrongObjectPtr<UGSTN_CombatRules> Rules(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
		Rules->DamageMode = Mode;
		Rules->MaxHealth = 100.f;
		Rules->StaggerThreshold = 0.f;
		Rules->MinStaggerLockFrames = 10;
		Rules->DefaultWeightCurve.Samples.Reset();
		Rules->MeterGainPerDamage = 1.f;
		if (UGSTN_KnockbackModel* Model = Rules->KnockbackModel.Get())
		{
			Model->Tuning.PushBackThreshold = 300.f;
			Model->Tuning.LaunchThreshold = 1000.f;
			Model->Tuning.TumbleThreshold = 100000.f;
			Model->Tuning.MaxHitstop = 20;
			Model->Tuning.WeightReference = 100.f;
		}
		return Rules;
	}

	/** One body capsule (radius 40, half height 90) standing on the root */
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> MakeHurtboxes(EGSTN_HitReaction MaxReaction = EGSTN_HitReaction::Launch)
	{
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
		FGSTN_Hurtbox& Body = Set->Hurtboxes.AddDefaulted_GetRef();
		Body.Name = TEXT("Body");
		Body.Shape.Type = EGSTN_ShapeType::Capsule;
		Body.Shape.Extent = FVector(40.0, 40.0, 90.0);
		Body.Shape.Local = FTransform(FVector(0.0, 0.0, 90.0));
		Body.ReactionScale.MaxReaction = MaxReaction;
		return Set;
	}

	TStrongObjectPtr<UGSTN_MinionDefinition> MakeDefinition(UGSTN_HurtboxSetDefinition* Set, float Weight = 100.f)
	{
		TStrongObjectPtr<UGSTN_MinionDefinition> Definition(NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
		Definition->Stats.MaxHealth = 40.f;
		Definition->Stats.Weight = Weight;
		Definition->TeamId = 1;
		Definition->HurtboxSet = Set;
		return Definition;
	}

	/** A hit with a fixed knockback (so the tier is the test's choice) and an overridden hitstop */
	FGSTN_HitData MakeHitData(float Damage, float FixedKnockback, int32 HitstopFrames)
	{
		FGSTN_HitData Hit;
		Hit.Damage = Damage;
		Hit.FixedKnockback = FixedKnockback;
		Hit.HitstopMode = EGSTN_HitstopMode::Override;
		Hit.HitstopFramesOverride = HitstopFrames;
		return Hit;
	}

	struct FTestHit
	{
		int32 Frame = 1;
		int32 AttackerId = 7;
		uint8 AttackerTeam = 255;
		uint8 HurtState = 0;
		uint8 HitboxIndex = 0;
		FGSTN_HitData Data;
	};

	bool PostTestHit(FHitRig& Rig, const FGSTN_MinionHandle& Handle, const FTestHit& Test)
	{
		FGSTN_HitInboxEntry Hit;
		Hit.Frame = Test.Frame;
		Hit.AttackerId = Test.AttackerId;
		Hit.AttackerTeam = Test.AttackerTeam;
		// No index: the TeamFilter step resolves the relationship from the teams, as for a hit whose resolver made up a tag
		Hit.Relationship = 255;
		Hit.Hit = Test.Data;
		Hit.HitLocation = FVector(Rig.Minions->GetStore().Position[Handle.Index]) + FVector(-60.0, 0.0, 90.0);
		Hit.HurtboxIndex = 0;
		Hit.HurtState = Test.HurtState;
		Hit.AttackerHitboxIndex = Test.HitboxIndex;
		return Rig.Minions->PostToEntityInbox(GSTNEntityKinds::MakeMinionEntityId(Handle.Index), Handle.Generation, Hit.ToInboxEntry());
	}

	/** What OnOutcomeApplied saw, in call order */
	struct FCapturedOutcome
	{
		int32 Slot = INDEX_NONE;
		int32 AttackerId = INDEX_NONE;
		FGSTN_HitOutcome Outcome;
	};

	FDelegateHandle CaptureOutcomes(FHitRig& Rig, TArray<FCapturedOutcome>& Out)
	{
		return Rig.Minions->OnOutcomeApplied.AddLambda([&Out](const FGSTN_MinionHandle& Minion, const FGSTN_HitInboxEntry& Hit, const FGSTN_HitContextView& Resolved, int32)
		{
			FCapturedOutcome& Captured = Out.AddDefaulted_GetRef();
			Captured.Slot = Minion.Index;
			Captured.AttackerId = Hit.AttackerId;
			Captured.Outcome = Resolved.Outcome;
		});
	}

	EGSTN_MinionActionState StateOf(const FHitRig& Rig, const FGSTN_MinionHandle& Handle)
	{
		return static_cast<EGSTN_MinionActionState>(Rig.Minions->GetStore().ActionState[Handle.Index]);
	}

	/** A fighter for the minions to hit: a registered snapshot source that keeps what is posted to it */
	struct FStubFighter : public IGSTN_SnapshotSource
	{
		virtual bool ReadSnapshotEntry(int32 Frame, FGSTN_SnapshotEntry& Out) override
		{
			Out.Transform = Transform;
			Out.Facing = Transform.GetUnitAxis(EAxis::X);
			Out.Team = Team;
			Out.HurtStates.SetNum(Out.Hurtboxes ? Out.Hurtboxes->Hurtboxes.Num() : 0);
			return true;
		}

		virtual bool PostToInbox(const FGSTN_InboxEntry& Entry) override
		{
			Inbox.Add(Entry);
			return true;
		}

		FTransform Transform;
		FGenericTeamId Team = FGenericTeamId(2);
		TArray<FGSTN_InboxEntry> Inbox;
	};

	constexpr int32 FighterId = 1;

	/** A move: TotalFrames long, with (optionally) one Root-space sphere hitbox 100 cm ahead at chest height over [0, 10) */
	TStrongObjectPtr<UGSTN_CombatTimeline> MakeMoveTimeline(int32 TotalFrames, bool bWithHitbox, int32 AttackerHitstopFrames)
	{
		TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transient));
		Timeline->Data.TotalFrames = TotalFrames;
		Timeline->Data.FirstActionableFrame = TotalFrames;
		if (bWithHitbox)
		{
			TInstancedStruct<FGSTN_TimelineTrack>& Track = Timeline->Data.Tracks.AddDefaulted_GetRef();
			Track.InitializeAs<FGSTN_TimelineTrack_Hitbox>();
			FGSTN_TimelineTrack_Hitbox& Hitbox = Track.GetMutable<FGSTN_TimelineTrack_Hitbox>();
			Hitbox.StartFrame = 0;
			Hitbox.EndFrame = 10;
			FGSTN_HitboxSpec& Spec = Hitbox.Hitboxes.AddDefaulted_GetRef();
			Spec.Shape.Type = EGSTN_ShapeType::Sphere;
			Spec.Shape.Extent = FVector(60.0);
			Spec.Shape.Local = FTransform(FVector(100.0, 0.0, 90.0));
			Spec.Space = EGSTN_HitboxSpace::Root;
			Spec.Data = MakeHitData(5.f, 100.f, AttackerHitstopFrames);
		}
		return Timeline;
	}

	FGameplayTag HitTestCommand()
	{
		const FGameplayTag Swing = FGameplayTag::RequestGameplayTag(TEXT("Command.Minion.Attack.Swing"), false);
		return Swing.IsValid() ? Swing : FGameplayTag(GSTNMinionTags::Type);
	}

	/** Asks a minion to swing at the stub fighter */
	void IntendAttack(FHitRig& Rig, const FGSTN_MinionHandle& Handle)
	{
		FGSTN_MinionIntent Intent;
		Intent.Command = HitTestCommand();
		Intent.TargetEntityId = FighterId;
		Rig.Minions->SetIntent(Handle, Intent);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionHitsVictimViewTest, "GSTNMinion.Runtime.Hits.VictimView", GSTN_MINION_HIT_TEST_FLAGS)
bool FGSTN_MinionHitsVictimViewTest::RunTest(const FString& Parameters)
{
	FHitRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists"), Rig.Minions))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules();
	Rig.Minions->SetCombatRules(Rules.Get());
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set = MakeHurtboxes();
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeDefinition(Set.Get(), 80.f);
	const FGSTN_MinionHandle Handle = Rig.Spawn(Definition.Get());
	Rig.Minions->TickBatch(1);

	FGSTN_HitVictimView View;
	if (!TestTrue(TEXT("a live minion has a victim view"), Rig.Minions->BuildVictimView(Handle, 2, View)))
	{
		return false;
	}
	TestEqual(TEXT("the view's id is the minion pipeline id"), View.EntityId, GSTNEntityKinds::MakeMinionEntityId(Handle.Index));
	TestEqual(TEXT("the team is the type's"), static_cast<int32>(View.TeamId), 1);
	TestEqual(TEXT("the weight is the type's"), View.Weight, 80.f);
	TestEqual(TEXT("full health"), View.HealthFraction, 1.f);
	TestTrue(TEXT("the kind tag is on the view"), View.StateTags.HasTagExact(GSTNMinionTags::Entity_Minion));
	TestEqual(TEXT("no damage percent yet"), View.ReactionState.GetDamagePercent(), 0.f);
	TestFalse(TEXT("not in hitstop"), View.bInHitstop);

	TArray<FCapturedOutcome> Outcomes;
	CaptureOutcomes(Rig, Outcomes);

	// TeamFilter: an ally's hit is refused, nothing changes
	FTestHit Ally;
	Ally.AttackerTeam = 1;
	Ally.Data = MakeHitData(10.f, 100.f, 4);
	PostTestHit(Rig, Handle, Ally);
	Rig.Minions->TickBatch(2);
	TestEqual(TEXT("TeamFilter: the ally hit is drained"), Rig.Minions->GetLastBatchStats().HitsDrained, 1);
	TestEqual(TEXT("TeamFilter: and refused"), Rig.Minions->GetLastBatchStats().HitsApplied, 0);
	TestEqual(TEXT("TeamFilter: no damage"), Rig.Minions->GetHealth(Handle), 40.f);

	// HurtState: an Invincible hurtbox lands the hit with no damage and no flinch
	FTestHit Invincible;
	Invincible.Frame = 2;
	Invincible.HurtState = static_cast<uint8>(EGSTN_HurtState::Invincible);
	Invincible.Data = MakeHitData(10.f, 100.f, 4);
	PostTestHit(Rig, Handle, Invincible);
	Rig.Minions->TickBatch(3);
	TestEqual(TEXT("HurtState: the hit lands"), Rig.Minions->GetLastBatchStats().HitsApplied, 1);
	TestEqual(TEXT("HurtState: no damage"), Rig.Minions->GetHealth(Handle), 40.f);
	TestTrue(TEXT("HurtState: no stagger"), StateOf(Rig, Handle) != EGSTN_MinionActionState::Hitstop && StateOf(Rig, Handle) != EGSTN_MinionActionState::Hitstun);

	// ComputeKnockback, ApplyDamage, Hitstop, MeterGain: a normal hit
	Outcomes.Reset();
	FTestHit Normal;
	Normal.Frame = 3;
	Normal.HitboxIndex = 1;
	Normal.Data = MakeHitData(10.f, 100.f, 4);
	PostTestHit(Rig, Handle, Normal);
	Rig.Minions->TickBatch(4);
	if (TestEqual(TEXT("one outcome applied"), Outcomes.Num(), 1))
	{
		const FGSTN_HitOutcome& Outcome = Outcomes[0].Outcome;
		TestTrue(TEXT("ComputeKnockback ran"), Outcome.bHasKnockback);
		// Weight driven: the bracket times the reference weight over the minion's weight
		TestTrue(TEXT("ComputeKnockback read the minion's weight"), FMath::IsNearlyEqual(Outcome.Knockback.Knockback, 100.f * 100.f / 80.f, 0.5f));
		TestEqual(TEXT("ApplyDamage wrote the damage"), Outcome.Damage, 10.f);
		TestEqual(TEXT("Hitstop: the victim hitstop of the hit"), Outcome.Knockback.HitstopFrames, 4);
		TestTrue(TEXT("MeterGain is skipped for a minion victim"), Outcome.MeterGains.IsEmpty());
	}
	TestEqual(TEXT("ApplyDamage: health went down"), Rig.Minions->GetHealth(Handle), 30.f);
	TestEqual(TEXT("Hitstop: the window ends at the hit's frame + 4"), Rig.Minions->GetHitstopEndFrame(Handle.Index), 3 + 4);
	TestEqual(TEXT("the last attacker is recorded"), Rig.Minions->GetLastAttacker(Handle.Index), 7);
	TestEqual(TEXT("HitCounter counts every landed hit"), static_cast<int32>(Rig.Minions->GetStore().HitCounter[Handle.Index]), 2);

	// HurtboxReaction: a hurtbox capped at StaggerInPlace keeps a launcher in place
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Capped = MakeHurtboxes(EGSTN_HitReaction::StaggerInPlace);
	TStrongObjectPtr<UGSTN_MinionDefinition> Heavy = MakeDefinition(Capped.Get());
	const FGSTN_MinionHandle HeavyHandle = Rig.Spawn(Heavy.Get());
	Rig.Minions->TickBatch(5);
	Outcomes.Reset();
	FTestHit Launcher;
	Launcher.Frame = 5;
	Launcher.Data = MakeHitData(1.f, 3000.f, 4);
	PostTestHit(Rig, HeavyHandle, Launcher);
	Rig.Minions->TickBatch(6);
	if (TestEqual(TEXT("one outcome on the capped minion"), Outcomes.Num(), 1))
	{
		TestTrue(TEXT("HurtboxReaction capped the tier"), Outcomes[0].Outcome.Knockback.Reaction == EGSTN_HitReaction::StaggerInPlace);
	}
	TestTrue(TEXT("the capped minion waits in place"), Rig.Minions->GetPendingReaction(HeavyHandle.Index) == EGSTN_MinionPendingReaction::StaggerInPlace);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionHitsHealthFractionTest, "GSTNMinion.Runtime.Hits.HealthFractionModel", GSTN_MINION_HIT_TEST_FLAGS)
bool FGSTN_MinionHitsHealthFractionTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set = MakeHurtboxes();
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeDefinition(Set.Get());

	for (const EGSTN_DamageMode Mode : { EGSTN_DamageMode::Percent, EGSTN_DamageMode::Health })
	{
		FHitRig Rig;
		if (!TestNotNull(TEXT("the minion subsystem exists"), Rig.Minions))
		{
			return false;
		}
		const TCHAR* ModeName = Mode == EGSTN_DamageMode::Percent ? TEXT("Percent") : TEXT("Health");
		TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules(Mode);
		Rig.Minions->SetCombatRules(Rules.Get());
		const FGSTN_MinionHandle Handle = Rig.Spawn(Definition.Get());
		Rig.Minions->TickBatch(1);

		FTestHit Hit;
		Hit.Data = MakeHitData(10.f, 100.f, 0);
		PostTestHit(Rig, Handle, Hit);
		Rig.Minions->TickBatch(2);

		FGSTN_HitVictimView View;
		Rig.Minions->BuildVictimView(Handle, 3, View);
		TArray<FGSTN_SnapshotEntry> Entries;
		Rig.Minions->AppendSnapshotEntities(3, Entries);

		TestEqual(*FString::Printf(TEXT("%s: the store lost 10 hit points"), ModeName), Rig.Minions->GetHealth(Handle), 30.f);
		// The minion's own hit points decide its fraction in every mode (it dies at 0 whatever the rules count)
		TestEqual(*FString::Printf(TEXT("%s: the view's fraction is health over max"), ModeName), View.HealthFraction, 0.75f);
		if (TestEqual(*FString::Printf(TEXT("%s: one snapshot entry"), ModeName), Entries.Num(), 1))
		{
			TestEqual(*FString::Printf(TEXT("%s: the snapshot carries the same fraction"), ModeName), Entries[0].HealthFraction, 0.75f);
		}
		const float ExpectedPercent = Mode == EGSTN_DamageMode::Percent ? 10.f : 0.f;
		TestEqual(*FString::Printf(TEXT("%s: the knockback percent"), ModeName), Rig.Minions->GetKnockbackPercent(Handle.Index), ExpectedPercent);
		TestEqual(*FString::Printf(TEXT("%s: the view's damage percent"), ModeName), View.ReactionState.GetDamagePercent(), ExpectedPercent);

		// 0 health: Dying, out of the snapshot
		FTestHit Kill;
		Kill.Frame = 2;
		Kill.HitboxIndex = 1;
		Kill.Data = MakeHitData(100.f, 100.f, 0);
		PostTestHit(Rig, Handle, Kill);
		Rig.Minions->TickBatch(3);
		TestTrue(*FString::Printf(TEXT("%s: the minion is dying"), ModeName), StateOf(Rig, Handle) == EGSTN_MinionActionState::Dying);
		Entries.Reset();
		Rig.Minions->AppendSnapshotEntities(4, Entries);
		TestEqual(*FString::Printf(TEXT("%s: a dying minion is not hittable"), ModeName), Entries.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionHitsReactionTiersTest, "GSTNMinion.Runtime.Hits.ReactionTiers", GSTN_MINION_HIT_TEST_FLAGS)
bool FGSTN_MinionHitsReactionTiersTest::RunTest(const FString& Parameters)
{
	FHitRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists"), Rig.Minions))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules();
	Rig.Minions->SetCombatRules(Rules.Get());
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set = MakeHurtboxes();
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeDefinition(Set.Get());

	// Three minions far apart, one tier each: 100 stays in place, 600 pushes back, 3000 launches (thresholds 300 and 1000)
	const FGSTN_MinionHandle Stagger = Rig.Spawn(Definition.Get(), FVector(0.0, 0.0, 0.0));
	const FGSTN_MinionHandle Push = Rig.Spawn(Definition.Get(), FVector(1000.0, 0.0, 0.0));
	const FGSTN_MinionHandle Launch = Rig.Spawn(Definition.Get(), FVector(2000.0, 0.0, 0.0));
	Rig.Minions->TickBatch(1);

	TArray<FCapturedOutcome> Outcomes;
	CaptureOutcomes(Rig, Outcomes);
	const float Knockbacks[3] = { 100.f, 600.f, 3000.f };
	const FGSTN_MinionHandle Handles[3] = { Stagger, Push, Launch };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FTestHit Hit;
		Hit.Data = MakeHitData(1.f, Knockbacks[Index], 4);
		PostTestHit(Rig, Handles[Index], Hit);
	}
	Rig.Minions->TickBatch(2);
	TestEqual(TEXT("three outcomes"), Outcomes.Num(), 3);

	// During the hitstop (frames before 1 + 4) every tier is frozen in Hitstop with its reaction pending
	const EGSTN_MinionPendingReaction Expected[3] = { EGSTN_MinionPendingReaction::StaggerInPlace, EGSTN_MinionPendingReaction::PushBack, EGSTN_MinionPendingReaction::Launch };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestTrue(*FString::Printf(TEXT("hit %d: frozen in hitstop"), Index), StateOf(Rig, Handles[Index]) == EGSTN_MinionActionState::Hitstop);
		TestTrue(*FString::Printf(TEXT("hit %d: the tier waits"), Index), Rig.Minions->GetPendingReaction(Handles[Index].Index) == Expected[Index]);
	}
	Rig.Minions->TickBatch(3);
	Rig.Minions->TickBatch(4);
	TestTrue(TEXT("still in hitstop on frame 4"), StateOf(Rig, Stagger) == EGSTN_MinionActionState::Hitstop);
	Rig.Minions->TickBatch(5);
	TestTrue(TEXT("StaggerInPlace -> Hitstun when the hitstop ends"), StateOf(Rig, Stagger) == EGSTN_MinionActionState::Hitstun);
	// A tier with no direction to move along falls back to a stagger in place (ApplyReaction): accept it, but say which
	const bool bPushMoved = Outcomes.IsValidIndex(1) && !Outcomes[1].Outcome.Knockback.LaunchVelocity.IsNearlyZero();
	const bool bLaunchMoved = Outcomes.IsValidIndex(2) && !Outcomes[2].Outcome.Knockback.LaunchVelocity.IsNearlyZero();
	TestTrue(TEXT("PushBack -> PushedBack"), StateOf(Rig, Push) == (bPushMoved ? EGSTN_MinionActionState::PushedBack : EGSTN_MinionActionState::Hitstun));
	TestTrue(TEXT("Launch -> Launched"), StateOf(Rig, Launch) == (bLaunchMoved ? EGSTN_MinionActionState::Launched : EGSTN_MinionActionState::Hitstun));

	// The stagger in place ends at its hitstun end frame
	const int32 HitstunEnd = Rig.Minions->GetHitstunEndFrame(Stagger.Index);
	TestTrue(TEXT("the hitstun ends after the hitstop"), HitstunEnd > 5);
	for (int32 Frame = 6; Frame < HitstunEnd; ++Frame)
	{
		Rig.Minions->TickBatch(Frame);
	}
	TestTrue(TEXT("in hitstun until its end"), StateOf(Rig, Stagger) == EGSTN_MinionActionState::Hitstun);
	Rig.Minions->TickBatch(HitstunEnd);
	TestTrue(TEXT("back to locomotion at the end"), StateOf(Rig, Stagger) == EGSTN_MinionActionState::Idle);

	// With the stub part step in the optional slot (an armoured part): the launcher only staggers
	Rig.Minions->SetExtraVictimStep(UGSTN_MinionTestStep_CapReaction::StaticClass());
	const FGSTN_MinionHandle Armoured = Rig.Spawn(Definition.Get(), FVector(3000.0, 0.0, 0.0));
	const int32 SpawnFrame = HitstunEnd + 1;
	Rig.Minions->TickBatch(SpawnFrame);
	FTestHit Launcher;
	Launcher.Frame = SpawnFrame;
	Launcher.Data = MakeHitData(1.f, 3000.f, 4);
	PostTestHit(Rig, Armoured, Launcher);
	Rig.Minions->TickBatch(SpawnFrame + 1);
	TestTrue(TEXT("the part step capped the tier"), Rig.Minions->GetPendingReaction(Armoured.Index) == EGSTN_MinionPendingReaction::StaggerInPlace);

	// Below the stagger threshold: damage and HitCounter only, no state change, no window
	Rules->StaggerThreshold = 1.0e6f;
	Rig.Minions->SetExtraVictimStep(nullptr);
	const FGSTN_MinionHandle Tough = Rig.Spawn(Definition.Get(), FVector(4000.0, 0.0, 0.0));
	Rig.Minions->TickBatch(SpawnFrame + 2);
	FTestHit Poke;
	Poke.Frame = SpawnFrame + 2;
	Poke.Data = MakeHitData(5.f, 3000.f, 4);
	PostTestHit(Rig, Tough, Poke);
	Rig.Minions->TickBatch(SpawnFrame + 3);
	TestEqual(TEXT("below threshold: damage applies"), Rig.Minions->GetHealth(Tough), 35.f);
	TestEqual(TEXT("below threshold: HitCounter moves"), static_cast<int32>(Rig.Minions->GetStore().HitCounter[Tough.Index]), 1);
	TestTrue(TEXT("below threshold: no reaction state"), StateOf(Rig, Tough) == EGSTN_MinionActionState::Idle);
	TestEqual(TEXT("below threshold: no hitstop window"), Rig.Minions->GetHitstopEndFrame(Tough.Index), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionHitsInboxOrderTest, "GSTNMinion.Runtime.Hits.InboxOrderAndGeneration", GSTN_MINION_HIT_TEST_FLAGS)
bool FGSTN_MinionHitsInboxOrderTest::RunTest(const FString& Parameters)
{
	FHitRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists"), Rig.Minions))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules();
	Rules->StaggerThreshold = 1.0e6f;
	Rig.Minions->SetCombatRules(Rules.Get());
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set = MakeHurtboxes();
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeDefinition(Set.Get());
	const FGSTN_MinionHandle First = Rig.Spawn(Definition.Get(), FVector(0.0, 0.0, 0.0));
	const FGSTN_MinionHandle Second = Rig.Spawn(Definition.Get(), FVector(500.0, 0.0, 0.0));
	Rig.Minions->TickBatch(1);

	TArray<FCapturedOutcome> Outcomes;
	CaptureOutcomes(Rig, Outcomes);

	// Posted out of order: (second, 9, hitbox 1), (second, 9, hitbox 0), (first, 5), (second, 3)
	auto Post = [&Rig](const FGSTN_MinionHandle& Handle, int32 Attacker, uint8 Hitbox)
	{
		FTestHit Hit;
		Hit.AttackerId = Attacker;
		Hit.HitboxIndex = Hitbox;
		Hit.Data = MakeHitData(1.f, 100.f, 0);
		return PostTestHit(Rig, Handle, Hit);
	};
	Post(Second, 9, 1);
	Post(Second, 9, 0);
	Post(First, 5, 0);
	Post(Second, 3, 0);
	// A hit stamped for this frame waits for the next drain (the M4 rule: reaction at F+1)
	FTestHit Late;
	Late.Frame = 2;
	Late.AttackerId = 11;
	Late.Data = MakeHitData(1.f, 100.f, 0);
	PostTestHit(Rig, First, Late);
	Rig.Minions->TickBatch(2);

	const TArray<FIntPoint> ExpectedOrder = { FIntPoint(First.Index, 5), FIntPoint(Second.Index, 3), FIntPoint(Second.Index, 9), FIntPoint(Second.Index, 9) };
	if (TestEqual(TEXT("four hits applied this frame"), Outcomes.Num(), ExpectedOrder.Num()))
	{
		for (int32 Index = 0; Index < ExpectedOrder.Num(); ++Index)
		{
			TestEqual(*FString::Printf(TEXT("hit %d: victim slot"), Index), Outcomes[Index].Slot, ExpectedOrder[Index].X);
			TestEqual(*FString::Printf(TEXT("hit %d: attacker"), Index), Outcomes[Index].AttackerId, ExpectedOrder[Index].Y);
		}
	}
	Rig.Minions->TickBatch(3);
	TestEqual(TEXT("the late hit lands one frame later"), Outcomes.Num(), ExpectedOrder.Num() + 1);

	// HitBy: the same hit delivered twice lands once
	Outcomes.Reset();
	FTestHit Twice;
	Twice.Frame = 3;
	Twice.AttackerId = 21;
	Twice.Data = MakeHitData(1.f, 100.f, 0);
	PostTestHit(Rig, First, Twice);
	PostTestHit(Rig, First, Twice);
	Rig.Minions->TickBatch(4);
	TestEqual(TEXT("HitBy: one landing"), Outcomes.Num(), 1);
	TestEqual(TEXT("HitBy: one duplicate dropped"), Rig.Minions->GetLastBatchStats().HitsDeduped, 1);

	// Generation: a hit stamped for the previous occupant of a recycled slot never lands on the new one
	FTestHit Stale;
	Stale.Frame = 4;
	Stale.AttackerId = 31;
	Stale.Data = MakeHitData(1.f, 100.f, 0);
	PostTestHit(Rig, Second, Stale);
	TestTrue(TEXT("despawn"), Rig.Minions->Despawn(Second));
	const FGSTN_MinionHandle Recycled = Rig.Spawn(Definition.Get(), FVector(500.0, 0.0, 0.0));
	TestEqual(TEXT("the slot is reused"), static_cast<int32>(Recycled.Index), static_cast<int32>(Second.Index));
	TestFalse(TEXT("posting with the old generation is refused"), PostTestHit(Rig, Second, Stale));
	Outcomes.Reset();
	Rig.Minions->TickBatch(5);
	TestEqual(TEXT("the queued stale hit is dropped at the drain"), Outcomes.Num(), 0);
	TestEqual(TEXT("the new occupant is untouched"), Rig.Minions->GetHealth(Recycled), 40.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionHitsAttackerHitstopTest, "GSTNMinion.Runtime.Hits.AttackerHitstopIndependence", GSTN_MINION_HIT_TEST_FLAGS)
bool FGSTN_MinionHitsAttackerHitstopTest::RunTest(const FString& Parameters)
{
	FHitRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists"), Rig.Minions) || !TestNotNull(TEXT("the combat world exists"), Rig.Combat))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules();
	Rig.Minions->SetCombatRules(Rules.Get());
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set = MakeHurtboxes();
	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline = MakeMoveTimeline(30, true, 5);
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeDefinition(Set.Get());
	FGSTN_MinionMove& Move = Definition->Moves.AddDefaulted_GetRef();
	Move.Command = HitTestCommand();
	Move.Timeline = Timeline.Get();
	Move.MaxRange = 300.f;

	FStubFighter Fighter;
	Fighter.Transform = FTransform(FRotator(0.0, 180.0, 0.0), FVector(120.0, 0.0, 0.0));
	TestTrue(TEXT("the stub fighter registers"), Rig.Combat->RegisterEntity(FighterId, Fighter, nullptr, Fighter.Team, Set.Get()));

	const FGSTN_MinionHandle Attacker = Rig.Spawn(Definition.Get(), FVector(0.0, 0.0, 0.0));
	const FGSTN_MinionHandle Bystander = Rig.Spawn(Definition.Get(), FVector(-2000.0, 0.0, 0.0));
	Rig.Minions->TickBatch(1);
	IntendAttack(Rig, Attacker);
	Rig.Minions->TickBatch(2);

	TestEqual(TEXT("the move started"), static_cast<int32>(Rig.Minions->GetStore().MoveId[Attacker.Index]), 1);
	if (TestEqual(TEXT("the fighter's inbox got the hit"), Fighter.Inbox.Num(), 1))
	{
		const FGSTN_HitInboxEntry* Hit = FGSTN_HitInboxEntry::FromInboxEntry(Fighter.Inbox[0]);
		TestNotNull(TEXT("it is a hit"), Hit);
		TestEqual(TEXT("stamped with the batch frame"), Fighter.Inbox[0].Frame, 2);
		if (Hit)
		{
			TestEqual(TEXT("from the minion's pipeline id"), Hit->AttackerId, GSTNEntityKinds::MakeMinionEntityId(Attacker.Index));
			TestEqual(TEXT("with the minion's team"), static_cast<int32>(Hit->AttackerTeam), 1);
		}
	}
	TestEqual(TEXT("the attacker's hitstop: 5 frames after the hit"), Rig.Minions->GetAttackerHitstopEndFrame(Attacker.Index), 2 + 5);
	// Independence: the attacker hitstop is not a victim hitstop, and nobody else is paused
	TestEqual(TEXT("no victim hitstop on the attacker"), Rig.Minions->GetHitstopEndFrame(Attacker.Index), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("the attacker is not in a hit reaction"), StateOf(Rig, Attacker) != EGSTN_MinionActionState::Hitstop);

	for (int32 Frame = 3; Frame <= 8; ++Frame)
	{
		Rig.Minions->TickBatch(Frame);
	}
	TestEqual(TEXT("the move clock stood still for the 5 frames"), Rig.Minions->GetPausedFrames(Attacker.Index), 5);
	TestEqual(TEXT("the bystander was never paused"), Rig.Minions->GetPausedFrames(Bystander.Index), 0);
	TestEqual(TEXT("the same group never hits the fighter twice in one move"), Fighter.Inbox.Num(), 1);
	TestEqual(TEXT("the attacker holds the fighter's token while it swings"), Rig.Minions->GetAttackTokens().NumHolders(FighterId), 1);
	Rig.Combat->UnregisterEntity(FighterId);
	return true;
}

// The attacker's presentation (cue and mesh pause) of a pawn hit on a minion: a minion has no actor, and the presentation used to drop the hit
// (UGSTN_HitPresentationComponent::StartEffect). Same hit data on a minion and on a fighter: the same pause length and cue count
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionHitsAttackerPresentationTest, "GSTNMinion.Runtime.Hits.AttackerHitstopOnMinionHit", GSTN_MINION_HIT_TEST_FLAGS)
bool FGSTN_MinionHitsAttackerPresentationTest::RunTest(const FString& Parameters)
{
	FHitRig Rig;
	if (!TestNotNull(TEXT("the combat world exists"), Rig.Combat))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set = MakeHurtboxes();
	constexpr int32 TimelineStart = 10;
	constexpr int32 HitFrame = 12;
	constexpr int32 AttackerHitstop = 6;
	constexpr float StepMs = 1000.f / 60.f;
	const FGameplayTag Cue = GSTNMinionTags::Entity_Minion.GetTag();

	auto MakeAttacker = [&](UGSTN_HitPresentationComponent*& OutComponent, USkeletalMeshComponent*& OutMesh)
	{
		AActor* Actor = Rig.World->SpawnActor<AActor>();
		OutMesh = NewObject<USkeletalMeshComponent>(Actor);
		Actor->SetRootComponent(OutMesh);
		OutMesh->RegisterComponent();
		OutComponent = NewObject<UGSTN_HitPresentationComponent>(Actor);
		OutComponent->RegisterComponent();
		return Actor;
	};
	auto Present = [&](UGSTN_HitPresentationComponent& Component, int32 VictimId)
	{
		FGSTN_CombatSyncCore Core;
		Core.ActionState = EGSTN_ActionState::InTimeline;
		Core.TimelineStartFrame = TimelineStart;
		Core.Hit.AddHitTarget(VictimId, TimelineStart, 0);
		FGSTN_PredictedHitView View;
		View.Frame = HitFrame;
		View.VictimId = VictimId;
		View.HitGroup = 0;
		View.Timeline = 1;
		View.HitboxIndex = 0;
		View.AttackerHitstopFrames = AttackerHitstop;
		View.CueTag = Cue;
		Component.OnAttackerFrame(Core, HitFrame, 0, MakeArrayView(&View, 1), StepMs);
	};

	UGSTN_HitPresentationComponent* ForMinion = nullptr;
	USkeletalMeshComponent* MinionAttackerMesh = nullptr;
	MakeAttacker(ForMinion, MinionAttackerMesh);
	UGSTN_HitPresentationComponent* ForFighter = nullptr;
	USkeletalMeshComponent* FighterAttackerMesh = nullptr;
	MakeAttacker(ForFighter, FighterAttackerMesh);
	if (!TestNotNull(TEXT("the components exist"), ForMinion) || !TestNotNull(TEXT("the second component exists"), ForFighter))
	{
		return false;
	}

	FStubFighter Fighter;
	AActor* FighterVictim = Rig.World->SpawnActor<AActor>();
	TestTrue(TEXT("the stub fighter registers"), Rig.Combat->RegisterEntity(FighterId, Fighter, FighterVictim, Fighter.Team, Set.Get()));

	const int32 MinionId = GSTNEntityKinds::MakeMinionEntityId(3);
	Present(*ForMinion, MinionId);
	Present(*ForFighter, FighterId);
	Rig.Combat->UnregisterEntity(FighterId);

	TestEqual(TEXT("the attacker's mesh is paused for the minion hit"), ForMinion->GetNumPausedMeshes(), 1);
	TestEqual(TEXT("the attacker's mesh is paused for the fighter hit"), ForFighter->GetNumPausedMeshes(), 1);
	TestEqual(TEXT("the pause is zero rate on the minion hit"), MinionAttackerMesh->GlobalAnimRateScale, 0.f);
	// "started,cancelled,cuesPlayed,cuesSuppressed,active,lastHitstopMs": the same numbers for both victims
	const FString MinionInfo = ForMinion->GetPresentationInfo();
	TestEqual(TEXT("the presentation of a minion hit equals the fighter's (started, cue, active, hitstop ms)"), MinionInfo, ForFighter->GetPresentationInfo());
	TestTrue(TEXT("the minion hit started an effect"), MinionInfo.StartsWith(TEXT("1,")));
	TestTrue(TEXT("the hitstop length is the attacker's frames"), MinionInfo.EndsWith(FString::Printf(TEXT(",%.1f"), AttackerHitstop * StepMs)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionHitsTokensTest,"GSTNMinion.Runtime.Hits.TokenRelease", GSTN_MINION_HIT_TEST_FLAGS)
bool FGSTN_MinionHitsTokensTest::RunTest(const FString& Parameters)
{
	FHitRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists"), Rig.Minions) || !TestNotNull(TEXT("the combat world exists"), Rig.Combat))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules();
	Rig.Minions->SetCombatRules(Rules.Get());
	Rig.Minions->SetMaxAttackersPerTarget(1);
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set = MakeHurtboxes();
	// No hitbox: the tokens are the subject, not the hits
	TStrongObjectPtr<UGSTN_CombatTimeline> Long = MakeMoveTimeline(40, false, 0);
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeDefinition(Set.Get());
	FGSTN_MinionMove& Move = Definition->Moves.AddDefaulted_GetRef();
	Move.Command = HitTestCommand();
	Move.Timeline = Long.Get();
	Move.MaxRange = 500.f;
	Move.CooldownFrames = 0;

	FStubFighter Fighter;
	Fighter.Transform = FTransform(FVector(200.0, 0.0, 0.0));
	Rig.Combat->RegisterEntity(FighterId, Fighter, nullptr, Fighter.Team, Set.Get());

	const FGSTN_MinionHandle A = Rig.Spawn(Definition.Get(), FVector(0.0, 100.0, 0.0));
	const FGSTN_MinionHandle B = Rig.Spawn(Definition.Get(), FVector(0.0, -100.0, 0.0));
	Rig.Minions->TickBatch(1);
	IntendAttack(Rig, A);
	IntendAttack(Rig, B);
	Rig.Minions->TickBatch(2);
	const FGSTN_MinionAttackTokens& Tokens = Rig.Minions->GetAttackTokens();
	TestTrue(TEXT("the lower slot gets the only token"), Tokens.Holds(FighterId, A.Index));
	TestEqual(TEXT("A swings"), static_cast<int32>(Rig.Minions->GetStore().MoveId[A.Index]), 1);
	TestEqual(TEXT("B waits"), static_cast<int32>(Rig.Minions->GetStore().MoveId[B.Index]), 0);
	TestEqual(TEXT("one start was denied"), Rig.Minions->GetLastBatchStats().TokensDenied, 1);

	// Hit-stagger: A's token goes back, B takes it in the same batch
	FTestHit Stagger;
	Stagger.Frame = 2;
	Stagger.Data = MakeHitData(1.f, 100.f, 4);
	PostTestHit(Rig, A, Stagger);
	Rig.Minions->TickBatch(3);
	TestEqual(TEXT("the stagger cancelled A's move"), static_cast<int32>(Rig.Minions->GetStore().MoveId[A.Index]), 0);
	TestFalse(TEXT("A gave its token back"), Tokens.Holds(FighterId, A.Index));
	TestTrue(TEXT("B holds it now"), Tokens.Holds(FighterId, B.Index));
	TestEqual(TEXT("B swings"), static_cast<int32>(Rig.Minions->GetStore().MoveId[B.Index]), 1);

	// Death: B's token goes back
	FTestHit Kill;
	Kill.Frame = 3;
	Kill.Data = MakeHitData(1000.f, 100.f, 0);
	PostTestHit(Rig, B, Kill);
	Rig.Minions->TickBatch(4);
	TestTrue(TEXT("B is dying"), StateOf(Rig, B) == EGSTN_MinionActionState::Dying);
	TestFalse(TEXT("the dead hold no token"), Tokens.Holds(FighterId, B.Index));
	TestEqual(TEXT("nobody holds a token (A is still staggered)"), Tokens.NumHolders(FighterId), 0);

	// Timeline end: a short move against the fighter gives its token back when it ends
	TStrongObjectPtr<UGSTN_CombatTimeline> Short = MakeMoveTimeline(3, false, 0);
	TStrongObjectPtr<UGSTN_MinionDefinition> Quick = MakeDefinition(Set.Get());
	FGSTN_MinionMove& QuickMove = Quick->Moves.AddDefaulted_GetRef();
	QuickMove.Command = HitTestCommand();
	QuickMove.Timeline = Short.Get();
	QuickMove.MaxRange = 500.f;
	QuickMove.CooldownFrames = 1000;
	const FGSTN_MinionHandle C = Rig.Spawn(Quick.Get(), FVector(0.0, 300.0, 0.0));
	Rig.Minions->TickBatch(5);
	IntendAttack(Rig, C);
	Rig.Minions->TickBatch(6);
	TestTrue(TEXT("C holds the token while it swings"), Tokens.Holds(FighterId, C.Index));
	for (int32 Frame = 7; Frame <= 10; ++Frame)
	{
		Rig.Minions->TickBatch(Frame);
	}
	TestEqual(TEXT("C's move ended"), static_cast<int32>(Rig.Minions->GetStore().MoveId[C.Index]), 0);
	TestFalse(TEXT("and its token went back"), Tokens.Holds(FighterId, C.Index));
	Rig.Combat->UnregisterEntity(FighterId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionHitsLagCompTest, "GSTNMinion.Runtime.Hits.LagCompSampleReject", GSTN_MINION_HIT_TEST_FLAGS)
bool FGSTN_MinionHitsLagCompTest::RunTest(const FString& Parameters)
{
	FHitRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists"), Rig.Minions))
	{
		return false;
	}
	// No hurtboxes: a bound radius of 0, so the swept bounds are the feet's path exactly
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeDefinition(nullptr);
	const FGSTN_MinionHandle Handle = Rig.Spawn(Definition.Get());
	Rig.Minions->TickBatch(1);
	// No gravity: the launch flies straight along X at 600 cm/s (9.6 cm per 16 ms frame), under gstn.Minion.LagComp.MaxSpeed (700) as the
	// candidate search assumes
	Rig.Minions->SetFixedStepSeconds(0.016f);
	Rig.Minions->LaunchMinion(Handle, FVector(600.0, 0.0, 0.0), false);

	TMap<int32, FVector> PositionAt;
	for (int32 Frame = 2; Frame <= 12; ++Frame)
	{
		Rig.Minions->TickBatch(Frame);
		PositionAt.Add(Frame, FVector(Rig.Minions->GetStore().Position[Handle.Index]));
	}
	const FVector At5 = PositionAt.FindRef(5);
	const FVector Now = PositionAt.FindRef(12);
	TestTrue(TEXT("the minion moved"), FVector::Dist(At5, Now) > 40.0);

	constexpr int32 StepMs = 16;
	const float RewindMs = static_cast<float>(FGSTN_LagCompTime::TimeOfFrame(5, StepMs));
	TArray<FGSTN_MinionRewoundPose> Poses;

	// Sample: a box around where the minion was at frame 5 finds it there
	Rig.Minions->GatherRewoundCandidates(FBox(At5 - FVector(10.0), At5 + FVector(10.0)), RewindMs, StepMs, Poses);
	if (TestEqual(TEXT("the rewound query finds the minion"), Poses.Num(), 1))
	{
		TestEqual(TEXT("at the rewind frame"), Poses[0].Frame, 5);
		TestTrue(TEXT("where it was then"), Poses[0].Transform.GetLocation().Equals(At5, 0.01));
		TestEqual(TEXT("with its pipeline id"), Poses[0].EntityId, GSTNEntityKinds::MakeMinionEntityId(Handle.Index));
	}

	// Reject: a box around where it is now is a candidate (the inflated hash finds it) but its swept bound at frame 5 is far from it
	const int64 RejectedBefore = Rig.Minions->GetBenchTotals().LagCompRejected;
	Rig.Minions->GatherRewoundCandidates(FBox(Now - FVector(10.0), Now + FVector(10.0)), RewindMs, StepMs, Poses);
	TestEqual(TEXT("the present position is not a rewound hit"), Poses.Num(), 0);
	TestEqual(TEXT("the swept bound rejected the candidate"), Rig.Minions->GetBenchTotals().LagCompRejected, RejectedBefore + 1);

	// The present (no rewind): the box around now finds it
	Rig.Minions->GatherRewoundCandidates(FBox(Now - FVector(10.0), Now + FVector(10.0)), static_cast<float>(FGSTN_LagCompTime::TimeOfFrame(12, StepMs)), StepMs, Poses);
	TestEqual(TEXT("no rewind: the present pose"), Poses.Num(), 1);

	// A freed minion leaves no history behind for the next occupant
	Rig.Minions->Despawn(Handle);
	Rig.Minions->GatherRewoundCandidates(FBox(At5 - FVector(10.0), At5 + FVector(10.0)), RewindMs, StepMs, Poses);
	TestEqual(TEXT("a despawned minion is never rewound"), Poses.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
