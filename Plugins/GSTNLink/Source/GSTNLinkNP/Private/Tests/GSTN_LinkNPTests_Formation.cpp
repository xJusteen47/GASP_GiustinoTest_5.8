// Automation tests of link formation (plan F10, m6 spec 7.2, 9 steps 3 and 4): the target accepts at F+1 and the initiator takes its role at
// F+2 through the inbox; a refused request whiffs on both sides with no hitstop; two requests on one target, the first wins; chains are
// refused; a predicted request with no response clears after the timeout; a response that re-arrives after a rollback changes nothing; and a
// latch, whose PARKED role is the initiator's, forms through the very same step. No world: the steps, the applier and the handlers on views,
// fragments and an inbox.

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_LinkSettings.h"
#include "GSTN_LinkTags.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitResolution.h"
#include "Pipeline/GSTN_HitStep_GrabImmunity.h"
#include "Pipeline/GSTN_HitStep_LinkRequest.h"
#include "Pipeline/GSTN_LinkHitOutcome.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "Simulation/GSTN_CombatInbox.h"

using namespace GSTNLinkNPTest;

namespace
{
	constexpr int32 HitFrame = 100;

	/** One request as both sides' pipelines see it: the snapshot extensions of the end of the previous frame, the hit of frame F */
	struct FRequestRig
	{
		FGSTN_LinkSnapshotExt TargetExt;
		FGSTN_LinkSnapshotExt InitiatorExt;
		FGSTN_HitContextView View;

		FRequestRig(const FGameplayTag& LinkType, int32 InitiatorId, const FGSTN_LinkFragment& Initiator, int32 TargetId, const FGSTN_LinkFragment& Target)
		{
			TargetExt.SetFromFragment(Target);
			InitiatorExt.SetFromFragment(Initiator);
			View.Incoming.Hit.HitType = GSTNCombatHitTags::HitType_Grab;
			View.Incoming.Hit.LinkType = LinkType;
			View.Incoming.Frame = HitFrame;
			View.Incoming.AttackerEntityId = InitiatorId;
			View.Victim.EntityId = TargetId;
			Point();
		}

		/** The view points at this rig's extensions (call again after a copy or a change of TargetExt) */
		void Point()
		{
			FGSTN_HitVictimView::FSnapshotExtensionRef Ref;
			Ref.TypeId = GSTNFeatureIds::Link;
			Ref.Data = &TargetExt;
			View.Victim.SetSnapshotExtensions(&Ref, 1);
			View.Incoming.AttackerExtensions[0].TypeId = GSTNFeatureIds::Link;
			View.Incoming.AttackerExtensions[0].Data = &InitiatorExt;
		}

		EGSTN_StepResult Run(FGSTN_StepResult& Out) const
		{
			return GetDefault<UGSTN_HitStep_LinkRequest>()->Resolve_Implementation(View, Out);
		}
	};

	/** The applier on the target's fragments, as the victim step runs it at F+1 */
	void ApplyOutcome(const FGSTN_StepResult& Out, const FGSTN_HitContextView& View, FGSTN_SyncFragmentArray& TargetFragments)
	{
		FGSTN_LinkOutcomeApplier Applier;
		FGSTN_OutcomeApplyContext Context;
		Context.VictimFragments = &TargetFragments;
		for (const FInstancedStruct& Extension : Out.OutcomeExtensions)
		{
			if (Extension.GetScriptStruct() == FGSTN_LinkHitOutcome::StaticStruct())
			{
				Applier.Apply(Extension, View, Context);
			}
		}
	}

	FGSTN_LinkFragment PredictedOn(uint8 TypeIndex, int32 TargetId)
	{
		FGSTN_LinkFragment Fragment;
		FGSTN_LinkTickExtension::MarkPredicted(Fragment, *UGSTN_LinkRegistry::Get(), UGSTN_LinkRegistry::Get()->GetLinkType(TypeIndex), TargetId, HitFrame);
		return Fragment;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFormationAcceptedTest, "GSTNLink.NP.Formation.AcceptedAtF1ResponseAtF2", Flags)
bool FGSTN_LinkFormationAcceptedTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	FGSTN_LinkInboxHandlers::Register();
	FScopedLinkTypes Types;

	// F: the initiator's own attacker step marked its predicted request; the target is free
	FGSTN_LinkFragment Initiator = PredictedOn(Types.GrabIndex, 6);
	TestTrue(TEXT("predicted: pending, no role"), Initiator.Links.Body.Phase == EGSTN_LinkPhase::Pending && Initiator.Links.Body.Role == 0);
	FRequestRig Rig(TestGrab, 5, Initiator, 6, FGSTN_LinkFragment());
	FGSTN_StepResult Out;
	TestEqual(TEXT("the request stops the pipeline"), Rig.Run(Out), EGSTN_StepResult::Stop);
	TestTrue(TEXT("an accepted request stands"), Out.bHit);
	TestTrue(TEXT("and does no damage"), Out.bNoDamage);
	TestEqual(TEXT("one outcome for the applier"), Out.OutcomeExtensions.Num(), 1);
	TestEqual(TEXT("one entry back to the initiator"), Out.OutgoingEntries.Num(), 1);

	// F+1: the target's role holds from the frame after the hit
	FGSTN_SyncFragmentArray TargetFragments;
	ApplyOutcome(Out, Rig.View, TargetFragments);
	const FGSTN_LinkFragment* Target = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(TargetFragments);
	if (!TestNotNull(TEXT("the applier wrote the target's fragment"), Target))
	{
		return false;
	}
	TestEqual(TEXT("target: role 2"), static_cast<int32>(Target->Links.Body.Role), 2);
	TestTrue(TEXT("target: active"), Target->Links.Body.Phase == EGSTN_LinkPhase::Active);
	TestEqual(TEXT("target: link frame F+1"), Target->Links.Body.LinkFrame, HitFrame + 1);
	TestEqual(TEXT("target: the initiator as partner"), static_cast<int32>(Target->Links.Body.PartnerId.Id), 5);
	TestEqual(TEXT("target: serial one after both sides' (0, 0)"), static_cast<int32>(Target->Links.Body.Serial), 1);
	TestTrue(TEXT("target: parked and vetoed from F+1"), FGSTN_LinkTickExtension::IsActionBlocked(*Target, UGSTN_LinkRegistry::Get()));

	// The victim step stamps the reply with its frame (F+1); the initiator's inbox delivers it at F+2, not before
	FGSTN_InboxEntry Reply = Out.OutgoingEntries[0];
	Reply.Frame = HitFrame + 1;
	Reply.SenderId = 6;
	FGSTN_CombatInbox Inbox;
	Inbox.Add(Reply);
	TArray<FGSTN_InboxEntry> Drained;
	Inbox.Drain(HitFrame + 1, Drained);
	TestEqual(TEXT("nothing at F+1"), Drained.Num(), 0);
	Inbox.Drain(HitFrame + 2, Drained);
	TestEqual(TEXT("the response at F+2"), Drained.Num(), 1);

	FGSTN_CombatSyncState InitiatorSync;
	FGSTN_SyncFragmentList::FindOrAdd<FGSTN_LinkFragment>(InitiatorSync.Fragments) = Initiator;
	FGSTN_InboxDispatchContext Dispatch;
	Dispatch.Frame = HitFrame + 2;
	Dispatch.Sync = &InitiatorSync;
	TestTrue(TEXT("the response kind is registered"), FGSTN_InboxKindRegistry::Get().Dispatch(Drained[0], Dispatch));
	const FGSTN_LinkFragment* Formed = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(InitiatorSync.Fragments);
	TestTrue(TEXT("initiator: role 1 active at F+2"), Formed && Formed->Links.Body.Role == 1 && Formed->Links.Body.Phase == EGSTN_LinkPhase::Active);
	TestTrue(TEXT("initiator: the same serial and link frame as the target"), Formed && Formed->Links.Body.Serial == Target->Links.Body.Serial
		&& Formed->Links.Body.LinkFrame == HitFrame + 1);
	TestFalse(TEXT("initiator: the predicted flag is gone"), Formed && Formed->Links.Body.HasFlag(GSTNLinkFlags::Predicted));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFormationRefusedTest, "GSTNLink.NP.Formation.RefusedWhiffsNoHitstop", Flags)
bool FGSTN_LinkFormationRefusedTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;

	// The target is held by someone else: the request is refused on the snapshot both sides read
	FRequestRig Rig(TestGrab, 5, PredictedOn(Types.GrabIndex, 6), 6, MakeLinked(Types.GrabIndex, 2, 9, 4, 80));
	FGSTN_StepResult Out;
	TestEqual(TEXT("the refused request stops"), Rig.Run(Out), EGSTN_StepResult::Stop);
	TestFalse(TEXT("and whiffs: no registration, so no hitstop on either side"), Out.bHit);
	TestEqual(TEXT("no outcome"), Out.OutcomeExtensions.Num(), 0);
	TestEqual(TEXT("no entry (a hit that does not stand posts nothing)"), Out.OutgoingEntries.Num(), 0);
	TestFalse(TEXT("no knockback, so no hitstop window"), Out.bHasKnockback);

	// Immune by frame: the GrabImmunity step stops it first, with no registration
	FGSTN_LinkFragment Immune;
	Immune.GrabImmunityEndFrame = HitFrame + 5;
	FRequestRig ImmuneRig(TestGrab, 5, FGSTN_LinkFragment(), 6, Immune);
	FGSTN_StepResult ImmuneOut;
	TestEqual(TEXT("immunity stops the grab"), GetDefault<UGSTN_HitStep_GrabImmunity>()->Resolve_Implementation(ImmuneRig.View, ImmuneOut), EGSTN_StepResult::Stop);
	TestFalse(TEXT("an immune target is not grabbed"), ImmuneOut.bHit);
	ImmuneRig.View.Incoming.Frame = HitFrame + 5;
	FGSTN_StepResult LaterOut;
	TestEqual(TEXT("the immunity ends at its frame"), GetDefault<UGSTN_HitStep_GrabImmunity>()->Resolve_Implementation(ImmuneRig.View, LaterOut), EGSTN_StepResult::Continue);

	// A plain hit is no link request
	FRequestRig Plain(FGameplayTag(), 5, FGSTN_LinkFragment(), 6, FGSTN_LinkFragment());
	Plain.View.Incoming.Hit.HitType = FGameplayTag();
	FGSTN_StepResult PlainOut;
	TestEqual(TEXT("a hit with no link type continues"), Plain.Run(PlainOut), EGSTN_StepResult::Continue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFormationSimultaneousTest, "GSTNLink.NP.Formation.SimultaneousFirstWins", Flags)
bool FGSTN_LinkFormationSimultaneousTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	FScopedLinkTypes Types;

	// Two grabbers, 5 and 7, reach target 6 on the same frame; the drain order puts 5 first
	FGSTN_SyncFragmentArray TargetFragments;
	FRequestRig First(TestGrab, 5, PredictedOn(Types.GrabIndex, 6), 6, FGSTN_LinkFragment());
	FGSTN_StepResult FirstOut;
	First.Run(FirstOut);
	TestTrue(TEXT("the first request is accepted"), FirstOut.bHit && FirstOut.OutcomeExtensions.Num() == 1);
	ApplyOutcome(FirstOut, First.View, TargetFragments);

	// The victim drain rebuilds the link extension from the fragments the first entry wrote: the second request sees the target busy
	const FGSTN_LinkFragment* AfterFirst = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(TargetFragments);
	FRequestRig Second(TestGrab, 7, PredictedOn(Types.GrabIndex, 6), 6, AfterFirst ? *AfterFirst : FGSTN_LinkFragment());
	FGSTN_StepResult SecondOut;
	Second.Run(SecondOut);
	TestFalse(TEXT("the second is refused"), SecondOut.bHit);
	ApplyOutcome(SecondOut, Second.View, TargetFragments);
	const FGSTN_LinkFragment* Target = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(TargetFragments);
	TestTrue(TEXT("the target keeps the first grabber"), Target && Target->Links.Body.PartnerId.Id == 5);

	// The loser's prediction clears by the timeout, on every machine at the same frame
	FGSTN_LinkFragment Loser = PredictedOn(Types.GrabIndex, 6);
	FGSTN_LinkTickExtension::StepLinkTick(Loser, HitFrame + UGSTN_LinkSettings::Get().RequestTimeoutFrames, UGSTN_LinkSettings::Get().RequestTimeoutFrames);
	TestTrue(TEXT("the loser is free again"), Loser.Links.Body.Phase == EGSTN_LinkPhase::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFormationChainTest, "GSTNLink.NP.Formation.ChainRefused", Flags)
bool FGSTN_LinkFormationChainTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;

	// A grabber that already holds someone cannot grab again
	FRequestRig Busy(TestGrab, 5, MakeLinked(Types.GrabIndex, 1, 9, 2, 60), 6, FGSTN_LinkFragment());
	FGSTN_StepResult BusyOut;
	Busy.Run(BusyOut);
	TestFalse(TEXT("a grabbing initiator is refused"), BusyOut.bHit);

	// Nobody grabs a grabber: its Body channel is taken
	FRequestRig Grabber(TestGrab, 5, FGSTN_LinkFragment(), 6, MakeLinked(Types.GrabIndex, 1, 9, 2, 60));
	FGSTN_StepResult GrabberOut;
	Grabber.Run(GrabberOut);
	TestFalse(TEXT("grabbing a grabber is refused"), GrabberOut.bHit);

	// A latch onto a grabbed entity (another Body type) is refused too: two Body links never coexist
	FRequestRig Latch(TestLatch, 5, FGSTN_LinkFragment(), 6, MakeLinked(Types.GrabIndex, 2, 9, 2, 60));
	FGSTN_StepResult LatchOut;
	Latch.Run(LatchOut);
	TestFalse(TEXT("a latch on a held entity is refused"), LatchOut.bHit);

	// A throw is valid only from the Body partner of the same serial
	FRequestRig Throw(FGameplayTag(), 5, MakeLinked(Types.GrabIndex, 1, 6, 3, 60), 6, MakeLinked(Types.GrabIndex, 2, 5, 3, 60));
	Throw.View.Incoming.Hit.HitType = GSTNLinkTags::HitType_Throw;
	FGSTN_StepResult ThrowOut;
	TestEqual(TEXT("the partner's throw continues"), Throw.Run(ThrowOut), EGSTN_StepResult::Continue);
	Throw.TargetExt.BodySerial = 4;
	FGSTN_StepResult StaleOut;
	TestEqual(TEXT("a throw of another serial stops"), Throw.Run(StaleOut), EGSTN_StepResult::Stop);
	TestFalse(TEXT("and does not land"), StaleOut.bHit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFormationTimeoutTest, "GSTNLink.NP.Formation.PredictedClearedOnTimeout", Flags)
bool FGSTN_LinkFormationTimeoutTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	const int32 Timeout = 12;

	FGSTN_LinkFragment Fragment = PredictedOn(Types.GrabIndex, 6);
	Fragment.Links.Body.Serial = 7;
	Fragment.Links.Body.LinkFrame = HitFrame;
	TestFalse(TEXT("one frame before the timeout: still pending"), FGSTN_LinkTickExtension::StepLinkTick(Fragment, HitFrame + Timeout - 1, Timeout));
	TestTrue(TEXT("still pending"), Fragment.Links.Body.Phase == EGSTN_LinkPhase::Pending);
	TestTrue(TEXT("at the timeout: cleared"), FGSTN_LinkTickExtension::StepLinkTick(Fragment, HitFrame + Timeout, Timeout));
	TestTrue(TEXT("idle"), Fragment.Links.Body.IsIdle());
	TestEqual(TEXT("the serial survives, so a late response stays old"), static_cast<int32>(Fragment.Links.Body.Serial), 7);

	// A live link never times out by this rule (its end is the mash's or the definition's, judged by the watchdog)
	FGSTN_LinkFragment Live = MakeLinked(Types.GrabIndex, 1, 6, 3, HitFrame);
	TestFalse(TEXT("a live link is untouched"), FGSTN_LinkTickExtension::StepLinkTick(Live, HitFrame + 500, Timeout));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFormationIdempotentTest, "GSTNLink.NP.Formation.SerialIdempotentAfterRollback", Flags)
bool FGSTN_LinkFormationIdempotentTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;

	FGSTN_LinkResponseEntry Response;
	Response.bAccepted = true;
	Response.LinkTypeIndex = Types.GrabIndex;
	Response.Serial = 1;
	Response.LinkFrame = HitFrame + 1;

	const FGSTN_LinkFragment Predicted = PredictedOn(Types.GrabIndex, 6);
	FGSTN_LinkFragment Live = Predicted;
	TestTrue(TEXT("the response forms the link"), FGSTN_LinkInboxHandlers::ApplyResponse(Live, Response, 6));
	const FGSTN_LinkFragment Formed = Live;
	TestFalse(TEXT("the same response again changes nothing"), FGSTN_LinkInboxHandlers::ApplyResponse(Live, Response, 6));
	TestTrue(TEXT("the state is the formed one"), Live == Formed);

	// A rollback restores the predicted state of F+1; the re-simulated sender posts the entry again and it lands on the same state
	FGSTN_LinkFragment Restored = Predicted;
	TestTrue(TEXT("after the rollback the entry applies again"), FGSTN_LinkInboxHandlers::ApplyResponse(Restored, Response, 6));
	TestTrue(TEXT("to the same formed state"), Restored == Formed);

	// An older serial (a stale entry of a previous link) never replaces a newer link
	FGSTN_LinkResponseEntry Stale = Response;
	Stale.Serial = 0;
	FGSTN_LinkFragment Free;
	Free.Links.Body.Serial = 1;
	TestFalse(TEXT("an older serial is ignored"), FGSTN_LinkInboxHandlers::ApplyResponse(Free, Stale, 6));

	// The break command applies to the live link of its own serial once
	FGSTN_LinkCommandEntry Break;
	Break.Kind = EGSTN_LinkCommandKind::Break;
	Break.Serial = 1;
	Break.Reason = static_cast<uint8>(EGSTN_LinkBreakReason::Release);
	FGSTN_LinkFragment Breaking = Formed;
	TestTrue(TEXT("the break of the live serial applies"), FGSTN_LinkInboxHandlers::ApplyCommand(Breaking, Break, HitFrame + 30));
	TestTrue(TEXT("the link is gone, its serial kept"), Breaking.Links.Body.Phase == EGSTN_LinkPhase::None && Breaking.Links.Body.Serial == 1);
	TestFalse(TEXT("a repeated break does nothing"), FGSTN_LinkInboxHandlers::ApplyCommand(Breaking, Break, HitFrame + 30));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkFormationLatchTest, "GSTNLink.NP.Formation.LatchParksInitiator", Flags)
bool FGSTN_LinkFormationLatchTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	FScopedLinkTypes Types;

	// The latch is a definition plus data: the same step, the same applier, the same response
	FRequestRig Rig(TestLatch, 8, PredictedOn(Types.LatchIndex, 9), 9, FGSTN_LinkFragment());
	FGSTN_StepResult Out;
	Rig.Run(Out);
	TestTrue(TEXT("the latch is accepted"), Out.bHit && Out.OutgoingEntries.Num() == 1);
	FGSTN_SyncFragmentArray TargetFragments;
	ApplyOutcome(Out, Rig.View, TargetFragments);
	const FGSTN_LinkFragment* Latched = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(TargetFragments);
	TestTrue(TEXT("the latched enemy holds role 2"), Latched && Latched->Links.Body.Role == 2);
	TestFalse(TEXT("and is not parked"), FGSTN_LinkTickExtension::IsParked(Latched, UGSTN_LinkRegistry::Get()));
	TestFalse(TEXT("and fights on"), Latched && FGSTN_LinkTickExtension::IsActionBlocked(*Latched, UGSTN_LinkRegistry::Get()));

	FGSTN_LinkResponseEntry Response;
	const FGSTN_InboxEntry& Reply = Out.OutgoingEntries[0];
	if (const FGSTN_LinkResponseEntry* Payload = Reply.Payload.GetPtr<FGSTN_LinkResponseEntry>())
	{
		Response = *Payload;
	}
	FGSTN_LinkFragment Latcher = PredictedOn(Types.LatchIndex, 9);
	FGSTN_LinkInboxHandlers::ApplyResponse(Latcher, Response, 9);
	TestEqual(TEXT("the latcher holds role 1 at F+2"), static_cast<int32>(Latcher.Links.Body.Role), 1);
	TestTrue(TEXT("the latcher is the PARKED side"), FGSTN_LinkTickExtension::IsParked(&Latcher, UGSTN_LinkRegistry::Get()));
	TestFalse(TEXT("and keeps acting"), FGSTN_LinkTickExtension::IsActionBlocked(Latcher, UGSTN_LinkRegistry::Get()));
	return true;
}

#endif
