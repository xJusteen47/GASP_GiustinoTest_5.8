// Held objects, simulation side tests (GSTNHeld.Runtime.*): the hold as a Hand link (M10 G1) and the watchdog's object partner.

#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_HeldLoadEffect.h"
#include "GSTN_HeldSettings.h"
#include "GSTN_HeldSim.h"
#include "GSTN_HeldTags.h"
#include "GSTN_HeldTypes.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkWatchdog.h"
#include "Misc/AutomationTest.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkEffects.h"
#include "Sim/GSTN_LinkInboxKinds.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNHeldRuntimeTest
{
	const FGSTN_LinkCommandEntry& CommandOf(const FGSTN_InboxEntry& Entry)
	{
		return *Entry.Payload.GetPtr<FGSTN_LinkCommandEntry>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldHandLinkTest, "GSTNHeld.Runtime.HandLink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldHandLinkTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHeldRuntimeTest;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const uint8 TypeIndex = FGSTN_HeldLinkType::GetTypeIndex(Registry);
	TestTrue(TEXT("the Held type is registered"), TypeIndex != 0);
	const UGSTN_LinkTypeDefinition* Definition = Registry ? Registry->GetDefinition(TypeIndex) : nullptr;
	TestTrue(TEXT("Hand channel, holder role, nobody parked"), Definition && Definition->Channel == EGSTN_LinkChannel::Hand
		&& Definition->InitiatorRole.RoleTag == GSTNHeldTags::Link_Role_Holder && Definition->GetParkedRoleIndex() == 0);
	if (TypeIndex == 0)
	{
		return false;
	}
	TArray<FString> Errors;
	Definition->Validate(Errors);
	TestEqual(TEXT("the definition validates"), Errors.Num(), 0);

	FGSTN_LinkFragment Fragment;
	Fragment.Actions.Exit.Frame = 77;
	const FGSTN_InboxEntry Hold = FGSTN_HeldLinkType::MakeHold(TypeIndex, 5, 3, 9);
	TestTrue(TEXT("hold forms"), FGSTN_LinkInboxHandlers::ApplyCommand(Fragment, CommandOf(Hold), 100));
	const FGSTN_LinkState& Hand = Fragment.Links.Hand;
	TestTrue(TEXT("holding: role 1, the object as partner"), FGSTN_HeldLinkType::IsHolding(Hand, TypeIndex) && Hand.Serial == 5 && Hand.LinkFrame == 101
		&& Hand.PartnerId == FGSTN_EntityRef(GSTNEntityKinds::Object, 3, 9));
	TestFalse(TEXT("the Body channel is untouched"), Fragment.Links.Body.IsLinked());
	TestFalse(TEXT("a second hold on a busy hand is refused"), FGSTN_LinkInboxHandlers::ApplyCommand(Fragment, CommandOf(FGSTN_HeldLinkType::MakeHold(TypeIndex, 6, 4, 1)), 101));

	TestFalse(TEXT("a stale release is ignored"), FGSTN_LinkInboxHandlers::ApplyCommand(Fragment,
		CommandOf(FGSTN_HeldLinkType::MakeRelease(4, FGSTN_HeldLinkType::ToLinkReason(EGSTN_HeldBreakReason::Dropped))), 102));
	TestTrue(TEXT("the matching release ends the hold"), FGSTN_LinkInboxHandlers::ApplyCommand(Fragment,
		CommandOf(FGSTN_HeldLinkType::MakeRelease(5, FGSTN_HeldLinkType::ToLinkReason(EGSTN_HeldBreakReason::Thrown))), 103));
	TestTrue(TEXT("empty hand, serial and reason kept"), !FGSTN_HeldLinkType::IsHolding(Hand, TypeIndex) && Hand.Serial == 5
		&& Hand.BreakReason == static_cast<uint8>(EGSTN_LinkBreakReason::Throw));
	TestEqual(TEXT("a Hand break leaves the Body exit record alone"), Fragment.Actions.Exit.Frame, 77);
	TestTrue(TEXT("the next hold needs a newer serial"), !FGSTN_LinkInboxHandlers::ApplyCommand(Fragment, CommandOf(Hold), 104)
		&& FGSTN_LinkInboxHandlers::ApplyCommand(Fragment, CommandOf(FGSTN_HeldLinkType::MakeHold(TypeIndex, 6, 3, 10)), 104));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldWatchdogPartnerTest, "GSTNHeld.Runtime.WatchdogObjectPartner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldWatchdogPartnerTest::RunTest(const FString& Parameters)
{
	UGSTN_LinkTypeDefinition* Definition = FGSTN_HeldLinkType::Build(nullptr);
	Definition->Break.MaxDistanceCm = 300.f;

	FGSTN_LinkWatchdogEntity Holder;
	Holder.EntityId = 3;
	FGSTN_LinkState& Hand = Holder.Links.Hand;
	Hand.LinkTypeIndex = 1;
	Hand.Role = 1;
	Hand.Phase = EGSTN_LinkPhase::Active;
	Hand.Serial = 2;
	Hand.LinkFrame = 10;
	// Object id 4 on purpose: the same number as a fighter entity, which must not be taken for the partner
	Hand.PartnerId = FGSTN_EntityRef(GSTNEntityKinds::Object, 4, 1);
	FGSTN_LinkWatchdogEntity Fighter4;
	Fighter4.EntityId = 4;
	const FGSTN_LinkWatchdogEntity Entities[] = { Holder, Fighter4 };

	FGSTN_LinkWatchdogLogic::FParams Params;
	Params.Definitions = [Definition](uint8 Index) { return Index == 1 ? Definition : nullptr; };
	TArray<FGSTN_LinkWatchdogPost> Posts;
	TArray<FGSTN_LinkWatchdogBreak> Breaks;

	FGSTN_LinkWatchdogLogic::Evaluate(Entities, 40, Params, Posts, Breaks);
	TestEqual(TEXT("no resolver: left alone"), Breaks.Num(), 0);

	bool bValid = true;
	FVector Where(100.0, 0.0, 0.0);
	Params.ExternalPartner = [&bValid, &Where](const FGSTN_EntityRef& Partner, FGSTN_LinkPartnerView& OutView)
	{
		OutView.bValid = bValid && Partner.Kind == GSTNEntityKinds::Object && Partner.Id == 4;
		OutView.Transform = FTransform(Where);
		return true;
	};
	FGSTN_LinkWatchdogLogic::Evaluate(Entities, 40, Params, Posts, Breaks);
	TestEqual(TEXT("object in reach: no break"), Breaks.Num(), 0);

	Where = FVector(500.0, 0.0, 0.0);
	FGSTN_LinkWatchdogLogic::Evaluate(Entities, 40, Params, Posts, Breaks);
	TestTrue(TEXT("object too far: Distance break, the holder only"), Breaks.Num() == 1 && Breaks[0].Reason == EGSTN_LinkBreakReason::Distance
		&& Posts.Num() == 1 && Posts[0].TargetId == 3 && Posts[0].Command.Channel == EGSTN_LinkChannel::Hand && Breaks[0].PartnerId == INDEX_NONE);

	Where = FVector(100.0, 0.0, 0.0);
	bValid = false;
	Posts.Reset();
	Breaks.Reset();
	FGSTN_LinkWatchdogLogic::Evaluate(Entities, 11, Params, Posts, Breaks);
	TestEqual(TEXT("object gone within the orphan grace: no break yet"), Breaks.Num(), 0);
	FGSTN_LinkWatchdogLogic::Evaluate(Entities, 40, Params, Posts, Breaks);
	TestTrue(TEXT("object gone: Forced orphan break of the holder's hand"), Breaks.Num() == 1 && Breaks[0].bOrphan
		&& Breaks[0].Reason == EGSTN_LinkBreakReason::Forced && Posts.Num() == 1 && Posts[0].TargetId == 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldCarryLoadTest, "GSTNHeld.Runtime.CarryLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldCarryLoadTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHeldRuntimeTest;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const uint8 TypeIndex = FGSTN_HeldLinkType::GetTypeIndex(Registry);
	const UGSTN_LinkTypeDefinition* Definition = Registry ? Registry->GetDefinition(TypeIndex) : nullptr;
	if (!TestTrue(TEXT("the Held type is registered"), Definition != nullptr))
	{
		return false;
	}
	// The holder's role effect is the carry load; the object's side has none
	TestTrue(TEXT("holder carries GE_HoldLoad"), FGSTN_LinkEffects::ResolveEffectClass(Definition->InitiatorRole.LinkEffect) == UGSTN_GE_HoldLoad::StaticClass()
		&& Definition->InitiatorRole.LinkEffect.SetByCallerTag == GSTNHeldTags::SetByCaller_Held_Load);
	TestFalse(TEXT("object role has no effect"), Definition->TargetRole.LinkEffect.IsSet());
	TestEqual(TEXT("walk, jump, air jump"), GetDefault<UGSTN_GE_HoldLoad>()->Modifiers.Num(), 3);

	// The payload rides the Form into the link state; the link tick applies the effect with it and removes it at the release
	const uint8 Payload = FGSTN_HeldLoad::EncodePayload(1, 1.f);
	FGSTN_LinkFragment Before;
	FGSTN_LinkFragment Holding;
	TestTrue(TEXT("hold forms"), FGSTN_LinkInboxHandlers::ApplyCommand(Holding, CommandOf(FGSTN_HeldLinkType::MakeHold(TypeIndex, 5, 3, 9, Payload)), 100));
	TestEqual(TEXT("payload in the link state"), Holding.Links.Hand.Payload, Payload);
	TArray<FGSTN_LinkEffects::FOp, TInlineAllocator<2>> Ops;
	FGSTN_LinkEffects::ComputeOps(&Before, &Holding, Registry, Ops);
	TestTrue(TEXT("one apply with the payload"), Ops.Num() == 1 && Ops[0].bApply && Ops[0].Payload == Payload);
	FGSTN_LinkFragment Released = Holding;
	FGSTN_LinkInboxHandlers::ApplyCommand(Released, CommandOf(FGSTN_HeldLinkType::MakeRelease(5, FGSTN_HeldLinkType::ToLinkReason(EGSTN_HeldBreakReason::HolderState))), 110);
	FGSTN_LinkEffects::ComputeOps(&Holding, &Released, Registry, Ops);
	TestTrue(TEXT("one removal at the release"), Ops.Num() == 1 && !Ops[0].bApply);

	// MMC goldens against the project's default classes (Medium at full load: the class's loss)
	const FGSTN_HeldSizeClass* Medium = UGSTN_HeldSettings::Get().GetSizeClass(1);
	if (Medium)
	{
		TestTrue(TEXT("speed golden"), FMath::IsNearlyEqual(UGSTN_GE_HoldLoad::SpeedScaleOf(Payload), 1.f - Medium->SpeedLossAtFullLoad, 1.e-4f));
		TestTrue(TEXT("jump golden"), FMath::IsNearlyEqual(UGSTN_GE_HoldLoad::JumpScaleOf(Payload), 1.f - Medium->JumpLossAtFullLoad, 1.e-4f));
	}
	TestEqual(TEXT("no load, no loss"), UGSTN_GE_HoldLoad::SpeedScaleOf(FGSTN_HeldLoad::EncodePayload(2, 0.f)), 1.f);
	return true;
}

#endif
