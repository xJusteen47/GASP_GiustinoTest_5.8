// Links, Network Prediction half: automation tests of the grab moves (F12a, m6 spec 8, 9 steps 3 and 4): a pummel keeps the link and lands on the
// partner only, a throw is a direct hit on the partner validated on the victim's side alone and ends both sides, the mash moves the escape frame
// the same way on every run, the immunity after a release and after a throw, the grabber's cooldown after a throw, and the grab gate (weight and
// Combat.Part.GrabLocked by name, with and without the parts plugin). No world: the steps, the appliers, the watchdog's pure function and the link
// tick's rules on views, fragments and an input history.

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/GSTN_GrabRules.h"
#include "GSTN_LinkTags.h"
#include "GSTN_LinkWatchdog.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitResolution.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputSample.h"
#include "Pipeline/GSTN_HitStep_GrabImmunity.h"
#include "Pipeline/GSTN_HitStep_LinkBreakOnHit.h"
#include "Pipeline/GSTN_HitStep_LinkRequest.h"
#include "Pipeline/GSTN_LinkHitOutcome.h"
#include "Rules/GSTN_LinkRule_Grab.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Sim/GSTN_LinkMash.h"
#include "Sim/GSTN_LinkThrow.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "State/GSTN_LinkMath.h"

using namespace GSTNLinkNPTest;

namespace
{
	constexpr int32 GrabTestGrabberId = 5;
	constexpr int32 GrabTestGrabbedId = 6;
	constexpr int32 GrabTestOtherId = 9;
	constexpr uint8 GrabTestSerial = 4;
	constexpr int32 GrabTestLinkFrame = 100;

	/** A hit as the victim's pipeline (and the attacker's prediction) sees it: both sides' link extensions of the end of the previous frame */
	struct FHitRig
	{
		FGSTN_LinkSnapshotExt VictimExt;
		FGSTN_LinkSnapshotExt AttackerExt;
		FGSTN_HitContextView View;

		FHitRig(int32 AttackerId, const FGSTN_LinkFragment& Attacker, int32 VictimId, const FGSTN_LinkFragment& Victim, int32 Frame)
		{
			VictimExt.SetFromFragment(Victim);
			AttackerExt.SetFromFragment(Attacker);
			View.Incoming.Frame = Frame;
			View.Incoming.AttackerEntityId = AttackerId;
			View.Incoming.AttackerFacing = FVector::ForwardVector;
			View.Victim.EntityId = VictimId;
			View.Victim.Location = FVector(30.0, 0.0, 0.0);
			Point();
		}

		void Point()
		{
			Ref.TypeId = GSTNFeatureIds::Link;
			Ref.Data = &VictimExt;
			View.Victim.SetSnapshotExtensions(&Ref, 1);
			View.Incoming.AttackerExtensions[0].TypeId = GSTNFeatureIds::Link;
			View.Incoming.AttackerExtensions[0].Data = &AttackerExt;
		}

		EGSTN_StepResult Run(const UGSTN_HitResolutionStep* Step, FGSTN_StepResult& Out) const
		{
			return Step->Resolve_Implementation(View, Out);
		}

		FGSTN_HitVictimView::FSnapshotExtensionRef Ref;
	};

	/** A grab rule on the test grab type, removed with the scope */
	struct FScopedGrabRule
	{
		UGSTN_LinkTypeDefinition* Definition = nullptr;
		UGSTN_LinkRule_Grab* Rule = nullptr;

		explicit FScopedGrabRule(UGSTN_LinkTypeDefinition* InDefinition)
			: Definition(InDefinition)
		{
			Rule = NewObject<UGSTN_LinkRule_Grab>(Definition);
			Definition->Rules.Add(Rule);
			FGSTN_LinkRequestTags::Invalidate();
		}

		~FScopedGrabRule()
		{
			Definition->Rules.Remove(Rule);
			FGSTN_LinkRequestTags::Invalidate();
		}
	};

	void ApplyAll(const FGSTN_StepResult& Out, const FGSTN_HitContextView& View, FGSTN_SyncFragmentArray& Fragments)
	{
		FGSTN_OutcomeApplyContext Context;
		Context.VictimFragments = &Fragments;
		FGSTN_LinkOutcomeApplier Formation;
		FGSTN_LinkThrowOutcomeApplier Throw;
		FGSTN_LinkBreakOutcomeApplier Break;
		for (const FInstancedStruct& Extension : Out.OutcomeExtensions)
		{
			for (const IGSTN_OutcomeExtensionApplier* Applier : { static_cast<const IGSTN_OutcomeExtensionApplier*>(&Formation), static_cast<const IGSTN_OutcomeExtensionApplier*>(&Throw),
				static_cast<const IGSTN_OutcomeExtensionApplier*>(&Break) })
			{
				if (Extension.GetScriptStruct() == Applier->GetExtensionStruct())
				{
					Applier->Apply(Extension, View, Context);
				}
			}
		}
	}

	bool HasOutcome(const FGSTN_StepResult& Out, const UScriptStruct* Struct)
	{
		return Out.OutcomeExtensions.ContainsByPredicate([Struct](const FInstancedStruct& Extension) { return Extension.GetScriptStruct() == Struct; });
	}

	/** One frame of the history: the attack button (bit 0) pressed on it, released the frame after */
	void AppendFrame(FGSTN_InputHistory& History, int32 Frame, bool bPress)
	{
		const bool bWasHeld = (History.GetHeldButtons() & 1u) != 0;
		FGSTN_InputSample Sample;
		Sample.Frame = Frame;
		Sample.Held = bPress ? 1u : 0u;
		Sample.Pressed = bPress && !bWasHeld ? 1u : 0u;
		Sample.Released = !bPress && bWasHeld ? 1u : 0u;
		History.Append(Sample);
	}

	FGSTN_LinkOutcomeKey MakeThrowKey()
	{
		FGSTN_LinkOutcomeKey Key;
		Key.Frame = 8;
		Key.Outcome = EGSTN_LinkOutcome::Throw;
		Key.ThrowHit.Damage = 8.f;
		Key.ThrowHit.BaseKnockback = 900.f;
		Key.ThrowHit.bParryable = false;
		return Key;
	}

	/** The thrower's and the thrown entity's step snapshot entries */
	void MakeThrowEntries(const FGSTN_LinkFragment& Grabber, const FGSTN_LinkFragment& Grabbed, FGSTN_SnapshotEntry& OutSelf, FGSTN_SnapshotEntry& OutPartner)
	{
		OutSelf = MakeEntry(GrabTestGrabberId, FTransform(FVector::ZeroVector), 0, &Grabber);
		OutPartner = MakeEntry(GrabTestGrabbedId, FTransform(FVector(30.0, 0.0, 0.0)), 0, &Grabbed);
	}

	/** The victim's view of a direct throw entry */
	FHitRig RigForEntry(const FGSTN_HitInboxEntry& Entry, const FGSTN_LinkFragment& Attacker, int32 VictimId, const FGSTN_LinkFragment& Victim)
	{
		FHitRig Rig(Entry.AttackerId, Attacker, VictimId, Victim, Entry.Frame);
		Rig.View.Incoming.Hit = Entry.Hit;
		Rig.View.Incoming.AttackerFacing = FRotator(0.0, Entry.AttackerYaw, 0.0).Vector();
		return Rig;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPGrabPummelTest, "GSTNLink.NP.Grab.PummelKeepsLink", Flags)
bool FGSTN_LinkNPGrabPummelTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FScopedGrabRule Grab(Types.Grab);
	Grab.Rule->Rules.PummelFreeFrames = 15;
	const FGSTN_LinkFragment Grabber = MakeLinked(Types.GrabIndex, 1, GrabTestGrabbedId, GrabTestSerial, GrabTestLinkFrame + 1);
	const FGSTN_LinkFragment Grabbed = MakeLinked(Types.GrabIndex, 2, GrabTestGrabberId, GrabTestSerial, GrabTestLinkFrame);

	// A pummel after the free frames: lands, with the reaction scaled away, and the link does not break on it
	FHitRig Rig(GrabTestGrabberId, Grabber, GrabTestGrabbedId, Grabbed, GrabTestLinkFrame + 20);
	Rig.View.Incoming.Hit.HitType = GSTNLinkTags::HitType_Pummel;
	Rig.View.Incoming.Hit.LinkFilter = EGSTN_LinkFilter::LinkedPartnerOnly;
	Rig.View.Incoming.Hit.Damage = 3.f;
	FGSTN_StepResult Out;
	TestEqual(TEXT("the pummel continues to the damage steps"), Rig.Run(GetDefault<UGSTN_HitStep_LinkRequest>(), Out), EGSTN_StepResult::Continue);
	TestTrue(TEXT("it lands"), Out.bHit);
	TestFalse(TEXT("it does damage"), Out.bNoDamage);
	TestTrue(TEXT("no knockback, no hitstun, no flinch"), Out.KnockbackScale == 0.f && Out.HitstunScale == 0.f && Out.bNoFlinch);
	FGSTN_StepResult BreakOut;
	Rig.Run(GetDefault<UGSTN_HitStep_LinkBreakOnHit>(), BreakOut);
	TestEqual(TEXT("the partner's pummel does not mark a break"), BreakOut.OutcomeExtensions.Num(), 0);
	FGSTN_SyncFragmentArray Fragments = ListOf(Grabbed);
	ApplyAll(BreakOut, Rig.View, Fragments);
	const FGSTN_LinkFragment* After = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Fragments);
	TestTrue(TEXT("the grabbed link is still active, with no pending break"), After && After->Links.Body.IsActive() && !After->Links.Body.HasFlag(GSTNLinkFlags::PendingBreak));

	// Inside the free frames the pummel whiffs (no instant mash)
	FHitRig Early(GrabTestGrabberId, Grabber, GrabTestGrabbedId, Grabbed, GrabTestLinkFrame + 5);
	Early.View.Incoming.Hit = Rig.View.Incoming.Hit;
	FGSTN_StepResult EarlyOut;
	TestEqual(TEXT("an early pummel stops"), Early.Run(GetDefault<UGSTN_HitStep_LinkRequest>(), EarlyOut), EGSTN_StepResult::Stop);
	TestFalse(TEXT("and does not land"), EarlyOut.bHit);

	// The same hitbox on a third entity: the link filter lets it land on the partner only
	FHitRig Other(GrabTestGrabberId, Grabber, GrabTestOtherId, FGSTN_LinkFragment(), GrabTestLinkFrame + 20);
	Other.View.Incoming.Hit = Rig.View.Incoming.Hit;
	FGSTN_StepResult OtherOut;
	TestEqual(TEXT("a third entity is filtered"), Other.Run(GetDefault<UGSTN_HitStep_LinkRequest>(), OtherOut), EGSTN_StepResult::Stop);
	TestFalse(TEXT("and never hit"), OtherOut.bHit);

	// An ordinary attack that excludes the partner never lands on it, and lands on the third entity
	FHitRig Excluded(GrabTestGrabberId, Grabber, GrabTestGrabbedId, Grabbed, GrabTestLinkFrame + 20);
	Excluded.View.Incoming.Hit.LinkFilter = EGSTN_LinkFilter::ExcludeLinkedPartner;
	FGSTN_StepResult ExcludedOut;
	Excluded.Run(GetDefault<UGSTN_HitStep_LinkRequest>(), ExcludedOut);
	TestFalse(TEXT("ExcludeLinkedPartner spares the partner"), ExcludedOut.bHit);
	FHitRig ExcludedOther(GrabTestGrabberId, Grabber, GrabTestOtherId, FGSTN_LinkFragment(), GrabTestLinkFrame + 20);
	ExcludedOther.View.Incoming.Hit.LinkFilter = EGSTN_LinkFilter::ExcludeLinkedPartner;
	FGSTN_StepResult ExcludedOtherOut;
	TestEqual(TEXT("and hits anyone else"), ExcludedOther.Run(GetDefault<UGSTN_HitStep_LinkRequest>(), ExcludedOtherOut), EGSTN_StepResult::Continue);
	TestTrue(TEXT("which lands"), ExcludedOtherOut.bHit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPGrabThrowTest, "GSTNLink.NP.Grab.ThrowDirectHitNoOverlap", Flags)
bool FGSTN_LinkNPGrabThrowTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FScopedGrabRule Grab(Types.Grab);
	FGSTN_LinkFragment Grabber = MakeLinked(Types.GrabIndex, 1, GrabTestGrabbedId, GrabTestSerial, GrabTestLinkFrame + 1);
	const FGSTN_LinkFragment Grabbed = MakeLinked(Types.GrabIndex, 2, GrabTestGrabberId, GrabTestSerial, GrabTestLinkFrame);
	constexpr int32 KeyFrame = 140;

	// The key: the entry goes straight to the partner (no hitbox), and the grabber's own link ends on the key's frame
	FGSTN_SnapshotEntry Self;
	FGSTN_SnapshotEntry Partner;
	MakeThrowEntries(Grabber, Grabbed, Self, Partner);
	const FGSTN_HitInboxEntry Entry = FGSTN_LinkThrow::MakeThrowEntry(MakeThrowKey(), KeyFrame, Self, Partner, GrabTestSerial, Types.GrabIndex, nullptr);
	TestEqual(TEXT("stamped with the key's frame"), Entry.Frame, KeyFrame);
	TestEqual(TEXT("from the grabber"), Entry.AttackerId, GrabTestGrabberId);
	TestTrue(TEXT("a throw hit"), Entry.Hit.HitType.MatchesTagExact(GSTNLinkTags::HitType_Throw));
	const FGSTN_LinkPartnerHit* Partnered = nullptr;
	for (const FInstancedStruct& Extension : Entry.Hit.ExtensionData)
	{
		Partnered = Partnered ? Partnered : Extension.GetPtr<FGSTN_LinkPartnerHit>();
	}
	TestTrue(TEXT("carrying the link's serial and type"), Partnered && Partnered->Serial == GrabTestSerial && Partnered->LinkTypeIndex == Types.GrabIndex);
	TestTrue(TEXT("the grabber's link ends at the key"), FGSTN_LinkThrow::EndOwnLink(Grabber, KeyFrame, &Grab.Rule->Rules));
	TestTrue(TEXT("idle, reason Throw, the serial kept"), Grabber.Links.Body.Phase == EGSTN_LinkPhase::None
		&& Grabber.Links.Body.GetBreakReason() == EGSTN_LinkBreakReason::Throw && Grabber.Links.Body.Serial == GrabTestSerial);

	// F+1 on the thrown side: valid on the victim's side alone (the grabber's side is already idle in the snapshot)
	FHitRig Rig = RigForEntry(Entry, Grabber, GrabTestGrabbedId, Grabbed);
	Rig.Point();
	FGSTN_StepResult Out;
	TestEqual(TEXT("the throw goes on to the knockback steps"), Rig.Run(GetDefault<UGSTN_HitStep_LinkRequest>(), Out), EGSTN_StepResult::Continue);
	TestTrue(TEXT("it lands"), Out.bHit);
	TestTrue(TEXT("with its knockback (no scaling)"), Out.KnockbackScale == 1.f);
	TestTrue(TEXT("and the throw outcome for the thrown side"), HasOutcome(Out, FGSTN_LinkThrowOutcome::StaticStruct()));
	FGSTN_SyncFragmentArray Fragments = ListOf(Grabbed);
	ApplyAll(Out, Rig.View, Fragments);
	const FGSTN_LinkFragment* Thrown = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Fragments);
	TestTrue(TEXT("the thrown side's link ends with reason Throw"), Thrown && Thrown->Links.Body.Phase == EGSTN_LinkPhase::None
		&& Thrown->Links.Body.GetBreakReason() == EGSTN_LinkBreakReason::Throw);
	TestTrue(TEXT("its exit teleports it on F+1 at the socket placement it was shown at"), Thrown && Thrown->Actions.Exit.Frame == KeyFrame + 1
		&& (Thrown->Actions.Exit.Flags & GSTNLinkExitFlags::Teleport) != 0 && Thrown->Actions.Exit.Position.Equals(FVector(30.0, 0.0, 0.0), 0.01));

	// The same entry for another serial, or on a third entity, does not land: no overlap ever chose the victim, the link does
	FGSTN_HitInboxEntry Stale = Entry;
	for (FInstancedStruct& Extension : Stale.Hit.ExtensionData)
	{
		if (FGSTN_LinkPartnerHit* Hit = Extension.GetMutablePtr<FGSTN_LinkPartnerHit>())
		{
			Hit->Serial = GrabTestSerial + 1;
		}
	}
	FHitRig StaleRig = RigForEntry(Stale, Grabber, GrabTestGrabbedId, Grabbed);
	StaleRig.Point();
	FGSTN_StepResult StaleOut;
	StaleRig.Run(GetDefault<UGSTN_HitStep_LinkRequest>(), StaleOut);
	TestFalse(TEXT("a throw of another serial is refused"), StaleOut.bHit);
	FHitRig ThirdRig = RigForEntry(Entry, Grabber, GrabTestOtherId, FGSTN_LinkFragment());
	ThirdRig.Point();
	FGSTN_StepResult ThirdOut;
	ThirdRig.Run(GetDefault<UGSTN_HitStep_LinkRequest>(), ThirdOut);
	TestFalse(TEXT("a throw on an entity that is not the partner is refused"), ThirdOut.bHit);

	// The watchdog leaves the thrown side to the simulation while the thrower shows the throw
	TArray<FGSTN_LinkWatchdogEntity> Entities;
	FGSTN_LinkWatchdogEntity A;
	A.EntityId = GrabTestGrabberId;
	A.Links = Grabber.Links;
	FGSTN_LinkWatchdogEntity B;
	B.EntityId = GrabTestGrabbedId;
	B.Links = Grabbed.Links;
	Entities = { A, B };
	FGSTN_LinkWatchdogLogic::FParams Params;
	Params.OrphanGraceFrames = 3;
	Params.Definitions = [](uint8 Index) { return UGSTN_LinkRegistry::Get()->GetDefinition(Index); };
	TArray<FGSTN_LinkWatchdogPost> Posts;
	TArray<FGSTN_LinkWatchdogBreak> Breaks;
	FGSTN_LinkWatchdogLogic::Evaluate(Entities, KeyFrame, Params, Posts, Breaks);
	TestEqual(TEXT("no orphan break races the throw"), Posts.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPGrabThrowDefenceTest, "GSTNLink.NP.Grab.ThrowSkipsParryShield", Flags)
bool FGSTN_LinkNPGrabThrowDefenceTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	const FGSTN_LinkFragment Grabber = MakeLinked(Types.GrabIndex, 1, GrabTestGrabbedId, GrabTestSerial, GrabTestLinkFrame + 1);
	const FGSTN_LinkFragment Grabbed = MakeLinked(Types.GrabIndex, 2, GrabTestGrabberId, GrabTestSerial, GrabTestLinkFrame);
	FGSTN_SnapshotEntry Self;
	FGSTN_SnapshotEntry Partner;
	MakeThrowEntries(Grabber, Grabbed, Self, Partner);
	const FGSTN_HitInboxEntry Entry = FGSTN_LinkThrow::MakeThrowEntry(MakeThrowKey(), 140, Self, Partner, GrabTestSerial, Types.GrabIndex, nullptr);

	// What the defence steps read to stay out (by name, the defence plugin may not depend on links): a grab-like hit type, not parryable, and the
	// body hurtbox, never an extra one such as the shield bubble: a direct entry chose no hurtbox by overlap
	const FGameplayTag ThrowByName = FGameplayTag::RequestGameplayTag(TEXT("Combat.HitType.Throw"), false);
	TestTrue(TEXT("the throw is grab-like by the name the defence reads"), ThrowByName.IsValid() && Entry.Hit.HitType.MatchesTag(ThrowByName));
	TestFalse(TEXT("not parryable"), Entry.Hit.bParryable);
	TestEqual(TEXT("on the body hurtbox, not the bubble"), static_cast<int32>(Entry.HurtboxIndex), 0);

	// With the defence plugin loaded, its steps run on the throw and stop nothing (found by name: no module dependency)
	FHitRig Rig = RigForEntry(Entry, Grabber, GrabTestGrabbedId, Grabbed);
	Rig.Point();
	int32 Ran = 0;
	for (const TCHAR* Path : { TEXT("/Script/GSTNDefenceNP.GSTN_HitStep_Parry"), TEXT("/Script/GSTNDefenceNP.GSTN_HitStep_Shield") })
	{
		const UClass* Class = FindObject<UClass>(nullptr, Path);
		if (!Class)
		{
			continue;
		}
		++Ran;
		FGSTN_StepResult Out;
		const EGSTN_StepResult Result = CastChecked<UGSTN_HitResolutionStep>(Class->GetDefaultObject())->Resolve_Implementation(Rig.View, Out);
		TestEqual(FString::Printf(TEXT("%s continues"), Path), Result, EGSTN_StepResult::Continue);
		TestTrue(FString::Printf(TEXT("%s neither parries nor blocks"), Path), Out.bHit && !Out.bParried && !Out.bBlocked);
	}
	if (Ran == 0)
	{
		AddInfo(TEXT("GSTNDefence is not loaded: the defence steps are not in this pipeline at all"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPGrabMashTest, "GSTNLink.NP.Grab.MashEscapeFrameDeterministic", Flags)
bool FGSTN_LinkNPGrabMashTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FScopedGrabRule Grab(Types.Grab);
	const FGSTN_MashRule& Rule = Grab.Rule->Rules.Mash;

	// Formation: the step decides the escape frame from the rule and the grabbed side's damage
	FHitRig Request(GrabTestGrabberId, FGSTN_LinkFragment(), GrabTestGrabbedId, FGSTN_LinkFragment(), GrabTestLinkFrame - 1);
	Request.View.Incoming.Hit.HitType = GSTNCombatHitTags::HitType_Grab;
	Request.View.Incoming.Hit.LinkType = TestGrab;
	FGSTN_StepResult Formed;
	Request.Run(GetDefault<UGSTN_HitStep_LinkRequest>(), Formed);
	FGSTN_SyncFragmentArray Fragments;
	ApplyAll(Formed, Request.View, Fragments);
	FGSTN_LinkFragment* Grabbed = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Fragments);
	if (!TestNotNull(TEXT("formed"), Grabbed))
	{
		return false;
	}
	const int32 Initial = FGSTN_LinkMash::InitialEndFrame(Rule, GrabTestLinkFrame, 0.f);
	TestEqual(TEXT("the escape frame from the formation on"), Grabbed->Links.Body.EndFrame, Initial);

	// Three presses while linked (and one before the link, which is the past)
	FGSTN_InputHistory History;
	for (int32 Frame = GrabTestLinkFrame - 4; Frame <= GrabTestLinkFrame + 30; ++Frame)
	{
		AppendFrame(History, Frame, Frame == GrabTestLinkFrame - 2 || Frame == GrabTestLinkFrame + 10 || Frame == GrabTestLinkFrame + 15 || Frame == GrabTestLinkFrame + 20);
	}
	FGSTN_LinkFragment First = *Grabbed;
	TestTrue(TEXT("the mash counts"), FGSTN_LinkMash::Step(First, History, Rule, MAX_uint32, GrabTestLinkFrame + 31));
	TestEqual(TEXT("three presses, ReducePerPress each"), First.Links.Body.EndFrame, Initial - 3 * Rule.ReducePerPress);
	TestEqual(TEXT("up to the history's latest frame"), First.MashProcessedFrame, GrabTestLinkFrame + 30);

	// The same frame simulated again (a re-simulation) and an independent run give the same frame: nothing counts twice
	FGSTN_LinkFragment Again = First;
	FGSTN_LinkMash::Step(Again, History, Rule, MAX_uint32, GrabTestLinkFrame + 31);
	TestEqual(TEXT("a repeated tick counts nothing"), Again.Links.Body.EndFrame, First.Links.Body.EndFrame);
	FGSTN_LinkFragment Other = *Grabbed;
	FGSTN_LinkMash::Step(Other, History, Rule, MAX_uint32, GrabTestLinkFrame + 31);
	TestTrue(TEXT("a second machine lands on the same state"), Other == First);

	// The watchdog's break rule escapes exactly on that frame, not one before
	FGSTN_LinkBreakInput Input;
	Input.LinkFrame = GrabTestLinkFrame;
	Input.EndFrame = First.Links.Body.EndFrame;
	Input.Frame = First.Links.Body.EndFrame - 1;
	TestFalse(TEXT("no escape one frame before"), UGSTN_LinkMath::EvaluateBreak(Types.Grab->Break, Types.Grab->MaxFrames, Input).bBreak);
	Input.Frame = First.Links.Body.EndFrame;
	const FGSTN_LinkBreakDecision Escape = UGSTN_LinkMath::EvaluateBreak(Types.Grab->Break, Types.Grab->MaxFrames, Input);
	TestTrue(TEXT("escape on the predicted frame"), Escape.bBreak && Escape.Reason == EGSTN_LinkBreakReason::Escape);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPGrabImmunityTest, "GSTNLink.NP.Grab.ImmunityAfterReleaseAndThrow", Flags)
bool FGSTN_LinkNPGrabImmunityTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FScopedGrabRule Grab(Types.Grab);
	const FGSTN_GrabRules& Rules = Grab.Rule->Rules;

	// A release: the grabber's release key marks PendingBreak(Release); the watchdog gives the grabbed its breather through the break command
	FGSTN_LinkFragment Grabber = MakeLinked(Types.GrabIndex, 1, GrabTestGrabbedId, GrabTestSerial, GrabTestLinkFrame + 1);
	FGSTN_LinkFragment Grabbed = MakeLinked(Types.GrabIndex, 2, GrabTestGrabberId, GrabTestSerial, GrabTestLinkFrame);
	TestTrue(TEXT("release marks the pending break"), FGSTN_LinkTickExtension::MarkPendingBreak(Grabber, static_cast<uint8>(EGSTN_LinkBreakReason::Release)));
	FGSTN_LinkWatchdogEntity A;
	A.EntityId = GrabTestGrabberId;
	A.Links = Grabber.Links;
	FGSTN_LinkWatchdogEntity B;
	B.EntityId = GrabTestGrabbedId;
	B.Links = Grabbed.Links;
	B.Transform = FTransform(FVector(30.0, 0.0, 0.0));
	FGSTN_LinkWatchdogLogic::FParams Params;
	Params.Definitions = [](uint8 Index) { return UGSTN_LinkRegistry::Get()->GetDefinition(Index); };
	TArray<FGSTN_LinkWatchdogPost> Posts;
	TArray<FGSTN_LinkWatchdogBreak> Breaks;
	constexpr int32 ReleaseFrame = 200;
	FGSTN_LinkWatchdogLogic::Evaluate({ A, B }, ReleaseFrame, Params, Posts, Breaks);
	const FGSTN_LinkWatchdogPost* ToGrabbed = Posts.FindByPredicate([](const FGSTN_LinkWatchdogPost& Post) { return Post.TargetId == GrabTestGrabbedId; });
	TestTrue(TEXT("a release break, both sides"), Posts.Num() == 2 && Breaks.Num() == 1 && Breaks[0].Reason == EGSTN_LinkBreakReason::Release);
	TestTrue(TEXT("with the release breather for the grabbed"), ToGrabbed && ToGrabbed->Command.ImmunityEndFrame == ReleaseFrame + 1 + Rules.ImmunityFramesAfterRelease);
	if (ToGrabbed)
	{
		FGSTN_LinkInboxHandlers::ApplyCommand(Grabbed, ToGrabbed->Command, ReleaseFrame);
	}
	TestEqual(TEXT("the grabbed carries it"), Grabbed.GrabImmunityEndFrame, ReleaseFrame + 1 + Rules.ImmunityFramesAfterRelease);

	// A regrab inside the breather whiffs (no registration), one at its end connects
	auto Regrab = [](const FGSTN_LinkFragment& Victim, int32 Frame)
	{
		FHitRig Rig(GrabTestGrabberId, FGSTN_LinkFragment(), GrabTestGrabbedId, Victim, Frame);
		Rig.View.Incoming.Hit.HitType = GSTNCombatHitTags::HitType_Grab;
		Rig.View.Incoming.Hit.LinkType = TestGrab;
		FGSTN_StepResult Out;
		return Rig.Run(GetDefault<UGSTN_HitStep_GrabImmunity>(), Out) == EGSTN_StepResult::Continue && Out.bHit;
	};
	TestFalse(TEXT("immune inside the breather"), Regrab(Grabbed, Grabbed.GrabImmunityEndFrame - 1));
	TestTrue(TEXT("grabbable at its end"), Regrab(Grabbed, Grabbed.GrabImmunityEndFrame));

	// A throw: the thrown side's breather is the AfterThrow one, from the throw's outcome
	FGSTN_LinkFragment Thrower = MakeLinked(Types.GrabIndex, 1, GrabTestGrabbedId, GrabTestSerial, GrabTestLinkFrame + 1);
	const FGSTN_LinkFragment Held = MakeLinked(Types.GrabIndex, 2, GrabTestGrabberId, GrabTestSerial, GrabTestLinkFrame);
	FGSTN_SnapshotEntry Self;
	FGSTN_SnapshotEntry Partner;
	MakeThrowEntries(Thrower, Held, Self, Partner);
	constexpr int32 ThrowFrame = 300;
	const FGSTN_HitInboxEntry Entry = FGSTN_LinkThrow::MakeThrowEntry(MakeThrowKey(), ThrowFrame, Self, Partner, GrabTestSerial, Types.GrabIndex, nullptr);
	FGSTN_LinkThrow::EndOwnLink(Thrower, ThrowFrame, &Rules);
	FHitRig Rig = RigForEntry(Entry, Thrower, GrabTestGrabbedId, Held);
	Rig.Point();
	FGSTN_StepResult Out;
	Rig.Run(GetDefault<UGSTN_HitStep_LinkRequest>(), Out);
	FGSTN_SyncFragmentArray Fragments = ListOf(Held);
	ApplyAll(Out, Rig.View, Fragments);
	const FGSTN_LinkFragment* Thrown = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Fragments);
	TestTrue(TEXT("the thrown side gets the AfterThrow breather"), Thrown && Thrown->GrabImmunityEndFrame == ThrowFrame + 1 + Rules.ImmunityFramesAfterThrow);
	TestTrue(TEXT("longer than the release one"), Rules.ImmunityFramesAfterThrow > Rules.ImmunityFramesAfterRelease);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPGrabCooldownTest, "GSTNLink.NP.Grab.GrabberCooldown", Flags)
bool FGSTN_LinkNPGrabCooldownTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FScopedGrabRule Grab(Types.Grab);
	const FGSTN_GrabRules& Rules = Grab.Rule->Rules;
	constexpr int32 ThrowFrame = 200;
	FGSTN_LinkFragment Thrower = MakeLinked(Types.GrabIndex, 1, GrabTestGrabbedId, GrabTestSerial, GrabTestLinkFrame + 1);
	FGSTN_LinkThrow::EndOwnLink(Thrower, ThrowFrame, &Rules);
	TestEqual(TEXT("the cooldown runs from the throw"), Thrower.Actions.CooldownEndFrame, ThrowFrame + Rules.GrabberCooldownFrames);
	FGSTN_LinkSnapshotExt Ext;
	Ext.SetFromFragment(Thrower);
	TestEqual(TEXT("the snapshot carries it"), Ext.CooldownEndFrame, Thrower.Actions.CooldownEndFrame);

	auto Grabs = [&Thrower](int32 Frame)
	{
		FHitRig Rig(GrabTestGrabberId, Thrower, GrabTestOtherId, FGSTN_LinkFragment(), Frame);
		Rig.View.Incoming.Hit.HitType = GSTNCombatHitTags::HitType_Grab;
		Rig.View.Incoming.Hit.LinkType = TestGrab;
		FGSTN_StepResult Out;
		Rig.Run(GetDefault<UGSTN_HitStep_LinkRequest>(), Out);
		return Out.bHit && HasOutcome(Out, FGSTN_LinkHitOutcome::StaticStruct());
	};
	TestFalse(TEXT("no new grab inside the cooldown (another target)"), Grabs(ThrowFrame + Rules.GrabberCooldownFrames - 1));
	TestTrue(TEXT("a grab at its end"), Grabs(ThrowFrame + Rules.GrabberCooldownFrames));

	// The common checks name the reason
	FGSTN_LinkRequestContext Context;
	Context.Frame = ThrowFrame + 1;
	Context.Definition = Types.Grab;
	Context.Initiator.EntityId = GrabTestGrabberId;
	Context.Initiator.CooldownEndFrame = Thrower.Actions.CooldownEndFrame;
	Context.Target.EntityId = GrabTestOtherId;
	TestTrue(TEXT("refused for the cooldown"), UGSTN_LinkRule::CheckCommon(Context).Refusal == EGSTN_LinkRefusal::Cooldown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPGrabWeightTest, "GSTNLink.NP.Grab.WeightGate", Flags)
bool FGSTN_LinkNPGrabWeightTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FScopedGrabRule Grab(Types.Grab);
	Grab.Rule->Rules.MaxGrabWeight = 120.f;
	auto Grabs = [](uint8 WeightScaleQ)
	{
		FHitRig Rig(GrabTestGrabberId, FGSTN_LinkFragment(), GrabTestGrabbedId, FGSTN_LinkFragment(), GrabTestLinkFrame);
		Rig.View.Incoming.Hit.HitType = GSTNCombatHitTags::HitType_Grab;
		Rig.View.Incoming.Hit.LinkType = TestGrab;
		Rig.View.Victim.ReactionState.WeightScaleQ = WeightScaleQ;
		FGSTN_StepResult Out;
		Rig.Run(GetDefault<UGSTN_HitStep_LinkRequest>(), Out);
		return Out.bHit && HasOutcome(Out, FGSTN_LinkHitOutcome::StaticStruct());
	};
	TestTrue(TEXT("a standard fighter (weight 100) is grabbed"), Grabs(64));
	TestTrue(TEXT("one under the limit (118.75) is grabbed"), Grabs(76));
	TestFalse(TEXT("a heavy one (150) is refused"), Grabs(96));
	// The same weight that the interact probe's pre-filter reads: the provider and the step cannot disagree
	TestEqual(TEXT("100 x the snapshot weight scale"), 100.f * 96.f / 64.f, 150.f);
	return true;
}

namespace
{
	/** A grab request on a target that carries Tags (as the snapshot's request tag mask carries them) */
	bool GrabsTargetWith(const FGameplayTagContainer& Tags)
	{
		FHitRig Rig(GrabTestGrabberId, FGSTN_LinkFragment(), GrabTestGrabbedId, FGSTN_LinkFragment(), GrabTestLinkFrame);
		Rig.VictimExt.RequestTagMask = FGSTN_LinkRequestTags::MakeMask(Tags);
		Rig.Point();
		Rig.View.Incoming.Hit.HitType = GSTNCombatHitTags::HitType_Grab;
		Rig.View.Incoming.Hit.LinkType = TestGrab;
		FGSTN_StepResult Out;
		Rig.Run(GetDefault<UGSTN_HitStep_LinkRequest>(), Out);
		return Out.bHit && HasOutcome(Out, FGSTN_LinkHitOutcome::StaticStruct());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPGrabLockedWithPartsTest, "GSTNLink.NP.Grab.GrabLockedByNameWithParts", Flags)
bool FGSTN_LinkNPGrabLockedWithPartsTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Locked = FGameplayTag::RequestGameplayTag(TEXT("Combat.Part.GrabLocked"), false);
	if (!Locked.IsValid())
	{
		AddInfo(TEXT("GSTNParts is not loaded: Combat.Part.GrabLocked does not exist (GrabLockedAbsentWithoutParts covers this build)"));
		return true;
	}
	FScopedLinkTypes Types;
	FScopedGrabRule Grab(Types.Grab);
	TestTrue(TEXT("the request tag table holds the tag, found by name"), FGSTN_LinkRequestTags::Get().Contains(Locked));
	TestFalse(TEXT("a target whose parts gate is unmet (GrabLocked) is refused"), GrabsTargetWith(FGameplayTagContainer(Locked)));
	TestTrue(TEXT("the same target once its parts broke (the tag gone) is grabbed"), GrabsTargetWith(FGameplayTagContainer()));
	Grab.Rule->Rules.bRespectGrabLocked = false;
	TestTrue(TEXT("a rule that ignores the gate grabs it anyway"), GrabsTargetWith(FGameplayTagContainer(Locked)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPGrabLockedWithoutPartsTest, "GSTNLink.NP.Grab.GrabLockedAbsentWithoutParts", Flags)
bool FGSTN_LinkNPGrabLockedWithoutPartsTest::RunTest(const FString& Parameters)
{
	FScopedLinkTypes Types;
	FScopedGrabRule Grab(Types.Grab);
	const FGameplayTag Locked = FGameplayTag::RequestGameplayTag(TEXT("Combat.Part.GrabLocked"), false);
	if (Locked.IsValid())
	{
		AddInfo(TEXT("GSTNParts is loaded: the parts-off run checks the absent tag"));
		TestTrue(TEXT("a target without the tag is grabbed"), GrabsTargetWith(FGameplayTagContainer()));
		return true;
	}
	const bool bInTable = FGSTN_LinkRequestTags::Get().ContainsByPredicate([](const FGameplayTag& Tag) { return Tag.GetTagName() == TEXT("Combat.Part.GrabLocked"); });
	TestFalse(TEXT("no GrabLocked in the request tag table"), bInTable);
	TestTrue(TEXT("the rule skips the name it cannot resolve: the weight decides alone"), GrabsTargetWith(FGameplayTagContainer()));
	return true;
}

#endif
