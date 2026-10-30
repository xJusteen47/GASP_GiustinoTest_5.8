// Automation tests of the defence pipeline steps, the outcome applier and the punish kind (M6 F7b).

#include "Tests/GSTN_DefenceNPTestFixture.h"

#include "GSTN_DefenceContentLibrary.h"
#include "GSTN_DefenceOutcomeApplier.h"
#include "GSTN_DefenceTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitRunner.h"
#include "Hits/GSTN_HitShapeUtils.h"
#include "Hits/GSTN_HitSteps.h"
#include "Math/GSTN_DefenceMath.h"
#include "Pipeline/GSTN_DefenceHitOutcome.h"
#include "Pipeline/GSTN_HitStep_Parry.h"
#include "Pipeline/GSTN_HitStep_Shield.h"
#include "Rules/GSTN_CombatRules.h"
#include "Sim/GSTN_DefenceInboxKinds.h"
#include "Sim/GSTN_DefenceSnapshotContributor.h"
#include "Sim/GSTN_DefenceSnapshotExt.h"
#include "Simulation/GSTN_CombatInbox.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNDefenceNPTest;

namespace
{
	/** The standard combat rules with the defence steps, defence rules in the attach table, and a snapshot extension the tests shape */
	struct FStepRig
	{
		explicit FStepRig(bool bShieldEnabled = true)
		{
			Rules.Reset(MakeRules(bShieldEnabled));
			Combat.Reset(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
			UGSTN_CombatRules::FillStandard(Combat.Get());
			UGSTN_DefenceContentLibrary::AddDefenceStepsToRules(Combat.Get());
			Ext.RulesIndex = FGSTN_DefenceAttach::IndexOf(Rules.Get());
			Ext.Flags = (bShieldEnabled ? GSTNDefenceSnapshotFlags::ShieldEnabled : 0) | GSTNDefenceSnapshotFlags::KitHasShield;
		}

		~FStepRig()
		{
			FGSTN_DefenceAttach::Reset();
		}

		/** A shield raised at Press, blocking, bubble up, worth Value */
		void Raise(int32 Press, double Value)
		{
			Ext.Flags |= GSTNDefenceSnapshotFlags::Raised | GSTNDefenceSnapshotFlags::Blocks | GSTNDefenceSnapshotFlags::Bubble;
			Ext.ShieldPressFrame = Press;
			Ext.ShieldReleaseFrame = INDEX_NONE;
			Ext.ShieldValueQ = FGSTN_DefenceFragment::QuantizeValue(Value);
		}

		/** A 10 damage enemy hit stamped at HitFrame on HurtboxIndex (0x80 = the bubble) */
		FGSTN_HitResolutionContext MakeHit(int32 HurtboxIndex, int32 HitFrame, FGameplayTag HitType = FGameplayTag())
		{
			FGSTN_HitResolutionContext Context;
			Context.Incoming.Hit.Damage = 10.f;
			Context.Incoming.Hit.BaseKnockback = 30.f;
			Context.Incoming.Hit.HitType = HitType;
			Context.Incoming.Frame = HitFrame;
			Context.Incoming.Relationship = GSTNCombatHitTags::Relationship_Enemy.GetTag();
			Context.Incoming.HitboxToVictim = FVector::ForwardVector;
			Context.HurtboxIndex = static_cast<uint8>(HurtboxIndex);
			FGSTN_HitVictimView::FSnapshotExtensionRef Ref;
			Ref.TypeId = GSTNFeatureIds::Defence;
			Ref.Data = &Ext;
			Context.Victim.SetSnapshotExtensions(&Ref, 1);
			return Context;
		}

		void Run(FGSTN_HitResolutionContext& Context) const
		{
			FGSTN_HitPipelineRunner::Run(*Combat, Context);
		}

		const FGSTN_DefenceHitOutcome* FindOutcome(const FGSTN_HitResolutionContext& Context) const
		{
			for (const FInstancedStruct& Extension : Context.OutcomeExtensions)
			{
				// The deciding outcome: the Parry step also hands a pass-through one (Result None, F8's hit flags) for every hit it lets go on
				const FGSTN_DefenceHitOutcome* Outcome = Extension.GetPtr<FGSTN_DefenceHitOutcome>();
				if (Outcome && Outcome->Result != EGSTN_DefenceHitResult::None)
				{
					return Outcome;
				}
			}
			return nullptr;
		}

		/** The applier on a fragment array, as the victim step calls it */
		FGSTN_OutcomeApplyContext Apply(const FGSTN_HitResolutionContext& Context, FGSTN_SyncFragmentArray& Fragments) const
		{
			FGSTN_HitContextView View;
			FGSTN_HitPipelineRunner::MakeView(Combat.Get(), Context, View);
			FGSTN_OutcomeApplyContext Apply;
			Apply.VictimFragments = &Fragments;
			FGSTN_DefenceOutcomeApplier Applier;
			for (const FInstancedStruct& Extension : Context.OutcomeExtensions)
			{
				if (Extension.GetScriptStruct() == Applier.GetExtensionStruct())
				{
					Applier.Apply(Extension, View, Apply);
				}
			}
			return Apply;
		}

		int32 IndexOfStep(const UClass* Class) const
		{
			return Combat->GetResolvedHitPipeline().IndexOfByPredicate([Class](const TSubclassOf<UGSTN_HitResolutionStep>& Step) { return Step.Get() == Class; });
		}

		TStrongObjectPtr<UGSTN_DefenceRules> Rules;
		TStrongObjectPtr<UGSTN_CombatRules> Combat;
		FGSTN_DefenceSnapshotExt Ext;
	};

	constexpr int32 Bubble = GSTNHurtboxIndex::ExtraIndexBase;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceStepsOrderTest, "GSTNDefence.NP.Steps.ParryBeforeShieldOrder", TestFlags)
bool FGSTN_DefenceStepsOrderTest::RunTest(const FString& Parameters)
{
	FStepRig Rig;
	const int32 Parry = Rig.IndexOfStep(UGSTN_HitStep_Parry::StaticClass());
	const int32 Shield = Rig.IndexOfStep(UGSTN_HitStep_Shield::StaticClass());
	const int32 Knockback = Rig.IndexOfStep(UGSTN_HitStep_ComputeKnockback::StaticClass());
	const int32 HurtState = Rig.IndexOfStep(UGSTN_HitStep_HurtState::StaticClass());
	TestTrue(TEXT("both steps resolved"), Parry != INDEX_NONE && Shield != INDEX_NONE);
	TestTrue(TEXT("HurtState < Parry < Shield < ComputeKnockback"), HurtState < Parry && Parry < Shield && Shield < Knockback);
	const int32 Count = Rig.Combat->HitPipeline.Num();
	TestTrue(TEXT("adding again changes nothing"), UGSTN_DefenceContentLibrary::AddDefenceStepsToRules(Rig.Combat.Get()) && Rig.Combat->HitPipeline.Num() == Count);

	// A press-anchored perfect shield: the same hit on the bubble is parried inside the window, blocked after it. The parry wins
	Rig.Rules->Parry.Anchor = EGSTN_ParryAnchor::Press;
	Rig.Raise(200, 100.0);
	FGSTN_HitResolutionContext Early = Rig.MakeHit(Bubble, 202);
	Rig.Run(Early);
	TestTrue(TEXT("inside the window: parried"), Early.Outcome.bParried);
	TestFalse(TEXT("and not blocked: the Shield step never ran"), Early.Outcome.bBlocked);
	TestEqual(TEXT("no damage"), Early.Outcome.Damage, 0.f);
	const FGSTN_DefenceHitOutcome* EarlyOutcome = Rig.FindOutcome(Early);
	TestTrue(TEXT("the source is the shield"), EarlyOutcome && EarlyOutcome->Result == EGSTN_DefenceHitResult::Parried && EarlyOutcome->Source == EGSTN_ParrySource::Shield);

	FGSTN_HitResolutionContext Late = Rig.MakeHit(Bubble, 210);
	Rig.Run(Late);
	TestFalse(TEXT("after the window: not parried"), Late.Outcome.bParried);
	TestTrue(TEXT("blocked"), Late.Outcome.bBlocked);
	FGSTN_HitResolutionContext Body = Rig.MakeHit(0, 202);
	Rig.Run(Body);
	TestFalse(TEXT("the shield's window needs a hit on the bubble"), Body.Outcome.bParried);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceStepsBlockTest, "GSTNDefence.NP.Steps.ShieldBlockNoDamage", TestFlags)
bool FGSTN_DefenceStepsBlockTest::RunTest(const FString& Parameters)
{
	FStepRig Rig;
	Rig.Raise(100, 80.0);
	FGSTN_HitResolutionContext Hit = Rig.MakeHit(Bubble, 150);
	Rig.Run(Hit);
	TestTrue(TEXT("blocked"), Hit.Outcome.bBlocked);
	TestTrue(TEXT("the hit stands (hitstop, events)"), Hit.Outcome.bHit);
	TestEqual(TEXT("no damage"), Hit.Outcome.Damage, 0.f);
	TestTrue(TEXT("a knockback result without a stagger"), Hit.Outcome.bHasKnockback && !Hit.Outcome.Knockback.bStaggered && Hit.Outcome.Knockback.LaunchVelocity.IsNearlyZero());
	const FGSTN_DefenceHitOutcome* Outcome = Rig.FindOutcome(Hit);
	TestNotNull(TEXT("the outcome for the applier"), Outcome);
	if (!Outcome)
	{
		return false;
	}
	TestEqual(TEXT("blocked, not broken"), static_cast<int32>(Outcome->Result), static_cast<int32>(EGSTN_DefenceHitResult::Blocked));
	TestEqual(TEXT("value 80 - 10 in quarter points"), static_cast<int32>(Outcome->ShieldValueAfterQ), 280);
	TestEqual(TEXT("shield stun from the damage"), Outcome->StunFrames, UGSTN_DefenceMath::ShieldStunFrames(10.0, Rig.Rules->Shield));
	TestEqual(TEXT("the victim's hitstop is the model's x ShieldHitstopMultiplier (1)"), Outcome->HitstopFrames, Hit.Outcome.Knockback.HitstopFrames);
	TestTrue(TEXT("pushed back along the hit"), Outcome->Velocity.X > 0.0 && Outcome->PushbackFrames == Rig.Rules->Shield.PushbackFrames);

	FGSTN_SyncFragmentArray Fragments;
	SetFragment(Fragments, MakeRaised(100, 80.0));
	const FGSTN_OutcomeApplyContext Applied = Rig.Apply(Hit, Fragments);
	const FGSTN_DefenceFragment After = ReadFragment(Fragments);
	TestEqual(TEXT("value re-stamped lower at the hit frame"), static_cast<int32>(After.ShieldValueQ), 280);
	TestEqual(TEXT("stamp"), After.ShieldStampFrame, 150);
	TestTrue(TEXT("still raised: the bubble keeps its size"), After.IsRaised() && !After.IsBreaking());
	TestEqual(TEXT("shield stun after the hitstop"), After.ShieldStunEndFrame, 150 + Outcome->HitstopFrames + Outcome->StunFrames);
	TestEqual(TEXT("push back from the hitstop end"), After.PushbackStartFrame, 150 + Outcome->HitstopFrames);
	TestFalse(TEXT("no reaction request: a block never staggers"), Applied.Reaction.IsSet());

	FGSTN_SimEventOutbox Outbox;
	FGSTN_DefenceOutcomeApplier().AppendEvents(Hit.OutcomeExtensions.Last(), FGSTN_SimEvent(), Outbox);
	TestTrue(TEXT("Combat.Event.Blocked"), Outbox.Events.Num() == 1 && Outbox.Events[0].EventType == GSTNDefenceTags::Event_Blocked.GetTag());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceStepsBodyHitTest, "GSTNDefence.NP.Steps.BodyHitWhileRaisedContinues", TestFlags)
bool FGSTN_DefenceStepsBodyHitTest::RunTest(const FString& Parameters)
{
	FStepRig Rig;
	Rig.Raise(100, 80.0);
	FGSTN_HitResolutionContext Hit = Rig.MakeHit(0, 150);
	FGSTN_HitContextView View;
	FGSTN_HitPipelineRunner::MakeView(Rig.Combat.Get(), Hit, View);
	FGSTN_StepResult Result;
	TestEqual(TEXT("the Shield step continues on a body hurtbox"), static_cast<int32>(GetDefault<UGSTN_HitStep_Shield>()->Resolve(View, Result)), static_cast<int32>(EGSTN_StepResult::Continue));
	TestTrue(TEXT("and writes nothing"), !Result.bBlocked && Result.OutcomeExtensions.IsEmpty() && Result.Events.IsEmpty());

	Rig.Run(Hit);
	TestFalse(TEXT("an ordinary hit: not blocked"), Hit.Outcome.bBlocked);
	TestEqual(TEXT("its damage"), Hit.Outcome.Damage, 10.f);
	TestNull(TEXT("no defence outcome: the shield value is untouched, no event"), Rig.FindOutcome(Hit));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceStepsBreakTest, "GSTNDefence.NP.Steps.BreakLaunchUp", TestFlags)
bool FGSTN_DefenceStepsBreakTest::RunTest(const FString& Parameters)
{
	FStepRig Rig;
	Rig.Raise(100, 5.0);
	FGSTN_HitResolutionContext Hit = Rig.MakeHit(Bubble, 150);
	// Arbitrary gravity: the launch follows the victim's local up, never world Z
	Hit.Incoming.Up = FVector(0.0, 1.0, 0.0);
	Rig.Run(Hit);
	TestTrue(TEXT("blocked (no damage) although it breaks"), Hit.Outcome.bBlocked && Hit.Outcome.Damage == 0.f);
	const FGSTN_DefenceHitOutcome* Outcome = Rig.FindOutcome(Hit);
	TestTrue(TEXT("broken"), Outcome && Outcome->Result == EGSTN_DefenceHitResult::Broken);
	if (!Outcome)
	{
		return false;
	}
	FGSTN_SyncFragmentArray Fragments;
	SetFragment(Fragments, MakeRaised(100, 5.0));
	const FGSTN_OutcomeApplyContext Applied = Rig.Apply(Hit, Fragments);
	const FGSTN_DefenceFragment After = ReadFragment(Fragments);
	TestTrue(TEXT("breaking, no longer raised"), After.IsBreaking() && !After.IsRaised());
	TestEqual(TEXT("value 0"), static_cast<int32>(After.ShieldValueQ), 0);
	TestEqual(TEXT("break end"), After.BreakEndFrame, 150 + Rig.Rules->Shield.BreakStunFrames);
	TestTrue(TEXT("a launch up the local up"), Applied.Reaction.HasLaunch() && Applied.Reaction.LaunchVelocity.Equals(FVector(0.0, Rig.Rules->Shield.BreakLaunch, 0.0), 0.01));
	TestTrue(TEXT("that never tumbles"), Applied.Reaction.bNoTumble);

	TestFalse(TEXT("no bubble while broken"), FGSTN_DefenceSnapshotContributor::BubbleExistsAt(After, 151, Rig.Rules->Shield.ReleaseFrames));

	FGSTN_SimEventOutbox Outbox;
	FGSTN_DefenceOutcomeApplier().AppendEvents(Hit.OutcomeExtensions.Last(), FGSTN_SimEvent(), Outbox);
	TestTrue(TEXT("Combat.Event.ShieldBroken"), Outbox.Events.Num() == 1 && Outbox.Events[0].EventType == GSTNDefenceTags::Event_ShieldBroken.GetTag());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceStepsGrabTest, "GSTNDefence.NP.Steps.GrabPassesThroughBubble", TestFlags)
bool FGSTN_DefenceStepsGrabTest::RunTest(const FString& Parameters)
{
	FStepRig Rig;
	// The bubble as the contributor adds it, over one body hurtbox: an ordinary hit picks the bubble, a grab the body
	const FGameplayTagContainer Ignore = FGSTN_DefenceAttach::MakeBubbleIgnoreHitTypes(nullptr);
	TestTrue(TEXT("the bubble ignores grabs"), Ignore.HasTag(GSTNCombatHitTags::HitType_Grab.GetTag()));
	const FGSTN_ExtraHurtbox Extra = UGSTN_DefenceMath::MakeShieldBubbleHurtbox(Rig.Rules->Shield, Ignore);
	TArray<FGSTN_Hurtbox> Body;
	Body.AddDefaulted();
	const TArray<int32> Overlapped = { 0, Bubble };
	const TArray<FGSTN_ExtraHurtbox> Extras = { Extra };
	TestEqual(TEXT("an ordinary hit lands on the bubble"), UGSTN_HitShapeUtils::PickHurtboxIndex(Overlapped, Body, Extras, FGameplayTag()), Bubble);
	TestEqual(TEXT("a grab passes through it to the body"), UGSTN_HitShapeUtils::PickHurtboxIndex(Overlapped, Body, Extras, GSTNCombatHitTags::HitType_Grab.GetTag()), 0);

	// The grab on the body of a shielding victim inside its perfect-shield window: neither parried nor blocked
	Rig.Rules->Parry.Anchor = EGSTN_ParryAnchor::Press;
	Rig.Raise(200, 100.0);
	FGSTN_HitResolutionContext Grab = Rig.MakeHit(0, 201, GSTNCombatHitTags::HitType_Grab.GetTag());
	Rig.Run(Grab);
	TestFalse(TEXT("a grab is never parried"), Grab.Outcome.bParried);
	TestFalse(TEXT("nor blocked"), Grab.Outcome.bBlocked);
	TestNull(TEXT("no defence outcome"), Rig.FindOutcome(Grab));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceStepsModeOffTest, "GSTNDefence.NP.Steps.ModeOffSkipsShieldNotParry", TestFlags)
bool FGSTN_DefenceStepsModeOffTest::RunTest(const FString& Parameters)
{
	// PvE rules: no shield. Even a fragment that says raised (it never would) is not blocked, but a counter move's window parries
	FStepRig Rig(false);
	Rig.Raise(100, 80.0);
	TestFalse(TEXT("the shield is not available"), Rig.Ext.IsShieldAvailable());
	FGSTN_HitResolutionContext OnBubble = Rig.MakeHit(Bubble, 150);
	Rig.Run(OnBubble);
	TestFalse(TEXT("the Shield step is skipped"), OnBubble.Outcome.bBlocked);

	FStepRig Counter(false);
	Counter.Ext.MoveParryStartFrame = 300;
	Counter.Ext.MoveParryEndFrame = 310;
	Counter.Ext.MoveParryProfile = 1 | FGSTN_DefenceStepUtils::CloseOnParryBit;
	FGSTN_HitResolutionContext Hit = Counter.MakeHit(0, 305);
	Counter.Run(Hit);
	TestTrue(TEXT("the move window parries"), Hit.Outcome.bParried);
	TestEqual(TEXT("no damage"), Hit.Outcome.Damage, 0.f);
	const FGSTN_DefenceHitOutcome* Outcome = Counter.FindOutcome(Hit);
	TestTrue(TEXT("source Move, the counter profile"), Outcome && Outcome->Source == EGSTN_ParrySource::Move && Outcome->ProfileIndex == 1);
	TestTrue(TEXT("the window closes after the hit (bCloseOnParry)"), Outcome && Outcome->MoveWindowEndFrame == 306);
	TestEqual(TEXT("the profile's hitstop on the victim"), Hit.Outcome.Knockback.HitstopFrames, Counter.Rules->ParryProfiles[1].HitstopFrames);
	TestEqual(TEXT("and the attacker"), Hit.Outcome.Knockback.AttackerHitstopFrames, Counter.Rules->ParryProfiles[1].HitstopFrames);

	FGSTN_SyncFragmentArray Fragments;
	FGSTN_DefenceFragment Window;
	Window.MoveParryStartFrame = 300;
	Window.MoveParryEndFrame = 310;
	SetFragment(Fragments, Window);
	Counter.Apply(Hit, Fragments);
	const FGSTN_DefenceFragment After = ReadFragment(Fragments);
	TestEqual(TEXT("LastParryFrame drives OnParry"), After.LastParryFrame, 305);
	TestEqual(TEXT("the window closed"), After.MoveParryEndFrame, 306);

	FGSTN_HitResolutionContext Outside = Counter.MakeHit(0, 312);
	Counter.Run(Outside);
	TestFalse(TEXT("outside the window: an ordinary hit"), Outside.Outcome.bParried);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceStepsPunishTest, "GSTNDefence.NP.Steps.OutgoingPunishPosted", TestFlags)
bool FGSTN_DefenceStepsPunishTest::RunTest(const FString& Parameters)
{
	FStepRig Rig(false);
	Rig.Ext.MoveParryStartFrame = 300;
	Rig.Ext.MoveParryEndFrame = 310;
	Rig.Ext.MoveParryProfile = 1;
	FGSTN_HitResolutionContext Hit = Rig.MakeHit(0, 305);
	Rig.Run(Hit);
	TestEqual(TEXT("one entry back to the attacker"), Hit.OutgoingEntries.Num(), 1);
	if (Hit.OutgoingEntries.Num() != 1)
	{
		return false;
	}
	const FGSTN_InboxEntry& Entry = Hit.OutgoingEntries[0];
	const FGSTN_DefencePunishEntry* Punish = Entry.Payload.GetPtr<FGSTN_DefencePunishEntry>();
	TestEqual(TEXT("the defence's Punish kind"), static_cast<int32>(Entry.Kind), static_cast<int32>(GSTNDefenceInboxKinds::Punish));
	TestTrue(TEXT("Parried, the profile's stun, anchored on the hit"), Punish && Punish->Kind == GSTNPunishKinds::Parried && Punish->StunFrames == 20 && Punish->HitFrame == 305
		&& Punish->HitstopFrames == Rig.Rules->ParryProfiles[1].HitstopFrames);
	TestNotNull(TEXT("the module registered the kind"), FGSTN_InboxKindRegistry::Get().Find(GSTNDefenceInboxKinds::Punish));

	// The attacker consumes it two frames after the hit: Punished until the parry hitstop end plus the stun, its move cancelled
	FGSTN_CombatSyncState Attacker;
	FGSTN_CombatTimelineLogic::Start(Attacker.Core, 300, 3, EGSTN_ActivationSource::Command);
	Attacker.Core.HitstopStartFrame = 305;
	Attacker.Core.HitstopEndFrame = 305 + Punish->HitstopFrames;
	FGSTN_InboxDispatchContext Dispatch;
	Dispatch.Frame = 307;
	Dispatch.Sync = &Attacker;
	FGSTN_InboxEntry Delivered = Entry;
	Delivered.Frame = 306;
	TestTrue(TEXT("dispatched"), FGSTN_InboxKindRegistry::Get().Dispatch(Delivered, Dispatch));
	const FGSTN_DefenceFragment After = ReadFragment(Attacker.Fragments);
	TestEqual(TEXT("punished until the hitstop end plus the stun"), After.PunishEndFrame, 305 + Punish->HitstopFrames + 20);
	TestTrue(TEXT("punished now"), After.IsPunished(307));
	TestTrue(TEXT("its move was cancelled"), Attacker.Core.ActionState != EGSTN_ActionState::InTimeline);
	TestEqual(TEXT("the hitstop it predicted is kept"), Attacker.Core.HitstopEndFrame, 305 + Punish->HitstopFrames);

	// A profile without attacker stun ("advantage only"): no entry
	Rig.Rules->ParryProfiles[1].AttackerStunFrames = 0;
	FGSTN_HitResolutionContext Advantage = Rig.MakeHit(0, 305);
	Rig.Run(Advantage);
	TestTrue(TEXT("parried"), Advantage.Outcome.bParried);
	TestEqual(TEXT("no punish without attacker stun"), Advantage.OutgoingEntries.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
