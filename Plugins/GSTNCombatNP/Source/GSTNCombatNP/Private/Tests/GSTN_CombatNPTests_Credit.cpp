// Automation tests of the partner credit of a hit (M7 G3): the neutral fields in the outcome, the meter split of the MeterGain step and the
// partner's copies of the HitLanded and KO events. No world: the step and the event writers are pure functions.

#include "Misc/AutomationTest.h"
#include "GSTN_CombatNPTags.h"
#include "GSTN_CombatTags.h"
#include "Hits/GSTN_HitResolution.h"
#include "Hits/GSTN_HitRunner.h"
#include "Hits/GSTN_HitSteps.h"
#include "Rules/GSTN_CombatRules.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	float SumMeter(const TArray<FGSTN_HitRequest>& Requests, EGSTN_HitRequestTarget Target, int32& OutCount)
	{
		float Sum = 0.f;
		OutCount = 0;
		for (const FGSTN_HitRequest& Request : Requests)
		{
			if (Request.Kind == EGSTN_HitRequestKind::MeterGain && Request.Target == Target)
			{
				Sum += Request.Magnitude;
				++OutCount;
			}
		}
		return Sum;
	}

	int32 CountTagged(const FGSTN_SimEventOutbox& Outbox, const FGameplayTag& Tag)
	{
		int32 Count = 0;
		for (const FGSTN_SimEvent& Event : Outbox.Events)
		{
			Count += Event.Tags.HasTagExact(Tag) ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CreditEmptyByDefaultTest, "GSTNCombat.Hits.Credit.EmptyByDefault", GSTN_NP_TEST_FLAGS)
bool FGSTN_CreditEmptyByDefaultTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("an outcome names no partner"), FGSTN_HitOutcome().Credit.IsSet());
	TestFalse(TEXT("a step result names no partner"), FGSTN_StepResult().Credit.IsSet());

	// The merge: an unset credit keeps what an earlier step wrote, a set one replaces it
	FGSTN_HitResolutionContext Context;
	FGSTN_StepResult First;
	First.Credit.PartnerEntityId = 4;
	First.Credit.PartnerShare = 64;
	FGSTN_HitPipelineRunner::Merge(First, Context);
	FGSTN_HitPipelineRunner::Merge(FGSTN_StepResult(), Context);
	TestTrue(TEXT("an empty step keeps the credit"), Context.Outcome.Credit.PartnerEntityId == 4 && Context.Outcome.Credit.PartnerShare == 64);
	FGSTN_StepResult Second;
	Second.Credit.PartnerEntityId = 9;
	FGSTN_HitPipelineRunner::Merge(Second, Context);
	TestTrue(TEXT("a later step replaces it"), Context.Outcome.Credit.PartnerEntityId == 9 && Context.Outcome.Credit.PartnerShare == 0);

	// Without a credit the meter step writes what it wrote before G3: one attacker request with the whole amount
	TStrongObjectPtr<UGSTN_CombatRules> Rules(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
	Rules->MeterGainPerDamage = 2.f;
	FGSTN_HitContextView View;
	View.Rules = Rules.Get();
	View.Outcome.Damage = 10.f;
	FGSTN_StepResult Out;
	GetDefault<UGSTN_HitStep_MeterGain>()->Resolve_Implementation(View, Out);
	int32 AttackerCount = 0;
	int32 PartnerCount = 0;
	TestEqual(TEXT("the attacker gains it all"), SumMeter(Out.Requests, EGSTN_HitRequestTarget::Attacker, AttackerCount), 20.f);
	SumMeter(Out.Requests, EGSTN_HitRequestTarget::Partner, PartnerCount);
	TestTrue(TEXT("in one request, none for a partner"), AttackerCount == 1 && PartnerCount == 0 && Out.Requests.Num() == 1);
	TestTrue(TEXT("one meter gain of the whole amount"), Out.MeterGains.Num() == 1 && Out.MeterGains[0].Amount == 20.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CreditMeterSplitTest, "GSTNCombat.Hits.Credit.MeterSplit", GSTN_NP_TEST_FLAGS)
bool FGSTN_CreditMeterSplitTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_CombatRules> Rules(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
	Rules->MeterGainPerDamage = 1.f;
	FGSTN_HitContextView View;
	View.Rules = Rules.Get();
	View.Outcome.Damage = 10.f;
	View.Outcome.Credit.PartnerEntityId = 7;

	struct FCase { uint8 Share; int32 AttackerRequests; int32 PartnerRequests; };
	const FCase Cases[] = { { 0, 1, 0 }, { 128, 1, 1 }, { 255, 0, 1 } };
	for (const FCase& Case : Cases)
	{
		View.Outcome.Credit.PartnerShare = Case.Share;
		FGSTN_StepResult Out;
		GetDefault<UGSTN_HitStep_MeterGain>()->Resolve_Implementation(View, Out);
		int32 AttackerCount = 0;
		int32 PartnerCount = 0;
		const float Owner = SumMeter(Out.Requests, EGSTN_HitRequestTarget::Attacker, AttackerCount);
		const float Partner = SumMeter(Out.Requests, EGSTN_HitRequestTarget::Partner, PartnerCount);
		const float Expected = FGSTN_HitCredit::PartnerPart(10.f, Case.Share);
		TestEqual(FString::Printf(TEXT("share %d: the partner's part"), Case.Share), Partner, Expected);
		TestEqual(FString::Printf(TEXT("share %d: the two add up to the amount"), Case.Share), Owner + Partner, 10.f);
		TestEqual(FString::Printf(TEXT("share %d: attacker requests"), Case.Share), AttackerCount, Case.AttackerRequests);
		TestEqual(FString::Printf(TEXT("share %d: partner requests"), Case.Share), PartnerCount, Case.PartnerRequests);
		TestEqual(FString::Printf(TEXT("share %d: MeterGains is the attacker's only"), Case.Share), Out.MeterGains.Num(), Case.AttackerRequests);
	}
	TestEqual(TEXT("half on the 0..255 scale is 128"), FGSTN_HitCredit::PartnerPart(255.f, 128), 128.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CreditEventCarriesPartnerTest, "GSTNCombat.Hits.Credit.EventCarriesPartner", GSTN_NP_TEST_FLAGS)
bool FGSTN_CreditEventCarriesPartnerTest::RunTest(const FString& Parameters)
{
	FGSTN_VictimHit Hit;
	Hit.Entry.Hit.Damage = 12.f;
	{
		FGSTN_SimEventOutbox Outbox;
		FGSTN_CombatVictim::AppendHitEvents(Hit, 5, nullptr, nullptr, FGameplayTagContainer(), Outbox);
		TestEqual(TEXT("no credit: HitLanded and HitReceived, as before"), Outbox.Events.Num(), 2);
		TestEqual(TEXT("none tagged as credit"), CountTagged(Outbox, GSTNCombatNPTags::EventHit_CreditPartner), 0);
	}
	Hit.Outcome.Credit.PartnerEntityId = 3;
	Hit.Outcome.Credit.PartnerShare = 128;
	{
		FGSTN_SimEventOutbox Outbox;
		FGSTN_CombatVictim::AppendHitEvents(Hit, 5, nullptr, nullptr, FGameplayTagContainer(), Outbox);
		TestEqual(TEXT("a credit adds the partner's HitLanded"), Outbox.Events.Num(), 3);
		if (Outbox.Events.Num() == 3)
		{
			TestTrue(TEXT("the attacker's comes first, untagged"), Outbox.Events[0].EventType == GSTNCombatTags::Event_HitLanded && !Outbox.Events[0].Tags.HasTagExact(GSTNCombatNPTags::EventHit_CreditPartner));
			TestTrue(TEXT("then the partner's copy, tagged"), Outbox.Events[1].EventType == GSTNCombatTags::Event_HitLanded && Outbox.Events[1].Tags.HasTagExact(GSTNCombatNPTags::EventHit_CreditPartner));
			TestEqual(TEXT("with the hit's damage"), Outbox.Events[1].Magnitude, 12.f);
			TestTrue(TEXT("then the victim's HitReceived"), Outbox.Events[2].EventType == GSTNCombatTags::Event_HitReceived);
		}
	}
	{
		FGSTN_SimEventOutbox Outbox;
		FGSTN_CombatVictim::AppendKOEvent(EGSTN_KOCause::Health, 6, nullptr, nullptr, Outbox, &Hit.Outcome.Credit);
		TestEqual(TEXT("a credited health KO has a partner copy"), Outbox.Events.Num(), 2);
		TestEqual(TEXT("one of them tagged"), CountTagged(Outbox, GSTNCombatNPTags::EventHit_CreditPartner), 1);
	}
	{
		FGSTN_SimEventOutbox Outbox;
		FGSTN_CombatVictim::AppendKOEvent(EGSTN_KOCause::Zone, 6, nullptr, nullptr, Outbox, &Hit.Outcome.Credit);
		TestEqual(TEXT("a zone KO credits nobody"), Outbox.Events.Num(), 1);
	}
	return true;
}

#endif
