// Test only (M6 F14, m6 spec 12 item 7): a link type added from this module, outside GSTNLink, through the public API only. It registers with
// the registry, forms through GSTNLink's own LinkRequest step, outcome applier and inbox kind, parks its target, refuses by its own rule,
// breaks by its own break rules and costs the same bytes as any link: nothing in GSTNLink or in the combat model's tick was changed for it.

#include "GSTN_ExtTestCarryRule.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_LinkRegistry.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitResolution.h"
#include "Misc/AutomationTest.h"
#include "Pipeline/GSTN_HitStep_LinkRequest.h"
#include "Pipeline/GSTN_LinkHitOutcome.h"
#include "Serialization/MemoryWriter.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Sim/GSTN_LinkSnapshotExt.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "Simulation/GSTN_CombatInbox.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "State/GSTN_LinkMath.h"
#include "Sync/GSTN_SyncFragment.h"
#include "UObject/Package.h"

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr int32 HitFrame = 300;
	constexpr int32 CarrierId = 11;
	constexpr int32 CarriedId = 12;

	/** The carry as a plugin's content would define it: data, its tags, its rule. Registered for the scope, removed after */
	struct FScopedCarry
	{
		UGSTN_LinkTypeDefinition* Definition = nullptr;
		uint8 Index = 0;
		int32 NumBefore = 0;

		FScopedCarry()
		{
			Definition = NewObject<UGSTN_LinkTypeDefinition>(GetTransientPackage());
			Definition->AddToRoot();
			Definition->LinkType = GSTNExtTestLinkTags::Type_Carry;
			Definition->InitiatorRole.RoleTag = GSTNExtTestLinkTags::Role_Carrier;
			Definition->InitiatorRole.bActsWhileLinked = true;
			Definition->TargetRole.RoleTag = GSTNExtTestLinkTags::Role_Carried;
			Definition->TargetRole.bParked = true;
			Definition->TargetRole.SocketOnPartner = TEXT("spine_03");
			Definition->TargetRole.SocketOffset = FTransform(FVector(0.0, 0.0, 40.0));
			Definition->Break.MaxDistanceCm = 50.f;
			Definition->Rules.Add(NewObject<UGSTN_ExtTestCarryRule>(Definition));

			UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
			NumBefore = Registry->Num();
			Registry->AddDefinition(Definition);
			Index = Registry->GetTypeIndex(GSTNExtTestLinkTags::Type_Carry);
		}

		~FScopedCarry()
		{
			UGSTN_LinkRegistry::Get()->RemoveDefinition(Definition);
			Definition->RemoveFromRoot();
		}
	};

	/** A carry request hit of frame F as the target's pipeline sees it: both snapshot extensions of F-1 and the victim's weight */
	struct FCarryRequest
	{
		FGSTN_LinkSnapshotExt TargetExt;
		FGSTN_LinkSnapshotExt InitiatorExt;
		FGSTN_HitContextView View;

		FCarryRequest(const FGSTN_LinkFragment& Initiator, uint8 WeightScaleQ)
		{
			InitiatorExt.SetFromFragment(Initiator);
			TargetExt.SetFromFragment(FGSTN_LinkFragment());
			View.Incoming.Hit.HitType = GSTNCombatHitTags::HitType_Grab;
			View.Incoming.Hit.LinkType = GSTNExtTestLinkTags::Type_Carry;
			View.Incoming.Frame = HitFrame;
			View.Incoming.AttackerEntityId = CarrierId;
			View.Victim.EntityId = CarriedId;
			View.Victim.ReactionState.WeightScaleQ = WeightScaleQ;   // 64 = weight 100
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

	FGSTN_LinkFragment PredictedCarry()
	{
		FGSTN_LinkFragment Fragment;
		FGSTN_LinkTickExtension::MarkPredicted(Fragment, *UGSTN_LinkRegistry::Get(), GSTNExtTestLinkTags::Type_Carry, CarriedId, HitFrame);
		return Fragment;
	}

	int32 PayloadBytes(const FGSTN_LinkFragment& Fragment)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		const_cast<FGSTN_LinkFragment&>(Fragment).NetSerialize(Writer);
		return Bytes.Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExtTestLinkRegistersTest, "GSTNExtTest.Link.NewTypeRegisters", Flags)
bool FGSTN_ExtTestLinkRegistersTest::RunTest(const FString& Parameters)
{
	FScopedCarry Carry;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	TestTrue(TEXT("the registry gave the new type a wire index"), Carry.Index != 0);
	TestEqual(TEXT("one more type than before"), Registry->Num(), Carry.NumBefore + 1);
	TestTrue(TEXT("the index leads back to this definition"), Registry->GetDefinition(Carry.Index) == Carry.Definition);
	TestEqual(TEXT("the carrier is role 1"), Registry->FindRoleIndex(Carry.Index, GSTNExtTestLinkTags::Role_Carrier), 1);
	TestEqual(TEXT("the carried is role 2"), Registry->FindRoleIndex(Carry.Index, GSTNExtTestLinkTags::Role_Carried), 2);
	const FGSTN_LinkTypeEntry* Entry = Registry->GetEntry(Carry.Index);
	TestTrue(TEXT("the registry knows the carried role is parked"), Entry && Entry->ParkedRoleIndex == 2);
	TestTrue(TEXT("the build reported no problem"), Registry->GetBuildErrors().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExtTestLinkFormsTest, "GSTNExtTest.Link.FormsThroughTheLinkSteps", Flags)
bool FGSTN_ExtTestLinkFormsTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	FGSTN_LinkInboxHandlers::Register();
	FScopedCarry Carry;

	// F: the carrier's own attacker step marked its predicted request, as for a grab
	FGSTN_LinkFragment Carrier = PredictedCarry();
	TestTrue(TEXT("predicted: pending on the carry's index"), Carrier.Links.Body.Phase == EGSTN_LinkPhase::Pending && Carrier.Links.Body.LinkTypeIndex == Carry.Index);

	// The target's pipeline: GSTNLink's LinkRequest step runs the carry's own rule and accepts a standard weight
	FCarryRequest Request(Carrier, 64);
	FGSTN_StepResult Out;
	TestEqual(TEXT("the request stops the pipeline"), Request.Run(Out), EGSTN_StepResult::Stop);
	TestTrue(TEXT("an accepted carry stands"), Out.bHit);
	TestEqual(TEXT("one outcome for the link applier"), Out.OutcomeExtensions.Num(), 1);
	TestEqual(TEXT("one response back to the carrier"), Out.OutgoingEntries.Num(), 1);

	// F+1: GSTNLink's applier writes the carried role
	FGSTN_SyncFragmentArray TargetFragments;
	FGSTN_LinkOutcomeApplier Applier;
	FGSTN_OutcomeApplyContext Apply;
	Apply.VictimFragments = &TargetFragments;
	for (const FInstancedStruct& Extension : Out.OutcomeExtensions)
	{
		if (Extension.GetScriptStruct() == FGSTN_LinkHitOutcome::StaticStruct())
		{
			Applier.Apply(Extension, Request.View, Apply);
		}
	}
	const FGSTN_LinkFragment* Carried = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(TargetFragments);
	if (!TestNotNull(TEXT("the applier wrote the carried fragment"), Carried))
	{
		return false;
	}
	TestEqual(TEXT("carried: the carry's index"), static_cast<int32>(Carried->Links.Body.LinkTypeIndex), static_cast<int32>(Carry.Index));
	TestEqual(TEXT("carried: role 2"), static_cast<int32>(Carried->Links.Body.Role), 2);
	TestTrue(TEXT("carried: active from F+1"), Carried->Links.Body.Phase == EGSTN_LinkPhase::Active && Carried->Links.Body.LinkFrame == HitFrame + 1);
	TestTrue(TEXT("carried: parked and not acting, so its actions are vetoed by the existing tick extension"),
		FGSTN_LinkTickExtension::IsActionBlocked(*Carried, UGSTN_LinkRegistry::Get()));

	// F+2: the response reaches the carrier through GSTNLink's inbox kind
	FGSTN_InboxEntry Reply = Out.OutgoingEntries[0];
	Reply.Frame = HitFrame + 1;
	Reply.SenderId = CarriedId;
	FGSTN_CombatInbox Inbox;
	Inbox.Add(Reply);
	TArray<FGSTN_InboxEntry> Drained;
	Inbox.Drain(HitFrame + 2, Drained);
	FGSTN_CombatSyncState CarrierSync;
	FGSTN_SyncFragmentList::FindOrAdd<FGSTN_LinkFragment>(CarrierSync.Fragments) = Carrier;
	FGSTN_InboxDispatchContext Dispatch;
	Dispatch.Frame = HitFrame + 2;
	Dispatch.Sync = &CarrierSync;
	TestTrue(TEXT("one response at F+2, dispatched by the registered kind"), Drained.Num() == 1 && FGSTN_InboxKindRegistry::Get().Dispatch(Drained[0], Dispatch));
	const FGSTN_LinkFragment* Formed = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(CarrierSync.Fragments);
	TestTrue(TEXT("carrier: role 1 active, same serial and link frame"), Formed && Formed->Links.Body.Role == 1 && Formed->Links.Body.Phase == EGSTN_LinkPhase::Active
		&& Formed->Links.Body.Serial == Carried->Links.Body.Serial && Formed->Links.Body.LinkFrame == HitFrame + 1);
	TestFalse(TEXT("carrier: keeps acting (its role says so)"), Formed && FGSTN_LinkTickExtension::IsActionBlocked(*Formed, UGSTN_LinkRegistry::Get()));

	// Wire cost: a live carry is the same 1 + 22 bytes as any live link channel (GSTNLink.NP.Fragment.TwoChannelsBytes: 21 B of fields + header)
	TestEqual(TEXT("a live carry costs what any live link costs"), PayloadBytes(*Carried), 1 + 22);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExtTestLinkRuleRefusesTest, "GSTNExtTest.Link.OwnRuleRefuses", Flags)
bool FGSTN_ExtTestLinkRuleRefusesTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	FScopedCarry Carry;

	// Weight 200 against the rule's 150: the carry's own rule refuses inside GSTNLink's step; a refused request whiffs and posts nothing
	FCarryRequest Heavy(PredictedCarry(), 128);
	FGSTN_StepResult Out;
	TestEqual(TEXT("the refused request stops"), Heavy.Run(Out), EGSTN_StepResult::Stop);
	TestFalse(TEXT("and whiffs"), Out.bHit);
	TestEqual(TEXT("no outcome"), Out.OutcomeExtensions.Num(), 0);
	TestEqual(TEXT("no entry"), Out.OutgoingEntries.Num(), 0);

	// The rule alone, on a context made by hand: deterministic for equal inputs
	FGSTN_LinkRequestContext Context;
	Context.Frame = HitFrame;
	Context.LinkType = GSTNExtTestLinkTags::Type_Carry;
	Context.Definition = Carry.Definition;
	Context.Initiator.EntityId = CarrierId;
	Context.Target.EntityId = CarriedId;
	Context.Target.EffectiveWeight = 150.f;
	const UGSTN_ExtTestCarryRule* Rule = CastChecked<UGSTN_ExtTestCarryRule>(Carry.Definition->Rules[0]);
	TestTrue(TEXT("150 is at the limit: accepted"), Rule->Evaluate_Implementation(Context).bAccepted);
	Context.Target.EffectiveWeight = 150.5f;
	const FGSTN_LinkRuleResult Over = Rule->Evaluate_Implementation(Context);
	TestTrue(TEXT("150.5 is refused for Requirements"), !Over.bAccepted && Over.Refusal == EGSTN_LinkRefusal::Requirements);
	Context.Target.EntityId = CarrierId;
	TestTrue(TEXT("the shared checks still run first: no self carry"), Rule->Evaluate_Implementation(Context).Refusal == EGSTN_LinkRefusal::SelfLink);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExtTestLinkBreakRulesTest, "GSTNExtTest.Link.OwnBreakRules", Flags)
bool FGSTN_ExtTestLinkBreakRulesTest::RunTest(const FString& Parameters)
{
	FScopedCarry Carry;

	// The watchdog evaluates every type through UGSTN_LinkMath::EvaluateBreak with the type's own rules: the carry's 50 cm leash
	FGSTN_LinkBreakInput Input;
	Input.Frame = HitFrame + 30;
	Input.LinkFrame = HitFrame + 1;
	Input.OwnRoleTag = GSTNExtTestLinkTags::Role_Carried;
	Input.PartnerDistanceCm = 40.f;
	TestFalse(TEXT("40 cm from its socket: held"), UGSTN_LinkMath::EvaluateBreak(Carry.Definition->Break, 0, Input).bBreak);
	Input.PartnerDistanceCm = 80.f;
	const FGSTN_LinkBreakDecision Far = UGSTN_LinkMath::EvaluateBreak(Carry.Definition->Break, 0, Input);
	TestTrue(TEXT("80 cm: breaks for Distance"), Far.bBreak && Far.Reason == EGSTN_LinkBreakReason::Distance);
	Input.PartnerDistanceCm = 0.f;
	Input.bPartnerDead = true;
	const FGSTN_LinkBreakDecision Dead = UGSTN_LinkMath::EvaluateBreak(Carry.Definition->Break, 0, Input);
	TestTrue(TEXT("the carrier dies: PartnerDeath, the generic rule every type shares"), Dead.bBreak && Dead.Reason == EGSTN_LinkBreakReason::PartnerDeath);
	return true;
}

#endif
