// Test only (M6 F14, m6 spec 12 item 7): a link type added from this module, outside GSTNLink, through the public API only. It registers with
// the registry, forms through GSTNLink's own LinkRequest step, outcome applier and inbox kind, parks its target, refuses by its own rule,
// breaks by its own break rules and costs the same bytes as any link: nothing in GSTNLink or in the combat model's tick was changed for it.

#include "GSTN_ExtTestCarryRule.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_InteractTypes.h"
#include "GSTN_LinkInteractProvider.h"
#include "GSTN_LinkRegistry.h"
#include "Sim/GSTN_LinkAttach.h"
#include "StructUtils/InstancedStruct.h"
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
			// M7 G1: the interact button offers it under its own meaning, below the grab, to kits that carry its kit entry
			Definition->InteractOffer.Meaning = GSTNExtTestLinkTags::Interact_Target_Carry;
			Definition->InteractOffer.Priority = 30;
			Definition->InteractOffer.RequiredKitExtension = FGSTN_ExtTestCarryKit::StaticStruct();

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExtTestLinkOfferTest, "GSTNExtTest.Link.InteractOfferFromOutside", Flags)
bool FGSTN_ExtTestLinkOfferTest::RunTest(const FString& Parameters)
{
	FScopedCarry Carry;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();

	// M7 G1: the definition's own offer is compiled by GSTNLink's registry, meaning and all; nothing in GSTNLink names it
	const FGSTN_LinkOfferEntry* Entry = Registry->GetInteractOffers().FindByPredicate([&Carry](const FGSTN_LinkOfferEntry& Offer) { return Offer.Definition == Carry.Definition; });
	TestTrue(TEXT("the carry is an interact offer"), Entry && Entry->Offer.Meaning == GSTNExtTestLinkTags::Interact_Target_Carry && Entry->Offer.Priority == 30);
	const int32 MeaningIndex = Registry->GetInteractMeanings().IndexOf(GSTNExtTestLinkTags::Interact_Target_Carry);
	TestTrue(TEXT("its meaning joined the probe's table"), MeaningIndex != 0);

	// Offered to a kit that carries the carry's own kit entry only
	FGSTN_KitExtensionList CarrierKit;
	FGSTN_ExtTestCarryKit CarryData;
	CarryData.MaxCarryWeight = 200.f;
	CarrierKit.ExtensionData.Add(FInstancedStruct::Make(CarryData));
	FGSTN_KitExtensionList PlainKit;
	auto IsOffered = [&Carry](TConstArrayView<FGSTN_LinkInteractOffer> Offers)
	{
		return Offers.ContainsByPredicate([&Carry](const FGSTN_LinkInteractOffer& Offer) { return Offer.Definition == Carry.Definition; });
	};
	TArray<FGSTN_LinkInteractOffer, TInlineAllocator<4>> Offers;
	FGSTN_LinkInteractProvider::MakeOffers(*Registry, &PlainKit, nullptr, Offers);
	TestFalse(TEXT("a kit without the carry entry is not offered the carry"), IsOffered(Offers));
	Offers.Reset();
	FGSTN_LinkInteractProvider::MakeOffers(*Registry, &CarrierKit, nullptr, Offers);
	TestTrue(TEXT("a carrier kit is"), IsOffered(Offers));

	// The pre-filter runs the carry's rule with the interactor's kit: 180 fits this kit's 200, not the rule's own 150
	FGSTN_LinkInteractOffer Only;
	for (const FGSTN_LinkInteractOffer& Offer : Offers)
	{
		Only = Offer.Definition == Carry.Definition ? Offer : Only;
	}
	const FGSTN_LinkInteractOffer OnlyOffers[] = { Only };
	FGSTN_LinkInteractSource Source;
	Source.Interactor.EntityId = CarrierId;
	Source.Interactor.KitExtensions = &CarrierKit;
	Source.Offers = OnlyOffers;
	Source.Registry = Registry;
	FGSTN_LinkInteractEntity Heavy;
	Heavy.Side.EntityId = CarriedId;
	Heavy.Side.EffectiveWeight = 180.f;
	TestEqual(TEXT("180 is offered to a 200 kit"), FGSTN_LinkInteractProvider::FindAcceptedOffer(Source, Heavy, HitFrame), 0);
	Source.Interactor.KitExtensions = &PlainKit;
	TestEqual(TEXT("and refused with the rule's own 150"), FGSTN_LinkInteractProvider::FindAcceptedOffer(Source, Heavy, HitFrame), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExtTestLinkPayloadTest, "GSTNExtTest.Link.PayloadFromOwnRule", Flags)
bool FGSTN_ExtTestLinkPayloadTest::RunTest(const FString& Parameters)
{
	FGSTN_LinkFragmentRegistration::Register();
	FGSTN_LinkInboxHandlers::Register();
	FScopedCarry Carry;

	// The carrier's body holds a kit with the carry entry (capacity 200); the target weighs 100: the rule writes 255 x 100 / 200 = 128
	FGSTN_KitExtensionList CarrierKit;
	FGSTN_ExtTestCarryKit CarryData;
	CarryData.MaxCarryWeight = 200.f;
	CarrierKit.ExtensionData.Add(FInstancedStruct::Make(CarryData));
	FGSTN_LinkFragment Carrier = PredictedCarry();
	FCarryRequest Request(Carrier, 64);
	Request.InitiatorExt.BodyIndex = FGSTN_LinkAttach::AddForTest({}, nullptr, &CarrierKit);
	FGSTN_StepResult Out;
	Request.Run(Out);
	const FGSTN_LinkHitOutcome* Outcome = Out.OutcomeExtensions.Num() == 1 ? Out.OutcomeExtensions[0].GetPtr<FGSTN_LinkHitOutcome>() : nullptr;
	if (!TestNotNull(TEXT("accepted with an outcome"), Outcome))
	{
		return false;
	}
	TestEqual(TEXT("payload from the carry rule"), static_cast<int32>(Outcome->Payload), 128);
	const FGSTN_LinkResponseEntry* Response = Out.OutgoingEntries.Num() == 1 ? Out.OutgoingEntries[0].Payload.GetPtr<FGSTN_LinkResponseEntry>() : nullptr;
	TestTrue(TEXT("the response carries it to the carrier"), Response && Response->Payload == 128);

	// Both sides hold it: the carried through GSTNLink's applier, the carrier through its response
	FGSTN_SyncFragmentArray TargetFragments;
	FGSTN_OutcomeApplyContext Apply;
	Apply.VictimFragments = &TargetFragments;
	FGSTN_LinkOutcomeApplier().Apply(Out.OutcomeExtensions[0], Request.View, Apply);
	const FGSTN_LinkFragment* Carried = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(TargetFragments);
	TestTrue(TEXT("carried: payload 128"), Carried && Carried->Links.Body.Payload == 128);
	if (Response)
	{
		FGSTN_LinkInboxHandlers::ApplyResponse(Carrier, *Response, CarriedId);
	}
	TestTrue(TEXT("carrier: payload 128, same serial"), Carrier.Links.Body.Payload == 128 && Carried && Carrier.Links.Body.Serial == Carried->Links.Body.Serial);
	TestEqual(TEXT("a live carry with a payload still costs 1 + 22 bytes"), PayloadBytes(Carrier), 1 + 22);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ExtTestPartnerMoveTest, "GSTNExtTest.Link.PartnerMoveFromOutside", Flags)
bool FGSTN_ExtTestPartnerMoveTest::RunTest(const FString& Parameters)
{
	// M7 G2: a provider of this module registers for the carry and drives a partner move through GSTNLink's key step, inbox rule and link tick
	FScopedCarry Carry;
	UGSTN_ExtTestCarryMoves* Moves = NewObject<UGSTN_ExtTestCarryMoves>(GetTransientPackage());
	Moves->AddToRoot();
	Moves->CarriedAbility = UGSTN_ExtTestCarryRule::StaticClass();
	UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	TestTrue(TEXT("registered from outside"), Registry->RegisterActionProvider(GSTNExtTestLinkTags::Type_Carry, Moves));
	TestFalse(TEXT("an object without the interface is refused"), Registry->RegisterActionProvider(GSTNExtTestLinkTags::Type_Carry, Carry.Definition));
	TestTrue(TEXT("the registry resolves through it"), Registry->GetActionProvider(Carry.Index) == static_cast<const IGSTN_LinkActionProvider*>(Moves));

	constexpr uint8 Serial = 3;
	constexpr int32 KeyFrame = HitFrame + 40;
	FGSTN_LinkFragment Carrier;
	Carrier.Links.Body.LinkTypeIndex = Carry.Index;
	Carrier.Links.Body.Role = 1;
	Carrier.Links.Body.Phase = EGSTN_LinkPhase::Active;
	Carrier.Links.Body.PartnerId = FGSTN_EntityRef(GSTNEntityKinds::Fighter, static_cast<uint16>(CarriedId));
	Carrier.Links.Body.Serial = Serial;
	FGSTN_LinkFragment Carried = Carrier;
	Carried.Links.Body.Role = 2;
	Carried.Links.Body.PartnerId = FGSTN_EntityRef(GSTNEntityKinds::Fighter, static_cast<uint16>(CarrierId));

	FGSTN_LinkActionInfo Info;
	TestTrue(TEXT("the toss resolves"), Registry->ResolveLinkAction(Carry.Index, UGSTN_ExtTestCarryMoves::TossId, Info));
	FGSTN_LinkPartnerMoveEntry Entry;
	TestTrue(TEXT("the carrier's key starts it"), FGSTN_LinkTickExtension::StartPartnerMove(Carrier, UGSTN_ExtTestCarryMoves::TossId, KeyFrame, Info, Entry));
	TestTrue(TEXT("the carried side schedules it"), FGSTN_LinkInboxHandlers::ApplyPartnerMove(Carried, Entry, KeyFrame));

	FGSTN_LinkTickExtension::FPartnerMoveFrame Frame;
	Frame.Frame = KeyFrame + 3;
	FGSTN_LinkTickExtension::FPartnerMoveResult Result;
	FGSTN_LinkTickExtension::StepPartnerMoves(Carried, Frame, Registry, Result);
	TestTrue(TEXT("the carried part starts 3 frames on, with the provider's ability"), Result.StartAbility == UGSTN_ExtTestCarryRule::StaticClass());

	// The carrier's timeline (started 2 frames before the key) crosses its frame 6: the link breaks with the provider's reason
	FGSTN_LinkTickExtension::FPartnerMoveFrame Outcome;
	Outcome.Frame = KeyFrame + 4;
	Outcome.bInTimeline = true;
	Outcome.TimelineStartFrame = KeyFrame - 2;
	Outcome.bTimelineCrossed = true;
	Outcome.TimelinePrevious = 5;
	Outcome.TimelineCurrent = 6;
	FGSTN_LinkTickExtension::FPartnerMoveResult OutcomeResult;
	FGSTN_LinkTickExtension::StepPartnerMoves(Carrier, Outcome, Registry, OutcomeResult);
	TestTrue(TEXT("pending break, reason Throw"), Carrier.Links.Body.HasFlag(GSTNLinkFlags::PendingBreak)
		&& Carrier.Links.Body.GetBreakReason() == EGSTN_LinkBreakReason::Throw);

	Registry->UnregisterActionProvider(GSTNExtTestLinkTags::Type_Carry);
	TestTrue(TEXT("unregistered"), Registry->GetActionProvider(Carry.Index) == nullptr);
	Moves->RemoveFromRoot();
	return true;
}
#endif
