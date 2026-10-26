// Automation tests of the hit feel through parts (plan P8): the ricochet prediction of the attacker, the hitstop contributor, the reaction
// weight and the one-shot launch and stagger of a break through the reaction request. The pipeline is the real runner with the part step and the
// weight driven model; the attacker and the victim step are the real pure functions of the combat tick (no world, the snapshot is built by hand),
// and the attacker outcome is the one the module registered.

#include "GSTN_PartsNPTestFixture.h"
#include "Data/GSTN_PartHitData.h"
#include "Data/GSTN_PenetrationTable.h"
#include "Events/GSTN_PartEvents.h"
#include "GSTN_CombatTags.h"
#include "GSTN_PartsTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitRunner.h"
#include "Hits/GSTN_HitSteps.h"
#include "Hits/GSTN_KnockbackModels.h"
#include "Pipeline/GSTN_HitStep_PartResolve.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Rules/GSTN_CombatRules.h"
#include "Sim/GSTN_PartsAttackerOutcome.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "World/GSTN_CombatWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNPartsNPTest;

namespace
{
	/** Hurtbox indices of the parts fixture: its four parts' boxes in order, then the body box */
	constexpr int32 TorsoBox = 0;
	constexpr int32 ArmBox = 1;
	constexpr int32 HandBox = 2;
	constexpr int32 HeadBox = 3;

	/** Penetration that damages every part of the fixture, and one that bounces off the arm (hardness 4) */
	constexpr int32 Strong = 8;
	constexpr int32 Weak = 0;

	/** The rules, the model and the hit as the pipeline runs them: part step, knockback (hitstop inside), and the hitstop requests */
	struct FFeelFixture
	{
		FPartsFixture Parts;
		TStrongObjectPtr<UGSTN_CombatRules> Rules;
		TStrongObjectPtr<UGSTN_KnockbackModel_WeightDriven> Model;
		FGSTN_PartsSnapshotExt Ext;

		FFeelFixture()
		{
			Parts.Set->Parts[Arm].LostMassFraction = 0.3f;
			Parts.Set->Parts[Hand].LostMassFraction = 0.3f;
			Parts.Set->Parts[Head].Reaction.HitstopScale = 2.f;
			Parts.Set->Parts[Head].Reaction.JuggleFactor = 0.5f;
			Parts.Set->RebakeWeightCurve();

			Model.Reset(NewObject<UGSTN_KnockbackModel_WeightDriven>(GetTransientPackage()));
			Model->Tuning.StaggerPerDamage = 0.f;
			Model->Tuning.StaggerPerKnockback = 0.f;
			Model->Tuning.HitstopPerDamage = 0.f;
			Model->Tuning.HitstopPerKnockback = 0.f;
			Model->Tuning.HitstopBase = 10.f;
			Model->Tuning.MaxHitstop = 20;
			Model->Tuning.PushBackThreshold = 100.f;
			Model->Tuning.LaunchThreshold = 600.f;

			Rules.Reset(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
			Rules->KnockbackModel = Model.Get();
			Rules->StaggerThreshold = 0.f;
			Rules->HitPipeline.Reset();
			Rules->HitPipeline.Add(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_TeamFilter::StaticClass()));
			Rules->HitPipeline.Add(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_PartResolve::StaticClass()));
			Rules->HitPipeline.Add(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_ComputeKnockback::StaticClass()));
			Rules->HitPipeline.Add(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_Hitstop::StaticClass()));
			Rules->ResolveHitPipeline();

			Ext.PartSet = Parts.Set.Get();
		}

		FGSTN_HitResolutionContext MakeContext(int32 HurtboxIndex, int32 Penetration) const
		{
			FGSTN_HitResolutionContext Context;
			Context.Incoming.Hit.Damage = 10.f;
			Context.Incoming.Hit.BaseKnockback = 300.f;
			Context.Incoming.Hit.KnockbackGrowth = 0.f;
			Context.Incoming.Hit.StaggerForceMode = EGSTN_StaggerForceMode::Override;
			Context.Incoming.Hit.StaggerForce = 100.f;
			FGSTN_PartHitData PartHit;
			PartHit.Penetration = static_cast<uint8>(Penetration);
			Context.Incoming.Hit.ExtensionData.Add(FInstancedStruct::Make(PartHit));
			Context.Incoming.Relationship = GSTNCombatHitTags::Relationship_Enemy.GetTag();
			Context.Victim.Weight = 100.f;
			Context.Victim.SnapshotExtension = &Ext;
			Context.HurtboxSet = Parts.Hurtboxes.Get();
			Context.HurtboxIndex = static_cast<uint8>(HurtboxIndex);
			return Context;
		}

		/** Runs the pipeline over a context and returns the view the attacker outcome seam would be handed */
		FGSTN_HitContextView Run(FGSTN_HitResolutionContext& Context) const
		{
			FGSTN_HitPipelineRunner::Run(*Rules, Context);
			FGSTN_HitContextView View;
			FGSTN_HitPipelineRunner::MakeView(Rules.Get(), Context, View);
			return View;
		}

		void SetMask(uint16 Mask, std::initializer_list<TPair<int32, uint8>> Damage = {})
		{
			Ext.SetState(MakeState(Mask, 0, Damage));
		}
	};

	const FGSTN_PartHitOutcome* FindOutcome(const FGSTN_HitContextView& View)
	{
		for (const FInstancedStruct& Entry : View.OutcomeExtensions)
		{
			if (const FGSTN_PartHitOutcome* Found = Entry.GetPtr<FGSTN_PartHitOutcome>())
			{
				return Found;
			}
		}
		return nullptr;
	}

	/** An attacker (entity 1, team 1) swinging one sphere hitbox at the arm box of a victim (entity 2, team 2) that carries the parts extension */
	struct FAttackerRig
	{
		FFeelFixture Feel;
		FGSTN_CombatTimelineData Timeline;
		FGSTN_StepSnapshot Snapshot;
		FGSTN_CombatSyncCore Core;
		FGSTN_TimelineFrameResult Scratch;
		TArray<FGSTN_AttackerHit> Hits;

		explicit FAttackerRig(int32 Penetration)
		{
			Timeline.TotalFrames = 30;
			Timeline.FirstActionableFrame = 20;
			Core.ActionState = EGSTN_ActionState::InTimeline;
			Core.ActiveTimeline = 0;
			Core.TimelineStartFrame = 0;
			Core.Hit.ResetRun();

			// One box per part, stacked 100 cm apart, so a small sphere at the arm's height touches the arm box only
			TArray<FGSTN_Hurtbox>& Boxes = Feel.Parts.Hurtboxes->Hurtboxes;
			for (int32 Index = 0; Index < Boxes.Num(); ++Index)
			{
				Boxes[Index].Shape.Type = EGSTN_ShapeType::Sphere;
				Boxes[Index].Shape.Extent = FVector(40.0);
				Boxes[Index].Shape.Local = FTransform(FVector(0.0, 0.0, 100.0 * Index));
			}

			FGSTN_HitboxSpec Spec;
			Spec.Shape.Type = EGSTN_ShapeType::Sphere;
			Spec.Shape.Extent = FVector(30.0);
			Spec.Shape.Local = FTransform(FVector(100.0, 0.0, 100.0 * ArmBox));
			Spec.HitGroup = 0;
			Spec.Data.Damage = 10.f;
			Spec.Data.BaseKnockback = 600.f;
			FGSTN_PartHitData PartHit;
			PartHit.Penetration = static_cast<uint8>(Penetration);
			Spec.Data.ExtensionData.Add(FInstancedStruct::Make(PartHit));
			FGSTN_TimelineTrack_Hitbox Track;
			Track.StartFrame = 0;
			Track.EndFrame = 10;
			Track.Hitboxes.Add(Spec);
			Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));

			// The victim stands 100 cm in front of the attacker's root, so the hitbox (100 cm out, at the arm box's height) meets the arm box only
			FGSTN_SnapshotEntry Victim = Feel.Parts.MakeEntry(2);
			Victim.Team = FGenericTeamId(2);
			Victim.Transform = FTransform(FVector(100.0, 0.0, 0.0));
			Victim.Reaction = FGSTN_VictimReactionState::Make(0.0, 1.0, 0, FGSTN_StaggerThreshold());
			FGSTN_PartsSnapshotExt Ext;
			Ext.PartSet = Feel.Parts.Set.Get();
			Victim.Extension.Set(GSTNFeatureIds::Parts, Ext);
			Victim.BoundRadius = static_cast<float>(UGSTN_CombatWorldSubsystem::ComputeBoundRadius(Feel.Parts.Hurtboxes.Get()));
			Snapshot.Entries.Add(Victim);
			Snapshot.Entries.Sort([](const FGSTN_SnapshotEntry& A, const FGSTN_SnapshotEntry& B) { return A.EntityId < B.EntityId; });
			Snapshot.BuildGrid(UGSTN_CombatWorldSubsystem::DefaultCellSize);

			// The attacker stands where the hitbox (100 cm in front of its root) reaches the victim
			Root = FTransform(FVector(0.0, 0.0, 0.0));
		}

		FTransform Root = FTransform::Identity;

		void Run(int32 Frame)
		{
			FGSTN_AttackerTickInput In;
			In.Frame = Frame;
			In.AttackerId = 1;
			In.AttackerTeam = FGenericTeamId(1);
			In.Timeline = &Timeline;
			In.TimelineFrame = Frame;
			In.Snapshot = &Snapshot;
			In.Rules = Feel.Rules.Get();
			In.Root = Root;
			FGSTN_CombatAttacker::Tick(In, Core, Scratch, Hits);
		}
	};

	/** The victim step over the parts fixture: the rules with the part step only, one entry, the parts fragment as the applier writes it */
	struct FVictimRig
	{
		FFeelFixture Feel;
		FGSTN_SnapshotEntry Self;
		FGSTN_CombatSyncCore Core;
		FGSTN_SyncFragmentArray Fragments;
		FGSTN_VictimResult Result;
		TArray<FGSTN_HitInboxEntry> Entries;

		FVictimRig()
		{
			Feel.Model->Tuning.PushBackThreshold = 0.f;
			Feel.Model->Tuning.LaunchThreshold = 0.f;
			// Only a request staggers in this rig: the hits' stagger force of 100 stays under the threshold
			Feel.Rules->StaggerThreshold = 1000.f;
			Feel.Parts.Set->BodyReaction.StaggerThreshold = 1000.f;
			Self = Feel.Parts.MakeEntry();
			Self.Team = FGenericTeamId(2);
			Self.Up = FVector::UpVector;
			Self.Reaction = FGSTN_VictimReactionState::Make(0.0, 1.0, 0, FGSTN_StaggerThreshold());
			FGSTN_PartsSnapshotExt Ext;
			Ext.PartSet = Feel.Parts.Set.Get();
			Self.Extension.Set(GSTNFeatureIds::Parts, Ext);
			Feel.Rules->HitPipeline.Reset();
			Feel.Rules->HitPipeline.Add(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_PartResolve::StaticClass()));
			Feel.Rules->HitPipeline.Add(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_ComputeKnockback::StaticClass()));
			Feel.Rules->ResolveHitPipeline();
		}

		void AddHit(int32 HurtboxIndex, float Damage, int32 Penetration, bool bRicochet = false)
		{
			FGSTN_HitInboxEntry Entry;
			Entry.Frame = 9;
			Entry.AttackerId = 1;
			Entry.AttackerTeam = 1;
			Entry.HurtboxIndex = static_cast<uint8>(HurtboxIndex);
			Entry.Hit.Damage = Damage;
			Entry.Hit.StaggerForceMode = EGSTN_StaggerForceMode::Override;
			Entry.Hit.StaggerForce = 100.f;
			FGSTN_PartHitData PartHit;
			PartHit.Penetration = static_cast<uint8>(Penetration);
			Entry.Hit.ExtensionData.Add(FInstancedStruct::Make(PartHit));
			Entry.HitLocation = FVector(-40.0, 0.0, 0.0);
			Entry.Relationship = GSTNHitRelationship::ToIndex(GSTNCombatHitTags::Relationship_Enemy.GetTag());
			Entry.bRicochet = bRicochet;
			Entries.Add(Entry);
		}

		void Run(int32 Frame = 10)
		{
			FGSTN_PartsSnapshotExt Ext;
			Ext.PartSet = Feel.Parts.Set.Get();
			Ext.SetState(ReadState(Fragments));
			Self.Extension.Set(GSTNFeatureIds::Parts, Ext);

			FGSTN_VictimTickInput In;
			In.Frame = Frame;
			In.Self = &Self;
			In.Rules = Feel.Rules.Get();
			In.Entries = Entries;
			In.Fragments = &Fragments;
			FGSTN_CombatVictim::Resolve(In, Core, Result);
		}
	};

	/** Part index to hurtbox index of the parts fixture (one box per part, in the part's order) */
	constexpr int32 BoxOf(int32 Part) { return Part; }
}

// ---- Ricochet

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsRicochetPredictsTest, "GSTNParts.NP.Ricochet.AttackerPredictsFromStaticData", TestFlags)
bool FGSTN_PartsRicochetPredictsTest::RunTest(const FString& Parameters)
{
	FFeelFixture Fixture;
	const FGSTN_PartsAttackerOutcome Outcome;

	// The arm (hardness 4) against penetration 0: the band below ricochets; against 8 it does not
	FGSTN_HitResolutionContext Weak0 = Fixture.MakeContext(ArmBox, Weak);
	FGSTN_HitResolutionContext Strong0 = Fixture.MakeContext(ArmBox, Strong);
	const FGSTN_HitContextView WeakView = Fixture.Run(Weak0);
	const FGSTN_HitContextView StrongView = Fixture.Run(Strong0);
	TestTrue(TEXT("the hit still happened (the attacker's loop must reach the seam)"), Weak0.Outcome.bHit);
	TestTrue(TEXT("a hit below the hardness bounces"), Outcome.Evaluate(WeakView) == EGSTN_AttackerHitOutcome::Ricochet);
	TestTrue(TEXT("a hit above does not"), Outcome.Evaluate(StrongView) == EGSTN_AttackerHitOutcome::Normal);

	// Only static data and the mask decide: the torso (hardness 0) takes penetration 0 as an equal band, and the answer for the arm does not move
	// with the damage the arm already took or with a broken hand (the hand is the arm's child, so a broken arm is a stump with the same hardness)
	FGSTN_HitResolutionContext TorsoHit = Fixture.MakeContext(TorsoBox, Weak);
	TestTrue(TEXT("an equal band does not bounce"), Outcome.Evaluate(Fixture.Run(TorsoHit)) == EGSTN_AttackerHitOutcome::Normal);
	for (const uint16 Mask : { uint16(0), uint16(1u << Hand), uint16(1u << Head) })
	{
		Fixture.SetMask(Mask, { { Arm, uint8(200) } });
		FGSTN_HitResolutionContext Context = Fixture.MakeContext(ArmBox, Weak);
		TestTrue(*FString::Printf(TEXT("mask %x: still a ricochet"), int32(Mask)), Outcome.Evaluate(Fixture.Run(Context)) == EGSTN_AttackerHitOutcome::Ricochet);
	}

	// A victim without parts, or a hit the part step never reached, has no entry: Normal
	FGSTN_HitContextView Empty;
	TestTrue(TEXT("no entry, no opinion"), Outcome.Evaluate(Empty) == EGSTN_AttackerHitOutcome::Normal);

	// And the module registered the same answer in the shared registry the attacker step asks
	Fixture.SetMask(0);
	FGSTN_HitResolutionContext Registered = Fixture.MakeContext(ArmBox, Weak);
	TestTrue(TEXT("the registry (the module's instance) says Ricochet"), FGSTN_AttackerOutcomeRegistry::Get().Evaluate(Fixture.Rules.Get(), Fixture.Run(Registered)) == EGSTN_AttackerHitOutcome::Ricochet);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsRicochetNoHitstopTest, "GSTNParts.NP.Ricochet.NoHitstopEitherSide", TestFlags)
bool FGSTN_PartsRicochetNoHitstopTest::RunTest(const FString& Parameters)
{
	// The attacker side: a normal hit opens the attacker's window, a ricochet opens none
	FAttackerRig Normal(Strong);
	Normal.Run(0);
	if (!TestEqual(TEXT("the normal hit landed"), Normal.Hits.Num(), 1))
	{
		return false;
	}
	TestFalse(TEXT("not flagged"), Normal.Hits[0].bRicochet);
	TestTrue(TEXT("the attacker freezes"), Normal.Hits[0].AttackerHitstopFrames > 0 && Normal.Core.HitstopEndFrame > 0);

	FAttackerRig Bounce(Weak);
	const int32 EndBefore = Bounce.Core.HitstopEndFrame;
	Bounce.Run(0);
	if (!TestEqual(TEXT("the ricochet hit is registered (inbox, ring)"), Bounce.Hits.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("flagged on the entry the victim receives"), Bounce.Hits[0].bRicochet && Bounce.Hits[0].Entry.bRicochet);
	TestEqual(TEXT("no hitstop for the attacker"), Bounce.Hits[0].AttackerHitstopFrames, 0);
	TestEqual(TEXT("no window opened"), Bounce.Core.HitstopEndFrame, EndBefore);
	

	// The victim side: the flagged entry opens no window and freezes nothing
	FVictimRig Victim;
	Victim.AddHit(ArmBox, 10.f, Weak, true);
	Victim.Run();
	TestEqual(TEXT("the victim reports the entry"), Victim.Result.Hits.Num(), 1);
	TestEqual(TEXT("no freeze for the victim"), Victim.Result.FreezeFrames, 0);
	TestFalse(TEXT("no victim window"), Victim.Core.IsInHitstop(10));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsRicochetNoConfirmTest, "GSTNParts.NP.Ricochet.NoHitConfirm", TestFlags)
bool FGSTN_PartsRicochetNoConfirmTest::RunTest(const FString& Parameters)
{
	FAttackerRig Normal(Strong);
	Normal.Run(0);
	TestTrue(TEXT("a landed hit raises the confirm flag of its group (the gate of OnHit cancels)"), Normal.Core.Hit.HitConfirmFlags != 0);

	FAttackerRig Bounce(Weak);
	Bounce.Run(0);
	TestEqual(TEXT("a ricochet is not a confirmed hit"), Bounce.Core.Hit.HitConfirmFlags, uint8(0));
	TestTrue(TEXT("and no meter either"), Bounce.Hits.Num() == 1 && Bounce.Hits[0].MeterGain == 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsRicochetVictimTest, "GSTNParts.NP.Ricochet.VictimGetsCueAndEventOnly", TestFlags)
bool FGSTN_PartsRicochetVictimTest::RunTest(const FString& Parameters)
{
	FVictimRig Victim;
	Victim.AddHit(ArmBox, 10.f, Weak, true);
	Victim.Run();
	if (!TestEqual(TEXT("one entry"), Victim.Result.Hits.Num(), 1))
	{
		return false;
	}
	const FGSTN_VictimHit& Hit = Victim.Result.Hits[0];
	TestTrue(TEXT("the entry is a ricochet"), Hit.bRicochet);
	TestFalse(TEXT("no flinch"), Hit.bStaggered);
	TestTrue(TEXT("no hitstun, no action lock"), Victim.Core.ActionState == EGSTN_ActionState::Actionable && !Victim.Core.Hit.HasPendingReaction());
	TestTrue(TEXT("the parts fragment was not written"), ReadState(Victim.Fragments) == FGSTN_PartsState());

	// The events of the victim: the landed one and, from the outcome extension the victim step now keeps for a flagged entry, the one Ricochet event of
	// the part (once: dedupe by part and serial), and no HitReceived
	FGSTN_SimEventOutbox Outbox;
	FGSTN_CombatVictim::AppendHitEvents(Hit, 10, nullptr, nullptr, FGameplayTagContainer(), Outbox);
	int32 Landed = 0;
	int32 Ricochets = 0;
	int32 Received = 0;
	for (const FGSTN_SimEvent& Event : Outbox.Events)
	{
		Landed += Event.EventType == GSTNCombatTags::Event_HitLanded.GetTag() ? 1 : 0;
		Received += Event.EventType == GSTNCombatTags::Event_HitReceived.GetTag() ? 1 : 0;
		if (Event.EventType == GSTNPartsTags::Event_Part_Ricochet.GetTag())
		{
			++Ricochets;
			TestEqual(TEXT("with the arm as its part"), FGSTN_PartEvents::GetPayload(Event).PartIndex, Arm);
		}
	}
	TestEqual(TEXT("the landed event"), Landed, 1);
	TestEqual(TEXT("the victim receives the Ricochet event once"), Ricochets, 1);
	TestEqual(TEXT("and no HitReceived"), Received, 0);
	TestTrue(TEXT("nothing else: landed and ricochet"), Outbox.Events.Num() == 2);

	// The part event made straight from the outcome entry of the part step is the same one Ricochet event, alone
	FFeelFixture Fixture;
	FGSTN_HitResolutionContext Context = Fixture.MakeContext(ArmBox, Weak);
	const FGSTN_HitContextView View = Fixture.Run(Context);
	const FGSTN_PartHitOutcome* Outcome = FindOutcome(View);
	if (!TestNotNull(TEXT("the step wrote an outcome entry"), Outcome))
	{
		return false;
	}
	FGSTN_SimEvent Envelope;
	Envelope.Frame = 10;
	TArray<FGSTN_SimEvent> Events;
	FGSTN_PartEvents::Make(*Outcome, Envelope, Events);
	TestTrue(TEXT("exactly the Ricochet event, with the arm as its part"), Events.Num() == 1 && Events[0].EventType == GSTNPartsTags::Event_Part_Ricochet.GetTag()
		&& FGSTN_PartEvents::GetPayload(Events[0]).PartIndex == Arm);
	TestFalse(TEXT("nothing landed in the pipeline either"), View.Outcome.bHasKnockback || View.Outcome.Damage > 0.f);
	return true;
}

// ---- Hitstop

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsHitstopStaticTest, "GSTNParts.NP.Hitstop.AttackerIgnoresBrokenMaskPercentWeight", TestFlags)
bool FGSTN_PartsHitstopStaticTest::RunTest(const FString& Parameters)
{
	// The part's contribution to the hitstop (its scale and the band's) must not move with anything the attacker cannot see exactly: the broken
	// mask, the damage percent and the weight. HitstopPerKnockback is 0 in the fixture: the knockback term of the combat's own Auto formula
	// depends on the weight, and that is M4's dial, not a part's
	FFeelFixture Fixture;
	struct { uint16 Mask; float Health; float Weight; float Percent; } Cases[] = {
		{ 0, 1.f, 100.f, 0.f }, { uint16(1u << Arm), 1.f, 100.f, 0.f }, { uint16((1u << Arm) | (1u << Hand)), 0.4f, 100.f, 80.f }, { 0, 0.1f, 40.f, 150.f },
	};
	int32 FirstVictim = INDEX_NONE;
	int32 FirstAttacker = INDEX_NONE;
	float FirstScale = -1.f;
	for (const auto& Case : Cases)
	{
		Fixture.SetMask(Case.Mask, { { Head, uint8(100) } });
		FGSTN_HitResolutionContext Context = Fixture.MakeContext(HeadBox, Strong);
		Context.Victim.HealthFraction = Case.Health;
		Context.Victim.Weight = Case.Weight;
		Context.Victim.ReactionState = FGSTN_VictimReactionState::Make(Case.Percent, 1.0, 0, FGSTN_StaggerThreshold());
		FGSTN_HitPipelineRunner::Run(*Fixture.Rules, Context);
		const FGSTN_KnockbackResult& Knockback = Context.Outcome.Knockback;
		if (FirstVictim == INDEX_NONE)
		{
			FirstVictim = Knockback.HitstopFrames;
			FirstAttacker = Knockback.AttackerHitstopFrames;
			FirstScale = Context.Reaction.HitstopScale;
		}
		TestEqual(*FString::Printf(TEXT("mask %x: the scale the part contributes"), int32(Case.Mask)), Context.Reaction.HitstopScale, FirstScale);
		TestEqual(*FString::Printf(TEXT("mask %x: the attacker's frames"), int32(Case.Mask)), Knockback.AttackerHitstopFrames, FirstAttacker);
		TestEqual(*FString::Printf(TEXT("mask %x: the victim's frames"), int32(Case.Mask)), Knockback.HitstopFrames, FirstVictim);
	}
	TestTrue(TEXT("and the weak point does freeze longer than the base (the scale is there)"), FirstAttacker > 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsHitstopAutoOnlyTest, "GSTNParts.NP.Hitstop.PartHitstopScaleAppliesToAutoOnly", TestFlags)
bool FGSTN_PartsHitstopAutoOnlyTest::RunTest(const FString& Parameters)
{
	FFeelFixture Fixture;
	// Auto: the head (scale 2) freezes twice the torso's 10 frames, on both sides
	FGSTN_HitResolutionContext TorsoHit = Fixture.MakeContext(TorsoBox, Strong);
	FGSTN_HitResolutionContext HeadHit = Fixture.MakeContext(HeadBox, Strong);
	FGSTN_HitPipelineRunner::Run(*Fixture.Rules, TorsoHit);
	FGSTN_HitPipelineRunner::Run(*Fixture.Rules, HeadHit);
	TestEqual(TEXT("torso: the base frames"), TorsoHit.Outcome.Knockback.HitstopFrames, 10);
	TestEqual(TEXT("head: scaled, victim"), HeadHit.Outcome.Knockback.HitstopFrames, 20);
	TestEqual(TEXT("head: scaled, attacker"), HeadHit.Outcome.Knockback.AttackerHitstopFrames, 20);

	// Override: the same blows with authored frames are not scaled
	FGSTN_HitResolutionContext HeadOverride = Fixture.MakeContext(HeadBox, Strong);
	HeadOverride.Incoming.Hit.HitstopMode = EGSTN_HitstopMode::Override;
	HeadOverride.Incoming.Hit.HitstopFramesOverride = 7;
	HeadOverride.Incoming.Hit.AttackerHitstopFramesOverride = 5;
	FGSTN_HitPipelineRunner::Run(*Fixture.Rules, HeadOverride);
	TestEqual(TEXT("override: victim frames as authored"), HeadOverride.Outcome.Knockback.HitstopFrames, 7);
	TestEqual(TEXT("override: attacker frames as authored"), HeadOverride.Outcome.Knockback.AttackerHitstopFrames, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsHitstopOverrideFinalTest, "GSTNParts.NP.Hitstop.OverrideIsFinal", TestFlags)
bool FGSTN_PartsHitstopOverrideFinalTest::RunTest(const FString& Parameters)
{
	// A band and a part that both scale the hitstop (3 and 2, even 0) cannot change an authored hitstop: only the cap of the rules does
	FFeelFixture Fixture;
	UGSTN_PenetrationTable* Table = NewObject<UGSTN_PenetrationTable>(GetTransientPackage());
	Table->Bands.Reset();
	FGSTN_PenetrationBand Damaging;
	Damaging.MinDelta = 0;
	Damaging.HitstopScale = 3.f;
	Table->Bands.Add(Damaging);
	FGSTN_PenetrationBand Wall;
	Wall.MinDelta = -15;
	Wall.bRicochet = true;
	Table->Bands.Add(Wall);
	Table->Normalize();
	Fixture.Parts.Set->Table = Table;

	for (const float PartScale : { 2.f, 0.f })
	{
		Fixture.Parts.Set->Parts[Head].Reaction.HitstopScale = PartScale;
		FGSTN_HitResolutionContext Context = Fixture.MakeContext(HeadBox, Strong);
		Context.Incoming.Hit.HitstopMode = EGSTN_HitstopMode::Override;
		Context.Incoming.Hit.HitstopFramesOverride = 7;
		Context.Incoming.Hit.AttackerHitstopFramesOverride = -1;
		FGSTN_HitPipelineRunner::Run(*Fixture.Rules, Context);
		TestEqual(*FString::Printf(TEXT("part scale %.0f: the scales did reach the modifiers"), PartScale), Context.Reaction.HitstopScale, 3.f * PartScale);
		TestEqual(*FString::Printf(TEXT("part scale %.0f: victim frames as authored"), PartScale), Context.Outcome.Knockback.HitstopFrames, 7);
		TestEqual(*FString::Printf(TEXT("part scale %.0f: attacker frames follow the victim's override"), PartScale), Context.Outcome.Knockback.AttackerHitstopFrames, 7);
	}

	// The same through Auto, to show the scale was not simply ignored: scale 0 means no hitstop at all
	FGSTN_HitResolutionContext Auto = Fixture.MakeContext(HeadBox, Strong);
	FGSTN_HitPipelineRunner::Run(*Fixture.Rules, Auto);
	TestEqual(TEXT("auto with a part scale of 0 freezes nothing"), Auto.Outcome.Knockback.HitstopFrames, 0);
	return true;
}

// ---- Reaction

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsReactionWeightTierTest, "GSTNParts.NP.Reaction.WeightTierMovesWithBrokenLimbs", TestFlags)
bool FGSTN_PartsReactionWeightTierTest::RunTest(const FString& Parameters)
{
	// A torso hit of 300 cm/s of bracket: a push back on the intact enemy; as the limbs go the weight falls (each 30 % of the mass) and the same
	// blow launches. The weight never goes below the 35 % clamp however much mass was lost
	FFeelFixture Fixture;
	Fixture.Parts.Set->Parts[Hand].LostMassFraction = 0.9f;
	float PreviousWeight = 2.f;
	float PreviousKnockback = 0.f;
	const struct { uint16 Mask; EGSTN_HitReaction Tier; } Cases[] = {
		{ 0, EGSTN_HitReaction::PushBack },
		{ uint16(1u << Arm), EGSTN_HitReaction::PushBack },
		{ uint16((1u << Arm) | (1u << Hand)), EGSTN_HitReaction::Launch },
	};
	for (const auto& Case : Cases)
	{
		Fixture.SetMask(Case.Mask);
		FGSTN_HitResolutionContext Context = Fixture.MakeContext(TorsoBox, Strong);
		FGSTN_HitPipelineRunner::Run(*Fixture.Rules, Context);
		const float Weight = Context.Reaction.WeightScale;
		TestTrue(*FString::Printf(TEXT("mask %x: lighter than before"), int32(Case.Mask)), Weight < PreviousWeight);
		TestTrue(*FString::Printf(TEXT("mask %x: the knockback grows"), int32(Case.Mask)), Context.Outcome.Knockback.Knockback > PreviousKnockback);
		TestTrue(*FString::Printf(TEXT("mask %x: the tier"), int32(Case.Mask)), Context.Outcome.Knockback.bStaggered && Context.Outcome.Knockback.Reaction == Case.Tier);
		TestTrue(*FString::Printf(TEXT("mask %x: never below the clamp"), int32(Case.Mask)), Weight >= 0.35f - UE_KINDA_SMALL_NUMBER);
		PreviousWeight = Weight;
		PreviousKnockback = Context.Outcome.Knockback.Knockback;
	}
	TestTrue(TEXT("the heavily mutilated enemy sits on the clamp"), FMath::IsNearlyEqual(PreviousWeight, 0.35f, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsReactionJuggleFactorTest, "GSTNParts.NP.Reaction.JuggleFactorPerPart", TestFlags)
bool FGSTN_PartsReactionJuggleFactorTest::RunTest(const FString& Parameters)
{
	// The head (factor 0.5) is hit and the effective weight halves against the torso (factor 1): more knockback from the same blow
	FFeelFixture Fixture;
	Fixture.Parts.Set->Parts[Head].Reaction.HitstopScale = 1.f;
	FGSTN_HitResolutionContext TorsoHit = Fixture.MakeContext(TorsoBox, Strong);
	FGSTN_HitResolutionContext HeadHit = Fixture.MakeContext(HeadBox, Strong);
	FGSTN_HitPipelineRunner::Run(*Fixture.Rules, TorsoHit);
	FGSTN_HitPipelineRunner::Run(*Fixture.Rules, HeadHit);
	TestEqual(TEXT("the weight scale is halved by the part"), HeadHit.Reaction.WeightScale, TorsoHit.Reaction.WeightScale * 0.5f);
	TestEqual(TEXT("so the knockback doubles"), HeadHit.Outcome.Knockback.Knockback, TorsoHit.Outcome.Knockback.Knockback * 2.f);

	// The body's factor applies to every hit, the part's on top of it
	Fixture.Parts.Set->BodyReaction.JuggleFactor = 2.f;
	FGSTN_HitResolutionContext HeadAgain = Fixture.MakeContext(HeadBox, Strong);
	FGSTN_HitPipelineRunner::Run(*Fixture.Rules, HeadAgain);
	TestEqual(TEXT("body factor times part factor"), HeadAgain.Reaction.WeightScale, TorsoHit.Reaction.WeightScale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsReactionAirborneScaleTest, "GSTNParts.NP.Reaction.AirborneJuggleScale", TestFlags)
bool FGSTN_PartsReactionAirborneScaleTest::RunTest(const FString& Parameters)
{
	// The one global is the combat rules' AirborneJuggleScale (default 1), read by the knockback step for a tumbling victim: parts add no second one.
	// It composes with the per-part factor (the head's 0.5): the scale multiplies the knockback, the factor divides the weight
	FFeelFixture Fixture;
	Fixture.Parts.Set->Parts[Head].Reaction.HitstopScale = 1.f;
	TestEqual(TEXT("the default changes nothing"), Fixture.Rules->AirborneJuggleScale, 1.f);

	auto Knockback = [&Fixture](int32 Box, bool bTumbling)
	{
		FGSTN_HitResolutionContext Context = Fixture.MakeContext(Box, Strong);
		Context.Victim.ReactionState = FGSTN_VictimReactionState::Make(0.0, 1.0, bTumbling ? GSTNVictimReactionFlags::Tumble : uint8(0), FGSTN_StaggerThreshold());
		FGSTN_HitPipelineRunner::Run(*Fixture.Rules, Context);
		return Context.Outcome.Knockback.Knockback;
	};

	const float Grounded = Knockback(HeadBox, false);
	TestEqual(TEXT("scale 1: a tumbling head takes the same blow"), Knockback(HeadBox, true), Grounded);
	Fixture.Rules->AirborneJuggleScale = 0.5f;
	TestEqual(TEXT("scale 0.5: a tumbling head takes half"), Knockback(HeadBox, true), Grounded * 0.5f);
	TestEqual(TEXT("and a grounded one is untouched"), Knockback(HeadBox, false), Grounded);
	TestEqual(TEXT("the torso's blow is scaled the same way"), Knockback(TorsoBox, true), Knockback(TorsoBox, false) * 0.5f);
	return true;
}

// ---- Break effects through the reaction request

namespace
{
	void AddLaunch(FGSTN_PartDefinition& Part, float Speed, float UpFraction)
	{
		FGSTN_PartBreakEffect_Launch Launch;
		Launch.Speed = Speed;
		Launch.UpFraction = UpFraction;
		Part.OnBreak.Add(FInstancedStruct::Make(Launch));
	}

	void AddStagger(FGSTN_PartDefinition& Part, int32 Frames)
	{
		FGSTN_PartBreakEffect_Stagger Stagger;
		Stagger.Frames = Frames;
		Part.OnBreak.Add(FInstancedStruct::Make(Stagger));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsBreakLaunchTest, "GSTNParts.NP.BreakEffects.LaunchThroughSeam", TestFlags)
bool FGSTN_PartsBreakLaunchTest::RunTest(const FString& Parameters)
{
	// Breaking the arm (cap 60) launches the victim 800 cm/s, half of it along its up, the other half along the hit; the pipeline of this rig has
	// no knockback model, so without the request the hit staggers nothing
	FVictimRig Control;
	AddLaunch(Control.Feel.Parts.Set->Parts[Arm], 800.f, 0.5f);
	Control.AddHit(ArmBox, 10.f, Strong);
	Control.Run();
	TestFalse(TEXT("a hit that breaks nothing asks for nothing"), Control.Result.Hits[0].bStaggered);

	FVictimRig Rig;
	AddLaunch(Rig.Feel.Parts.Set->Parts[Arm], 800.f, 0.5f);
	Rig.Feel.Parts.Set->Parts[Arm].PartCap = 20.f;
	Rig.AddHit(ArmBox, 100.f, Strong);
	Rig.Run();
	if (!TestEqual(TEXT("one hit"), Rig.Result.Hits.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("the arm broke"), ReadState(Rig.Fragments).IsBroken(Arm));
	TestTrue(TEXT("the victim is staggered into a launch"), Rig.Result.Hits[0].bStaggered && Rig.Result.Hits[0].Reaction == EGSTN_HitReaction::Launch);
	TestTrue(TEXT("hitstun state, launch pending"), Rig.Core.ActionState == EGSTN_ActionState::Hitstun && Rig.Core.Hit.HasPendingReaction());
	TestTrue(TEXT("along the hit and along up, as the effect says"), Rig.Core.Hit.GetPendingLaunch().Equals(FVector(400.0, 0.0, 400.0), 1.0));

	// A re-simulated hit from the same state asks for the same launch, once: the request is a function of the entry and the static set
	FVictimRig Again;
	AddLaunch(Again.Feel.Parts.Set->Parts[Arm], 800.f, 0.5f);
	Again.Feel.Parts.Set->Parts[Arm].PartCap = 20.f;
	Again.AddHit(ArmBox, 100.f, Strong);
	Again.Run();
	TestTrue(TEXT("the same launch on a second run"), Again.Core.Hit.GetPendingLaunch().Equals(Rig.Core.Hit.GetPendingLaunch(), 0.01));
	TestTrue(TEXT("and the same bytes"), ReadState(Again.Fragments) == ReadState(Rig.Fragments));

	// A second hit on the stump breaks nothing new: no new launch
	Rig.Core.ActionState = EGSTN_ActionState::Actionable;
	Rig.Core.Hit.ClearPendingReaction();
	Rig.Entries.Reset();
	Rig.AddHit(ArmBox, 100.f, Strong);
	Rig.Run(12);
	TestFalse(TEXT("nothing broke, nothing launches"), Rig.Result.Hits[0].bStaggered);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsBreakStaggerTest, "GSTNParts.NP.BreakEffects.StaggerFloorThroughSeam", TestFlags)
bool FGSTN_PartsBreakStaggerTest::RunTest(const FString& Parameters)
{
	// The break of the arm buckles the victim for at least 45 frames, and the longest of two breaks in one hit wins (the hand is the arm's child)
	FVictimRig Rig;
	AddStagger(Rig.Feel.Parts.Set->Parts[Arm], 45);
	AddStagger(Rig.Feel.Parts.Set->Parts[Hand], 70);
	Rig.Feel.Parts.Set->Parts[Arm].PartCap = 20.f;
	Rig.AddHit(ArmBox, 100.f, Strong);
	Rig.Run();
	if (!TestEqual(TEXT("one hit"), Rig.Result.Hits.Num(), 1))
	{
		return false;
	}
	const FGSTN_VictimHit& Hit = Rig.Result.Hits[0];
	TestTrue(TEXT("the arm and its hand broke"), (ReadState(Rig.Fragments).BrokenMask & ((1u << Arm) | (1u << Hand))) == ((1u << Arm) | (1u << Hand)));
	TestTrue(TEXT("a stagger in place"), Hit.bStaggered && Hit.Reaction == EGSTN_HitReaction::StaggerInPlace);
	TestTrue(TEXT("for at least the longest floor"), Hit.HitstunFrames >= 70);
	TestTrue(TEXT("hitstun until then"), Rig.Core.ActionState == EGSTN_ActionState::Hitstun && Rig.Core.Hit.HitstunEndFrame >= 10 + 70);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
