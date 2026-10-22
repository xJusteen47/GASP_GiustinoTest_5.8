#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "Attributes/GSTN_KitStats.h"
#include "Attributes/GSTN_MeterAttributeSet.h"
#include "Attributes/GSTN_MeterSetup.h"
#include "Effects/GSTN_GE_MeterCost.h"
#include "Effects/GSTN_GE_WeaponStats.h"
#include "GSTN_CombatTags.h"
#include "Settings/GSTN_CombatGASSettings.h"
#include "Tests/GSTN_GASTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	float Base(const UAbilitySystemComponent& ASC, const FGameplayAttribute& Attribute)
	{
		return ASC.GetNumericAttributeBase(Attribute);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttributesClampTest, "GSTNCombat.GAS.Attributes.Clamp", TestFlags)
bool FGSTN_AttributesClampTest::RunTest(const FString& Parameters)
{
	GSTNCombatGASTest::FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;

	// Starting values come from the settings, not from code
	for (const FGSTN_AttributeDefault& Default : UGSTN_CombatGASSettings::Get().AttributeDefaults)
	{
		if (Default.Attribute == FName(TEXT("WalkSpeed")))
		{
			TestEqual(TEXT("WalkSpeed starts at the configured default"), Base(ASC, UGSTN_CombatAttributeSet::GetWalkSpeedAttribute()), Default.Value);
		}
		if (Default.Attribute == FName(TEXT("ExMeterMax")))
		{
			TestEqual(TEXT("ExMeterMax starts at the configured default"), Base(ASC, UGSTN_MeterAttributeSet::GetExMeterMaxAttribute()), Default.Value);
		}
	}

	// Stats: never below zero, gravity scales never below the floor
	ASC.SetNumericAttributeBase(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), -50.f);
	TestEqual(TEXT("a negative speed clamps to 0"), Base(ASC, UGSTN_CombatAttributeSet::GetWalkSpeedAttribute()), 0.f);
	ASC.SetNumericAttributeBase(UGSTN_CombatAttributeSet::GetJumpRiseGravityScaleAttribute(), 0.f);
	TestEqual(TEXT("a zero gravity scale clamps to the floor"), Base(ASC, UGSTN_CombatAttributeSet::GetJumpRiseGravityScaleAttribute()), UGSTN_CombatAttributeSet::MinGravityScale);
	ASC.SetNumericAttributeBase(UGSTN_CombatAttributeSet::GetFastFallGravityScaleAttribute(), 3.f);
	TestEqual(TEXT("a valid gravity scale is untouched"), Base(ASC, UGSTN_CombatAttributeSet::GetFastFallGravityScaleAttribute()), 3.f);

	// The aggregated (current) value clamps too, not only the base
	ASC.SetNumericAttributeBase(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), 600.f);
	FGSTN_StatValue Drain;
	Drain.Stat = GSTNCombatGASTest::Stat_TestSpeed;
	Drain.Value = -9999.f;
	UGSTN_GE_WeaponStats::Apply(ASC, UGSTN_TestWeaponStatsA::StaticClass(), MakeArrayView(&Drain, 1));
	TestEqual(TEXT("an effect cannot push the current speed below 0"), ASC.GetNumericAttribute(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute()), 0.f);
	TestEqual(TEXT("the base is untouched by an infinite effect"), Base(ASC, UGSTN_CombatAttributeSet::GetWalkSpeedAttribute()), 600.f);
	UGSTN_GE_WeaponStats::Remove(ASC, UGSTN_TestWeaponStatsA::StaticClass());

	const UGSTN_CombatAttributeSet* Stats = ASC.GetSet<UGSTN_CombatAttributeSet>();
	if (TestNotNull(TEXT("the combat set is on the component"), Stats))
	{
		ASC.SetNumericAttributeBase(UGSTN_CombatAttributeSet::GetMaxJumpHoldFramesAttribute(), 11.9f);
		TestEqual(TEXT("frame counts are rounded down when read"), Stats->GetMaxJumpHoldFrameCount(), 11);
	}

	// Meters: [0, Max]
	const FGameplayAttribute ExMeter = UGSTN_MeterAttributeSet::GetExMeterAttribute();
	const FGameplayAttribute ExMax = UGSTN_MeterAttributeSet::GetExMeterMaxAttribute();
	ASC.SetNumericAttributeBase(ExMax, 100.f);
	ASC.SetNumericAttributeBase(ExMeter, 150.f);
	TestEqual(TEXT("a meter clamps to its maximum"), Base(ASC, ExMeter), 100.f);
	ASC.SetNumericAttributeBase(ExMeter, -5.f);
	TestEqual(TEXT("a meter clamps to 0"), Base(ASC, ExMeter), 0.f);
	ASC.SetNumericAttributeBase(ExMax, -1.f);
	TestEqual(TEXT("a maximum clamps to 0"), Base(ASC, ExMax), 0.f);

	// Both meters of the pair table are independent
	const FGameplayAttribute SuperMeter = UGSTN_MeterAttributeSet::GetSuperMeterAttribute();
	ASC.SetNumericAttributeBase(UGSTN_MeterAttributeSet::GetSuperMeterMaxAttribute(), 300.f);
	ASC.SetNumericAttributeBase(SuperMeter, 250.f);
	TestEqual(TEXT("the super meter is kept below its own maximum, not the EX one"), Base(ASC, SuperMeter), 250.f);

	// A lowered maximum pulls the meter down with it (instant effect path)
	ASC.SetNumericAttributeBase(ExMax, 100.f);
	ASC.SetNumericAttributeBase(ExMeter, 100.f);
	FGSTN_StatValue NewMax;
	NewMax.Stat = GSTNCombatGASTest::Stat_TestMeterMax;
	NewMax.Value = 40.f;
	FGSTN_KitStats KitStats;
	KitStats.Values = { NewMax, { GSTNCombatGASTest::Stat_TestSpeed, 600.f } };
	KitStats.ApplyTo(ASC, UGSTN_TestKitInitEffect::StaticClass());
	TestEqual(TEXT("the maximum was lowered"), Base(ASC, ExMax), 40.f);
	TestEqual(TEXT("the meter followed the lowered maximum"), Base(ASC, ExMeter), 40.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttributesKitStatsInitEffectTest, "GSTNCombat.GAS.Attributes.KitStatsInitEffect", TestFlags)
bool FGSTN_AttributesKitStatsInitEffectTest::RunTest(const FString& Parameters)
{
	GSTNCombatGASTest::FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;

	FGSTN_KitStats Kit;
	Kit.Values.Add({ GSTNCombatGASTest::Stat_TestSpeed, 777.f });
	Kit.Values.Add({ GSTNCombatGASTest::Stat_TestMeterMax, 250.f });

	TArray<FText> Errors;
	TestTrue(TEXT("two distinct leaves validate"), Kit.Validate(Errors));
	TestEqual(TEXT("no errors"), Errors.Num(), 0);

	TestFalse(TEXT("a null init effect applies nothing"), Kit.ApplyTo(ASC, nullptr).IsValid());

	const FActiveGameplayEffectHandle Handle = Kit.ApplyTo(ASC, UGSTN_TestKitInitEffect::StaticClass());
	TestTrue(TEXT("the instant effect ran"), Handle.WasSuccessfullyApplied());
	TestEqual(TEXT("walk speed comes from the kit number"), Base(ASC, UGSTN_CombatAttributeSet::GetWalkSpeedAttribute()), 777.f);
	TestEqual(TEXT("the meter maximum comes from the kit number"), Base(ASC, UGSTN_MeterAttributeSet::GetExMeterMaxAttribute()), 250.f);

	// Changing a number changes the result with no change to the effect
	Kit.Values[0].Value = 500.f;
	Kit.ApplyTo(ASC, UGSTN_TestKitInitEffect::StaticClass());
	TestEqual(TEXT("a re-applied kit overwrites the speed"), Base(ASC, UGSTN_CombatAttributeSet::GetWalkSpeedAttribute()), 500.f);

	// Validation
	Errors.Reset();
	FGSTN_KitStats Bad;
	Bad.Values.Add({ GSTNCombatGASTest::Stat_TestSpeed, 1.f });
	Bad.Values.Add({ GSTNCombatGASTest::Stat_TestSpeed, 2.f });
	Bad.Values.Add({ FGameplayTag(), 3.f });
	Bad.Values.Add({ GSTNCombatTags::Root_SetByCallerStat, 4.f });
	Bad.Values.Add({ GSTNCombatTags::SetByCaller_MeterCost, 5.f });
	TestFalse(TEXT("duplicates, an empty tag, the root itself and a tag outside the root are rejected"), Bad.Validate(Errors));
	TestEqual(TEXT("one error per defect"), Errors.Num(), 4);

	// The meter setup is addressed by tag and counts bars from its own divisor
	FGSTN_MeterSetup Setup;
	Setup.Meter = GSTNCombatTags::Root_Meter;
	Setup.Value = UGSTN_MeterAttributeSet::GetExMeterAttribute();
	Setup.Max = UGSTN_MeterAttributeSet::GetExMeterMaxAttribute();
	Setup.PointsPerBar = 50.f;
	Setup.Bars = 5;
	TestTrue(TEXT("a complete meter setup is valid"), Setup.IsValid());
	TestEqual(TEXT("bars round down"), UGSTN_MeterUtils::GetFullBars(Setup, 149.f), 2);
	FGSTN_MeterSetup Found;
	TestTrue(TEXT("a setup is found by its tag"), UGSTN_MeterUtils::FindMeterSetup({ Setup }, GSTNCombatTags::Root_Meter, Found));
	TestFalse(TEXT("an unknown tag finds nothing"), UGSTN_MeterUtils::FindMeterSetup({ Setup }, GSTNCombatTags::Root_Stat, Found));
	TestEqual(TEXT("the meter maximum is read through the setup"), UGSTN_MeterUtils::GetMeterMax(&ASC, Setup), 250.f);
	Setup.Max = Setup.Value;
	TestFalse(TEXT("a meter whose value and maximum are the same attribute is invalid"), Setup.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttributesMeterCostSetByCallerTest, "GSTNCombat.GAS.Attributes.MeterCostSetByCaller", TestFlags)
bool FGSTN_AttributesMeterCostSetByCallerTest::RunTest(const FString& Parameters)
{
	GSTNCombatGASTest::FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;
	const FGameplayAttribute ExMeter = UGSTN_MeterAttributeSet::GetExMeterAttribute();
	ASC.SetNumericAttributeBase(UGSTN_MeterAttributeSet::GetExMeterMaxAttribute(), 100.f);
	ASC.SetNumericAttributeBase(ExMeter, 60.f);

	const UGSTN_GE_MeterCost* Cost = GetDefault<UGSTN_TestExMeterCost>();
	TestEqual(TEXT("the pick builds exactly one modifier"), Cost->Modifiers.Num(), 1);
	if (Cost->Modifiers.Num() == 1)
	{
		TestTrue(TEXT("the modifier targets the picked meter"), Cost->Modifiers[0].Attribute == ExMeter);
		TestEqual(TEXT("the modifier is additive"), static_cast<int32>(Cost->Modifiers[0].ModifierOp), static_cast<int32>(EGameplayModOp::Additive));
	}
	TestEqual(TEXT("the cost is instant"), static_cast<int32>(Cost->DurationPolicy), static_cast<int32>(EGameplayEffectDurationType::Instant));

	auto Spend = [&ASC](float Amount)
	{
		FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(UGSTN_TestExMeterCost::StaticClass(), 1.f, ASC.MakeEffectContext());
		UGSTN_GE_MeterCost::SetCost(*Spec.Data, Amount);
		ASC.ApplyGameplayEffectSpecToSelf(*Spec.Data);
	};

	Spend(25.f);
	TestEqual(TEXT("the magnitude from the caller is spent"), Base(ASC, ExMeter), 35.f);
	Spend(-10.f);
	TestEqual(TEXT("a negative cost spends nothing"), Base(ASC, ExMeter), 35.f);
	Spend(50.f);
	TestEqual(TEXT("overspending empties the meter and stops at 0"), Base(ASC, ExMeter), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AttributesWeaponStatsApplyRemoveByClassTest, "GSTNCombat.GAS.Attributes.WeaponStatsApplyRemoveByClass", TestFlags)
bool FGSTN_AttributesWeaponStatsApplyRemoveByClassTest::RunTest(const FString& Parameters)
{
	GSTNCombatGASTest::FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;
	const FGameplayAttribute WalkSpeed = UGSTN_CombatAttributeSet::GetWalkSpeedAttribute();
	ASC.SetNumericAttributeBase(WalkSpeed, 600.f);

	const TSubclassOf<UGSTN_GE_WeaponStats> ClassA = UGSTN_TestWeaponStatsA::StaticClass();
	const TSubclassOf<UGSTN_GE_WeaponStats> ClassB = UGSTN_TestWeaponStatsB::StaticClass();
	const FGSTN_StatValue PlusHundred{ GSTNCombatGASTest::Stat_TestSpeed, 100.f };
	const FGSTN_StatValue PlusFifty{ GSTNCombatGASTest::Stat_TestSpeed, 50.f };

	TestEqual(TEXT("the effect is infinite"), static_cast<int32>(GetDefault<UGSTN_GE_WeaponStats>()->DurationPolicy), static_cast<int32>(EGameplayEffectDurationType::Infinite));
	TestFalse(TEXT("a null class applies nothing"), UGSTN_GE_WeaponStats::Apply(ASC, nullptr, MakeArrayView(&PlusHundred, 1)).IsValid());

	const FActiveGameplayEffectHandle HandleA = UGSTN_GE_WeaponStats::Apply(ASC, ClassA, MakeArrayView(&PlusHundred, 1));
	TestTrue(TEXT("A applied"), HandleA.IsValid());
	TestEqual(TEXT("A adds its modifier"), ASC.GetNumericAttribute(WalkSpeed), 700.f);
	TestTrue(TEXT("A is reported applied"), UGSTN_GE_WeaponStats::IsApplied(ASC, ClassA));
	TestFalse(TEXT("B is not"), UGSTN_GE_WeaponStats::IsApplied(ASC, ClassB));

	const FActiveGameplayEffectHandle Again = UGSTN_GE_WeaponStats::Apply(ASC, ClassA, MakeArrayView(&PlusHundred, 1));
	TestTrue(TEXT("a repeated apply returns the live effect"), Again == HandleA);
	TestEqual(TEXT("a repeated apply does not stack"), ASC.GetNumericAttribute(WalkSpeed), 700.f);

	UGSTN_GE_WeaponStats::Apply(ASC, ClassB, MakeArrayView(&PlusFifty, 1));
	TestEqual(TEXT("B stacks with A as a different class"), ASC.GetNumericAttribute(WalkSpeed), 750.f);

	TestEqual(TEXT("removing A removes one effect"), UGSTN_GE_WeaponStats::Remove(ASC, ClassA), 1);
	TestEqual(TEXT("only B remains"), ASC.GetNumericAttribute(WalkSpeed), 650.f);
	TestFalse(TEXT("A is gone"), UGSTN_GE_WeaponStats::IsApplied(ASC, ClassA));
	TestTrue(TEXT("B is still applied"), UGSTN_GE_WeaponStats::IsApplied(ASC, ClassB));
	TestEqual(TEXT("removing A again removes nothing"), UGSTN_GE_WeaponStats::Remove(ASC, ClassA), 0);

	TestEqual(TEXT("removing B removes one effect"), UGSTN_GE_WeaponStats::Remove(ASC, ClassB), 1);
	TestEqual(TEXT("the base speed is back"), ASC.GetNumericAttribute(WalkSpeed), 600.f);

	// A swap, as the combat model does it: remove the old class, apply the new one
	UGSTN_GE_WeaponStats::Apply(ASC, ClassA, MakeArrayView(&PlusHundred, 1));
	UGSTN_GE_WeaponStats::Remove(ASC, ClassA);
	UGSTN_GE_WeaponStats::Apply(ASC, ClassB, MakeArrayView(&PlusFifty, 1));
	TestEqual(TEXT("after a swap only the new weapon applies"), ASC.GetNumericAttribute(WalkSpeed), 650.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
