#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_HealthAttributeSet.h"
#include "Attributes/GSTN_MeterAttributeSet.h"
#include "Effects/GSTN_GE_Damage.h"
#include "Effects/GSTN_GE_MeterGain.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayTagsManager.h"
#include "GSTN_CombatRulesComponent.h"
#include "GSTN_CombatTags.h"
#include "GSTN_HitEffectContext.h"
#include "Events/GSTN_HitEvents.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Input/GSTN_CombatSettings.h"
#include "Rules/GSTN_CombatRules.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Tests/GSTN_GASTestTypes.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags DamageTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Rules whose only content is the damage mode: the execution reads nothing else */
	TStrongObjectPtr<UGSTN_CombatRules> MakeRules(EGSTN_DamageMode Mode)
	{
		UGSTN_CombatRules* Rules = NewObject<UGSTN_CombatRules>(GetTransientPackage());
		Rules->DamageMode = Mode;
		return TStrongObjectPtr<UGSTN_CombatRules>(Rules);
	}

	/** Applies the damage effect to the rig's combatant (it hits itself, so its own multipliers apply on both sides) */
	void ApplyDamage(GSTNCombatGASTest::FRig& Rig, const UGSTN_CombatRules* Rules, float Damage)
	{
		AActor* Owner = Rig.ASC->GetOwner();
		const FGameplayEffectContextHandle Context = FGSTN_HitEffectContext::Make(Owner, Owner, Rules);
		FGameplayEffectSpecHandle Spec = Rig.ASC->MakeOutgoingSpec(UGSTN_GE_Damage::StaticClass(), 1.f, Context);
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatTags::SetByCaller_Damage, Damage);
		Rig.ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}

	float DamageBase(const UAbilitySystemComponent& ASC, const FGameplayAttribute& Attribute)
	{
		return ASC.GetNumericAttributeBase(Attribute);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DamagePercentModeAddsTest, "GSTNCombat.GAS.Damage.PercentModeAdds", DamageTestFlags)
bool FGSTN_DamagePercentModeAddsTest::RunTest(const FString& Parameters)
{
	GSTNCombatGASTest::FRig Rig;
	const TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules(EGSTN_DamageMode::Percent);

	ApplyDamage(Rig, Rules.Get(), 12.f);
	ApplyDamage(Rig, Rules.Get(), 12.f);
	TestEqual(TEXT("the percent grows by every hit"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetDamagePercentAttribute()), 24.f);
	TestEqual(TEXT("and health is untouched"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetHealthAttribute()), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetMaxHealthAttribute()));

	// The percent mode has no cap: a heavy hit goes past the maximum
	ApplyDamage(Rig, Rules.Get(), 500.f);
	TestEqual(TEXT("past MaxHealth"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetDamagePercentAttribute()), 524.f);

	// A plain context (no rules) counts as the percent mode, and a negative damage heals nothing
	ApplyDamage(Rig, nullptr, 6.f);
	ApplyDamage(Rig, Rules.Get(), -50.f);
	TestEqual(TEXT("a context without rules adds percent, a negative damage nothing"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetDamagePercentAttribute()), 530.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DamageHealthModeSubtractsTest, "GSTNCombat.GAS.Damage.HealthModeSubtracts", DamageTestFlags)
bool FGSTN_DamageHealthModeSubtractsTest::RunTest(const FString& Parameters)
{
	GSTNCombatGASTest::FRig Rig;
	const TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules(EGSTN_DamageMode::Health);
	const float Full = DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetMaxHealthAttribute());
	TestTrue(TEXT("the defaults give a full health bar"), Full > 0.f && DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetHealthAttribute()) == Full);

	ApplyDamage(Rig, Rules.Get(), 30.f);
	TestEqual(TEXT("health falls by the damage"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetHealthAttribute()), Full - 30.f);
	TestEqual(TEXT("and the percent is untouched"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetDamagePercentAttribute()), 0.f);

	ApplyDamage(Rig, Rules.Get(), 1000.f);
	TestEqual(TEXT("health stops at zero"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetHealthAttribute()), 0.f);

	// Lowering the maximum pulls the health down with it
	Rig.ASC->SetNumericAttributeBase(UGSTN_HealthAttributeSet::GetHealthAttribute(), Full);
	Rig.ASC->SetNumericAttributeBase(UGSTN_HealthAttributeSet::GetMaxHealthAttribute(), 40.f);
	TestEqual(TEXT("the base clamp keeps health under the maximum"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetHealthAttribute()), Full);
	Rig.ASC->SetNumericAttributeBase(UGSTN_HealthAttributeSet::GetHealthAttribute(), 60.f);
	TestEqual(TEXT("a write above the maximum is clamped"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetHealthAttribute()), 40.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DamagePercentCappedClampsAtMaxTest, "GSTNCombat.GAS.Damage.PercentCappedClampsAtMax", DamageTestFlags)
bool FGSTN_DamagePercentCappedClampsAtMaxTest::RunTest(const FString& Parameters)
{
	GSTNCombatGASTest::FRig Rig;
	const TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules(EGSTN_DamageMode::PercentCapped);
	Rig.ASC->SetNumericAttributeBase(UGSTN_HealthAttributeSet::GetMaxHealthAttribute(), 50.f);

	ApplyDamage(Rig, Rules.Get(), 30.f);
	TestEqual(TEXT("under the cap the percent adds up"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetDamagePercentAttribute()), 30.f);
	ApplyDamage(Rig, Rules.Get(), 30.f);
	TestEqual(TEXT("the hit that passes the cap stops exactly at it"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetDamagePercentAttribute()), 50.f);
	ApplyDamage(Rig, Rules.Get(), 30.f);
	TestEqual(TEXT("and further hits add nothing"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetDamagePercentAttribute()), 50.f);
	TestEqual(TEXT("health is not used in this mode"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetHealthAttribute()), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DamageMultipliersAppliedTest, "GSTNCombat.GAS.Damage.MultipliersApplied", DamageTestFlags)
bool FGSTN_DamageMultipliersAppliedTest::RunTest(const FString& Parameters)
{
	GSTNCombatGASTest::FRig Rig;
	const TStrongObjectPtr<UGSTN_CombatRules> Rules = MakeRules(EGSTN_DamageMode::Percent);
	Rig.ASC->SetNumericAttributeBase(UGSTN_HealthAttributeSet::GetOutgoingDamageMultiplierAttribute(), 2.f);
	Rig.ASC->SetNumericAttributeBase(UGSTN_HealthAttributeSet::GetIncomingDamageMultiplierAttribute(), 1.5f);

	// The combatant hits itself, so it is both the attacker (outgoing) and the victim (incoming)
	ApplyDamage(Rig, Rules.Get(), 10.f);
	TestEqual(TEXT("10 x 2 x 1.5"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetDamagePercentAttribute()), 30.f);

	Rig.ASC->SetNumericAttributeBase(UGSTN_HealthAttributeSet::GetIncomingDamageMultiplierAttribute(), 0.f);
	ApplyDamage(Rig, Rules.Get(), 10.f);
	TestEqual(TEXT("a victim that takes no damage takes none"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetDamagePercentAttribute()), 30.f);

	Rig.ASC->SetNumericAttributeBase(UGSTN_HealthAttributeSet::GetIncomingDamageMultiplierAttribute(), -3.f);
	TestEqual(TEXT("a multiplier cannot go negative"), DamageBase(*Rig.ASC, UGSTN_HealthAttributeSet::GetIncomingDamageMultiplierAttribute()), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DamageEffectContextNetSerializeRoundTripTest, "GSTNCombat.GAS.Damage.EffectContextNetSerializeRoundTrip", DamageTestFlags)
bool FGSTN_DamageEffectContextNetSerializeRoundTripTest::RunTest(const FString& Parameters)
{
	FGSTN_HitEffectContext Source;
	Source.TimelineIndex = 3;
	Source.HitboxIndex = 1;
	Source.StartFrame = 12345;
	Source.HitLocation = FVector_NetQuantize10(120.5, -40.1, 33.3);
	Source.Relationship = GSTNCombatHitTags::Relationship_Enemy;
	Source.Reaction = EGSTN_HitReaction::Launch;
	Source.bStaggered = true;
	Source.HurtboxIndex = 2;
	Source.KOReason = GSTNCombatTags::KOReason_Zone;

	bool bSuccess = false;
	FNetBitWriter Writer(nullptr, 4096);
	Source.NetSerialize(Writer, nullptr, bSuccess);
	TestTrue(TEXT("the context serializes"), bSuccess);

	FNetBitReader Reader(nullptr, Writer.GetData(), Writer.GetNumBits());
	FGSTN_HitEffectContext Copy;
	Copy.NetSerialize(Reader, nullptr, bSuccess);
	TestTrue(TEXT("and reads back"), bSuccess);
	TestFalse(TEXT("without leftovers or an overrun"), Reader.IsError());
	TestEqual(TEXT("timeline index"), Copy.TimelineIndex, 3);
	TestEqual(TEXT("hitbox index"), Copy.HitboxIndex, 1);
	TestEqual(TEXT("start frame"), Copy.StartFrame, 12345);
	TestTrue(TEXT("location (quantized to a tenth)"), Copy.HitLocation.Equals(Source.HitLocation, 0.11));
	TestTrue(TEXT("relationship"), Copy.Relationship == Source.Relationship);
	TestTrue(TEXT("reaction"), Copy.Reaction == EGSTN_HitReaction::Launch);
	TestTrue(TEXT("staggered"), Copy.bStaggered);
	TestEqual(TEXT("hurtbox index"), Copy.HurtboxIndex, 2);
	TestTrue(TEXT("KO reason"), Copy.KOReason == GSTNCombatTags::KOReason_Zone);
	TestFalse(TEXT("the rules never travel"), Copy.Rules.IsValid());

	// Unset ids (INDEX_NONE) and empty tags survive too
	FGSTN_HitEffectContext Empty;
	FNetBitWriter EmptyWriter(nullptr, 4096);
	Empty.NetSerialize(EmptyWriter, nullptr, bSuccess);
	FNetBitReader EmptyReader(nullptr, EmptyWriter.GetData(), EmptyWriter.GetNumBits());
	FGSTN_HitEffectContext EmptyCopy;
	EmptyCopy.TimelineIndex = 9;
	EmptyCopy.NetSerialize(EmptyReader, nullptr, bSuccess);
	TestEqual(TEXT("an unset id stays unset"), EmptyCopy.TimelineIndex, static_cast<int32>(INDEX_NONE));
	TestFalse(TEXT("an empty relationship stays empty"), EmptyCopy.Relationship.IsValid());

	// The event payload is a view of the same context
	FGSTN_HitEventPayload Payload = FGSTN_HitEventPayload::FromContext(Source);
	FGameplayEventData EventData = Payload.MakeEventData(nullptr, nullptr, nullptr);
	TestTrue(TEXT("the gameplay event carries the event tag"), EventData.EventTag == GSTNCombatTags::Event_Hit_Received);
	FGSTN_HitEventPayload Read;
	TestTrue(TEXT("and the payload reads back from it"), FGSTN_HitEventPayload::FromEventData(EventData, Read));
	TestTrue(TEXT("with the same reaction, hurtbox and KO reason"), Read.Reaction == Payload.Reaction && Read.HurtboxIndex == 2 && Read.KOReason == Payload.KOReason && Read.bStaggered);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DamageMeterGainSetByCallerTest, "GSTNCombat.GAS.Damage.MeterGainSetByCaller", DamageTestFlags)
bool FGSTN_DamageMeterGainSetByCallerTest::RunTest(const FString& Parameters)
{
	GSTNCombatGASTest::FRig Rig;
	const FGameplayAttribute Ex = UGSTN_MeterAttributeSet::GetExMeterAttribute();
	const float Max = DamageBase(*Rig.ASC, UGSTN_MeterAttributeSet::GetExMeterMaxAttribute());
	TestTrue(TEXT("the meter starts empty and has room"), Max > 40.f && DamageBase(*Rig.ASC, Ex) == 0.f);

	auto Gain = [&Rig](float Amount)
	{
		FGameplayEffectSpecHandle Spec = Rig.ASC->MakeOutgoingSpec(UGSTN_TestExMeterGain::StaticClass(), 1.f, Rig.ASC->MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatTags::SetByCaller_Meter, Amount);
		Rig.ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	};

	Gain(40.f);
	TestEqual(TEXT("the meter gains the set-by-caller amount"), DamageBase(*Rig.ASC, Ex), 40.f);
	Gain(10.f);
	TestEqual(TEXT("and gains again on top"), DamageBase(*Rig.ASC, Ex), 50.f);
	Gain(Max * 10.f);
	TestEqual(TEXT("an overfill stops at the maximum"), DamageBase(*Rig.ASC, Ex), Max);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DamageRulesComponentHostAppliesWithoutOnRepTest, "GSTNCombat.GAS.Damage.RulesComponentHostAppliesWithoutOnRep", DamageTestFlags)
bool FGSTN_DamageRulesComponentHostAppliesWithoutOnRepTest::RunTest(const FString& Parameters)
{
	GSTNCombatGASTest::FRig Rig;
	UGSTN_CombatRulesComponent* Component = NewObject<UGSTN_CombatRulesComponent>(Rig.ASC->GetOwner());
	Component->RegisterComponent();

	// Nothing named: the rules of the settings, resolved on demand
	const UGSTN_CombatRules* SettingsRules = UGSTN_CombatSettings::Get().ResolveDefaultRules();
	TestTrue(TEXT("an empty reference plays the settings' rules"), Component->GetRules() == SettingsRules && SettingsRules != nullptr);

	// The authority picks an asset; nothing here calls OnRep_RulesAsset, the host never receives the replication of its own write
	const TStrongObjectPtr<UGSTN_CombatRules> Chosen = MakeRules(EGSTN_DamageMode::Health);
	const int32 Before = Component->GetApplyCount();
	Component->SetRulesAsset(TSoftObjectPtr<UGSTN_CombatRules>(Chosen.Get()));
	TestEqual(TEXT("the rules were resolved by the call itself"), Component->GetApplyCount(), Before + 1);
	TestTrue(TEXT("and are the chosen asset"), Component->GetRules() == Chosen.Get());
	TestTrue(TEXT("with its damage mode"), Component->GetRules()->DamageMode == EGSTN_DamageMode::Health);

	// Clearing the reference goes back to the settings, through the same path
	Component->SetRulesAsset(TSoftObjectPtr<UGSTN_CombatRules>());
	TestTrue(TEXT("an emptied reference plays the settings' rules again"), Component->GetRules() == SettingsRules);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DamageTagsRegisteredAndDocumentedTest, "GSTNCombat.GAS.Damage.TagsRegisteredAndDocumented", DamageTestFlags)
bool FGSTN_DamageTagsRegisteredAndDocumentedTest::RunTest(const FString& Parameters)
{
	TArray<FGameplayTag> Declared;
	GSTNCombatTags::GetDeclaredTags(Declared);
	const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	const TArray<FGameplayTag> HitTags = {
		GSTNCombatTags::SetByCaller_Damage, GSTNCombatTags::SetByCaller_Meter,
		GSTNCombatTags::Event_HitLanded, GSTNCombatTags::Event_HitReceived, GSTNCombatTags::Event_KO,
		GSTNCombatTags::Event_Hit_Received, GSTNCombatTags::Event_Impact,
		GSTNCombatTags::State_Tumble, GSTNCombatTags::Cue_Hit,
		GSTNCombatTags::KOReason_Health, GSTNCombatTags::KOReason_Zone };

	for (const FGameplayTag& Tag : HitTags)
	{
		const FString Name = Tag.ToString();
		TestTrue(*FString::Printf(TEXT("%s is registered"), *Name), Tag.IsValid());
		TestTrue(*FString::Printf(TEXT("%s is listed for the tag tests"), *Name), Declared.Contains(Tag));

		FString Comment;
		FName Source;
		bool bExplicit = false;
		bool bRestricted = false;
		bool bAllowNonRestrictedChildren = false;
		TestTrue(*FString::Printf(TEXT("%s is declared natively"), *Name),
			Manager.GetTagEditorData(Tag.GetTagName(), Comment, Source, bExplicit, bRestricted, bAllowNonRestrictedChildren) && bExplicit);
		TestFalse(*FString::Printf(TEXT("%s is documented"), *Name), Comment.TrimStartAndEnd().IsEmpty());
	}

	// Every event tag of the module lives under Combat.Event
	TestTrue(TEXT("Combat.Event.Hit.Received sits under Combat.Event"), GSTNCombatTags::Event_Hit_Received.GetTag().ToString().StartsWith(TEXT("Combat.Event.")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
