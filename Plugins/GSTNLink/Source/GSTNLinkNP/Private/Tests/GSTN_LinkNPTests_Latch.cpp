// Links, Network Prediction half: automation tests of the latch (F12b, m6 spec 7.0, 8): the grab's alternatives pick the latch by the victim's weight,
// the latch attack lands on the ridden partner only, the latched enemy shakes the latcher off with its own move (a release key whose reason is its role's
// data) and the watchdog breaks it, and both roles keep acting. Nothing here names the latch type: the test types are the fixture's.

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/GSTN_LinkHitData.h"
#include "GSTN_LinkTags.h"
#include "GSTN_LinkWatchdog.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitResolution.h"
#include "Pipeline/GSTN_HitStep_LinkBreakOnHit.h"
#include "Pipeline/GSTN_HitStep_LinkRequest.h"
#include "Pipeline/GSTN_LinkHitOutcome.h"
#include "Rules/GSTN_LinkRule_Grab.h"
#include "Rules/GSTN_LinkRule_Latch.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Sim/GSTN_LinkShakeOff.h"
#include "Sim/GSTN_LinkThrow.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "Timeline/GSTN_CombatTimeline.h"

using namespace GSTNLinkNPTest;

namespace
{
	constexpr int32 LatchTestLatcherId = 11;
	constexpr int32 LatchTestLatchedId = 12;
	constexpr int32 LatchTestBystanderId = 13;
	constexpr uint8 LatchTestSerial = 7;
	constexpr int32 LatchTestLinkFrame = 200;

	/** The grab lists the latch as its alternative; grab rule (heaviest 120) and latch rule (lightest 250) on the test types, removed with the scope */
	struct FScopedLatchSetup
	{
		FScopedLinkTypes& Types;
		UGSTN_LinkRule_Grab* GrabRule = nullptr;
		UGSTN_LinkRule_Latch* LatchRule = nullptr;

		explicit FScopedLatchSetup(FScopedLinkTypes& InTypes)
			: Types(InTypes)
		{
			GrabRule = NewObject<UGSTN_LinkRule_Grab>(Types.Grab);
			GrabRule->Rules.MaxGrabWeight = 120.f;
			Types.Grab->Rules.Add(GrabRule);
			LatchRule = NewObject<UGSTN_LinkRule_Latch>(Types.Latch);
			LatchRule->Rules.MinLatchWeight = 250.f;
			Types.Latch->Rules.Add(LatchRule);
			Types.Grab->Alternatives = { TestLatch };
			Types.Latch->TargetRole.ReleaseBreakReason = GSTNLinkTags::BreakReason_ShakeOff;
			FGSTN_LinkRequestTags::Invalidate();
		}

		~FScopedLatchSetup()
		{
			Types.Grab->Rules.Remove(GrabRule);
			Types.Latch->Rules.Remove(LatchRule);
			Types.Grab->Alternatives.Reset();
			Types.Latch->TargetRole.ReleaseBreakReason = FGameplayTag();
			FGSTN_LinkRequestTags::Invalidate();
		}
	};

	struct FLatchRig
	{
		FGSTN_LinkSnapshotExt VictimExt;
		FGSTN_LinkSnapshotExt AttackerExt;
		FGSTN_HitVictimView::FSnapshotExtensionRef Ref;
		FGSTN_HitContextView View;

		FLatchRig(int32 AttackerId, const FGSTN_LinkFragment& Attacker, int32 VictimId, const FGSTN_LinkFragment& Victim, int32 Frame)
		{
			VictimExt.SetFromFragment(Victim);
			AttackerExt.SetFromFragment(Attacker);
			View.Incoming.Frame = Frame;
			View.Incoming.AttackerEntityId = AttackerId;
			View.Victim.EntityId = VictimId;
			Ref.TypeId = GSTNFeatureIds::Link;
			Ref.Data = &VictimExt;
			View.Victim.SetSnapshotExtensions(&Ref, 1);
			View.Incoming.AttackerExtensions[0].TypeId = GSTNFeatureIds::Link;
			View.Incoming.AttackerExtensions[0].Data = &AttackerExt;
		}
	};

	const FGSTN_LinkHitOutcome* FindFormation(const FGSTN_StepResult& Out)
	{
		for (const FInstancedStruct& Extension : Out.OutcomeExtensions)
		{
			if (const FGSTN_LinkHitOutcome* Outcome = Extension.GetPtr<FGSTN_LinkHitOutcome>())
			{
				return Outcome;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPLatchWeightTest, "GSTNLink.NP.Latch.WeightPicksLatchOverGrab", Flags)
bool FGSTN_LinkNPLatchWeightTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	FGSTN_LinkInboxHandlers::Register();
	FScopedLinkTypes Types;
	FScopedLatchSetup Setup(Types);

	// One request type (the grab's tap), three weights: the first type whose rules accept forms
	auto Request = [](uint8 WeightScaleQ, FGSTN_StepResult& Out)
	{
		FLatchRig Rig(LatchTestLatcherId, FGSTN_LinkFragment(), LatchTestLatchedId, FGSTN_LinkFragment(), LatchTestLinkFrame - 1);
		Rig.View.Incoming.Hit.HitType = GSTNCombatHitTags::HitType_Grab;
		Rig.View.Incoming.Hit.LinkType = TestGrab;
		Rig.View.Victim.ReactionState.WeightScaleQ = WeightScaleQ;
		GetDefault<UGSTN_HitStep_LinkRequest>()->Resolve_Implementation(Rig.View, Out);
		return FindFormation(Out);
	};
	FGSTN_StepResult Light;
	const FGSTN_LinkHitOutcome* Grabbed = Request(64, Light);
	TestTrue(TEXT("a standard fighter (100) is grabbed"), Grabbed && Grabbed->LinkTypeIndex == Types.GrabIndex);
	FGSTN_StepResult Heavy;
	const FGSTN_LinkHitOutcome* Latched = Request(192, Heavy);
	TestTrue(TEXT("a big enemy (300) is latched onto, by the same tap"), Latched && Latched->LinkTypeIndex == Types.LatchIndex);
	TestEqual(TEXT("the response names the latch"), Heavy.OutgoingEntries.Num(), 1);
	FGSTN_StepResult Between;
	TestTrue(TEXT("one that fits neither rule (150) is refused"), Request(96, Between) == nullptr && !Between.bHit);

	// The initiator predicted the grab it asked for; the latch response replaces it, and the latch parks the initiator
	FGSTN_LinkFragment Initiator;
	FGSTN_LinkTickExtension::MarkPredicted(Initiator, *UGSTN_LinkRegistry::Get(), TestGrab, LatchTestLatchedId, LatchTestLinkFrame - 1);
	if (Latched && Heavy.OutgoingEntries.Num() == 1)
	{
		const FGSTN_LinkResponseEntry* Response = Heavy.OutgoingEntries[0].Payload.GetPtr<FGSTN_LinkResponseEntry>();
		TestTrue(TEXT("the response applies"), Response && FGSTN_LinkInboxHandlers::ApplyResponse(Initiator, *Response, LatchTestLatchedId));
	}
	TestTrue(TEXT("the initiator holds the latcher role"), Initiator.Links.Body.LinkTypeIndex == Types.LatchIndex && Initiator.Links.Body.Role == 1 && Initiator.Links.Body.IsActive());
	TestTrue(TEXT("and is the parked side"), FGSTN_LinkTickExtension::IsParked(&Initiator, UGSTN_LinkRegistry::Get()));
	TestFalse(TEXT("but keeps acting"), FGSTN_LinkTickExtension::IsActionBlocked(Initiator, UGSTN_LinkRegistry::Get()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPLatchAttackTest, "GSTNLink.NP.Latch.AttackHitsPartnerOnly", Flags)
bool FGSTN_LinkNPLatchAttackTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FScopedLatchSetup Setup(Types);
	const FGSTN_LinkFragment Latcher = MakeLinked(Types.LatchIndex, 1, LatchTestLatchedId, LatchTestSerial, LatchTestLinkFrame + 1);
	const FGSTN_LinkFragment Latched = MakeLinked(Types.LatchIndex, 2, LatchTestLatcherId, LatchTestSerial, LatchTestLinkFrame);

	FGSTN_HitData Attack;
	Attack.Damage = 5.f;
	Attack.BaseKnockback = 600.f;
	Attack.LinkFilter = EGSTN_LinkFilter::LinkedPartnerOnly;
	FGSTN_LinkHitData NoReaction;
	NoReaction.bNoReaction = true;
	Attack.ExtensionData.Add(FInstancedStruct::Make(NoReaction));

	// On the ridden enemy: it lands, its damage stays, its reaction does not (the enemy is not knocked off the latcher)
	FLatchRig OnPartner(LatchTestLatcherId, Latcher, LatchTestLatchedId, Latched, LatchTestLinkFrame + 30);
	OnPartner.View.Incoming.Hit = Attack;
	FGSTN_StepResult Out;
	TestEqual(TEXT("the latch attack continues to the damage steps"), GetDefault<UGSTN_HitStep_LinkRequest>()->Resolve_Implementation(OnPartner.View, Out),
		EGSTN_StepResult::Continue);
	TestTrue(TEXT("it lands on the partner"), Out.bHit && !Out.bNoDamage);
	TestTrue(TEXT("with no reaction"), Out.KnockbackScale == 0.f && Out.HitstunScale == 0.f && Out.bNoFlinch);
	FGSTN_StepResult BreakOut;
	GetDefault<UGSTN_HitStep_LinkBreakOnHit>()->Resolve_Implementation(OnPartner.View, BreakOut);
	TestEqual(TEXT("the latcher's own attack never breaks the latch"), BreakOut.OutcomeExtensions.Num(), 0);

	// On anyone else the same hitbox never lands
	FLatchRig OnBystander(LatchTestLatcherId, Latcher, LatchTestBystanderId, FGSTN_LinkFragment(), LatchTestLinkFrame + 30);
	OnBystander.View.Incoming.Hit = Attack;
	FGSTN_StepResult Other;
	GetDefault<UGSTN_HitStep_LinkRequest>()->Resolve_Implementation(OnBystander.View, Other);
	TestFalse(TEXT("a bystander is never hit"), Other.bHit);

	// Nor from a latcher of another link (another serial): partners are one link, not one type
	const FGSTN_LinkFragment OtherLatcher = MakeLinked(Types.LatchIndex, 1, LatchTestLatchedId, LatchTestSerial + 1, LatchTestLinkFrame + 1);
	FLatchRig Stale(LatchTestLatcherId, OtherLatcher, LatchTestLatchedId, Latched, LatchTestLinkFrame + 30);
	Stale.View.Incoming.Hit = Attack;
	FGSTN_StepResult StaleOut;
	GetDefault<UGSTN_HitStep_LinkRequest>()->Resolve_Implementation(Stale.View, StaleOut);
	TestFalse(TEXT("a link of another serial is not the partner"), StaleOut.bHit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPLatchShakeOffTest, "GSTNLink.NP.Latch.ShakeOffBreaksThroughWatchdog", Flags)
bool FGSTN_LinkNPLatchShakeOffTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FScopedLatchSetup Setup(Types);
	FGSTN_LinkFragment Latcher = MakeLinked(Types.LatchIndex, 1, LatchTestLatchedId, LatchTestSerial, LatchTestLinkFrame + 1);
	FGSTN_LinkFragment Latched = MakeLinked(Types.LatchIndex, 2, LatchTestLatcherId, LatchTestSerial, LatchTestLinkFrame);
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();

	// The latched enemy's release key: its role's reason is the shake-off; the latcher's own release keeps the plain Release
	TestEqual(TEXT("the latched role releases with ShakeOff"), static_cast<int32>(FGSTN_LinkShakeOff::ReleaseReasonOf(*Types.Latch, 2, Registry)),
		static_cast<int32>(EGSTN_LinkBreakReason::ShakeOff));
	TestEqual(TEXT("the latcher role with Release"), static_cast<int32>(FGSTN_LinkShakeOff::ReleaseReasonOf(*Types.Latch, 1, Registry)),
		static_cast<int32>(EGSTN_LinkBreakReason::Release));
	TestTrue(TEXT("the shake-off marks the pending break"), FGSTN_LinkShakeOff::ApplyReleaseKey(Latched, *Types.Latch, Registry));
	TestFalse(TEXT("once: a second key changes nothing"), FGSTN_LinkShakeOff::ApplyReleaseKey(Latched, *Types.Latch, Registry));

	// The watchdog does the rest: both sides, reason ShakeOff, the parked latcher placed by the type's placer
	FGSTN_LinkWatchdogEntity A;
	A.EntityId = LatchTestLatcherId;
	A.Links = Latcher.Links;
	A.Transform = FTransform(FVector(-45.0, 0.0, 60.0));
	FGSTN_LinkWatchdogEntity B;
	B.EntityId = LatchTestLatchedId;
	B.Links = Latched.Links;
	FGSTN_LinkWatchdogLogic::FParams Params;
	Params.Definitions = [](uint8 Index) { return UGSTN_LinkRegistry::Get()->GetDefinition(Index); };
	TArray<FGSTN_LinkWatchdogPost> Posts;
	TArray<FGSTN_LinkWatchdogBreak> Breaks;
	constexpr int32 Frame = 260;
	FGSTN_LinkWatchdogLogic::Evaluate({ A, B }, Frame, Params, Posts, Breaks);
	TestTrue(TEXT("one break, reason ShakeOff"), Breaks.Num() == 1 && Breaks[0].Reason == EGSTN_LinkBreakReason::ShakeOff);
	const FGSTN_LinkWatchdogPost* ToLatcher = Posts.FindByPredicate([](const FGSTN_LinkWatchdogPost& Post) { return Post.TargetId == LatchTestLatcherId; });
	const FGSTN_LinkWatchdogPost* ToLatched = Posts.FindByPredicate([](const FGSTN_LinkWatchdogPost& Post) { return Post.TargetId == LatchTestLatchedId; });
	TestTrue(TEXT("both sides get the command"), ToLatcher && ToLatched);
	TestTrue(TEXT("the parked latcher leaves through the placer (teleport)"), ToLatcher && (ToLatcher->Command.ExitFlags & GSTNLinkExitFlags::Teleport) != 0);
	TestTrue(TEXT("the latched enemy stays where it is"), ToLatched && ToLatched->Command.ExitFlags == 0);
	if (ToLatcher && ToLatched)
	{
		FGSTN_LinkInboxHandlers::ApplyCommand(Latcher, ToLatcher->Command, Frame);
		FGSTN_LinkInboxHandlers::ApplyCommand(Latched, ToLatched->Command, Frame);
	}
	TestTrue(TEXT("both sides end the link at F+1"), !Latcher.Links.Body.IsLinked() && !Latched.Links.Body.IsLinked());
	TestTrue(TEXT("with the shake-off as their reason"), Latcher.Links.Body.GetBreakReason() == EGSTN_LinkBreakReason::ShakeOff
		&& Latched.Links.Body.GetBreakReason() == EGSTN_LinkBreakReason::ShakeOff);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPLatchActingTest, "GSTNLink.NP.Latch.LatchedEnemyKeepsActing", Flags)
bool FGSTN_LinkNPLatchActingTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FScopedLatchSetup Setup(Types);
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const FGSTN_LinkFragment Latcher = MakeLinked(Types.LatchIndex, 1, LatchTestLatchedId, LatchTestSerial, LatchTestLinkFrame + 1);
	const FGSTN_LinkFragment Latched = MakeLinked(Types.LatchIndex, 2, LatchTestLatcherId, LatchTestSerial, LatchTestLinkFrame);

	// Neither role is vetoed: the enemy walks, turns and attacks with the latcher on its back, the latcher attacks it
	TestFalse(TEXT("the latched enemy is not vetoed"), FGSTN_LinkTickExtension::IsActionBlocked(Latched, Registry));
	TestFalse(TEXT("nor parked: it moves itself"), FGSTN_LinkTickExtension::IsParked(&Latched, Registry));
	TestFalse(TEXT("the latcher is not vetoed either"), FGSTN_LinkTickExtension::IsActionBlocked(Latcher, Registry));
	FGameplayTagContainer Tags;
	FGSTN_LinkTickExtension::GatherStateTags(&Latched, Registry, Tags);
	TestTrue(TEXT("the latched role tag is projected (its shake-off binding requires it)"), Tags.HasTagExact(TestLatched));

	// Its own attack on the latcher (the partner) does not break the latch by default (no hit rule for the latcher role)
	TestFalse(TEXT("the enemy's attack on its rider does not qualify for a break"), UGSTN_HitStep_LinkBreakOnHit::QualifiesForBreak(*Types.Latch, 1, true, 800.f));

	// Its moves keep running their tracks: a release key on frame 6 of its timeline is crossed exactly once
	FGSTN_CombatTimelineData Timeline;
	Timeline.TotalFrames = 20;
	FGSTN_LinkOutcomeTrack Track;
	Track.StartFrame = 0;
	Track.EndFrame = 20;
	FGSTN_LinkOutcomeKey Key;
	Key.Frame = 6;
	Key.Outcome = EGSTN_LinkOutcome::Release;
	Track.Keys.Add(Key);
	Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_LinkOutcomeTrack>(Track));
	int32 Crossed = 0;
	for (int32 Frame = 0; Frame < 20; ++Frame)
	{
		TArray<const FGSTN_LinkOutcomeKey*, TInlineAllocator<2>> Keys;
		FGSTN_LinkOutcomeKeys::Collect(Timeline, Frame - 1, Frame, Keys);
		Crossed += Keys.Num();
	}
	TestEqual(TEXT("the key fires once over the move"), Crossed, 1);
	FGSTN_CombatSyncCore In;
	In.ActionState = EGSTN_ActionState::InTimeline;
	In.ActiveTimeline = 3;
	In.TimelineStartFrame = 300;
	In.TimelinePausedFrames = 2;
	FGSTN_CombatSyncCore Out = In;
	int32 Previous = 0;
	int32 Current = 0;
	TestTrue(TEXT("a running move crosses its frames"), FGSTN_LinkOutcomeKeys::GetCrossedFrames(In, Out, 310, Previous, Current) && Previous == 7 && Current == 8);
	Out.TimelinePausedFrames = 3;
	TestFalse(TEXT("a hitstop frame crosses none"), FGSTN_LinkOutcomeKeys::GetCrossedFrames(In, Out, 310, Previous, Current));
	return true;
}

#endif
