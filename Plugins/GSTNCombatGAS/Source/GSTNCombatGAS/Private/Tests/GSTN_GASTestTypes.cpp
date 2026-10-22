#include "Tests/GSTN_GASTestTypes.h"
#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "Attributes/GSTN_MeterAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace GSTNCombatGASTest
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_TestSpeed, "Combat.SetByCaller.Stat.TestSpeed", "Automation test stat: walk speed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_TestMeterMax, "Combat.SetByCaller.Stat.TestMeterMax", "Automation test stat: EX meter maximum");

	namespace
	{
		FGameplayModifierInfo MakeStatModifier(const FGameplayAttribute& Attribute, EGameplayModOp::Type Op, const FGameplayTag& Stat)
		{
			FSetByCallerFloat Magnitude;
			Magnitude.DataTag = Stat;

			FGameplayModifierInfo Modifier;
			Modifier.Attribute = Attribute;
			Modifier.ModifierOp = Op;
			Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
			return Modifier;
		}
	}

	FRig::FRig()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNCombatGASTestWorld"));
		World->AddToRoot();
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());

		AActor* Owner = World->SpawnActor<AActor>();
		ASC = NewObject<UAbilitySystemComponent>(Owner);
		ASC->RegisterComponent();
		ASC->InitAbilityActorInfo(Owner, Owner);
		ASC->AddSet<UGSTN_CombatAttributeSet>();
		ASC->AddSet<UGSTN_MeterAttributeSet>();
	}

	FRig::~FRig()
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		World->RemoveFromRoot();
	}
}

UGSTN_TestKitInitEffect::UGSTN_TestKitInitEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	// The maximum first: the meter is clamped to the maximum that is current when it is written
	Modifiers.Add(GSTNCombatGASTest::MakeStatModifier(UGSTN_MeterAttributeSet::GetExMeterMaxAttribute(), EGameplayModOp::Override, GSTNCombatGASTest::Stat_TestMeterMax));
	Modifiers.Add(GSTNCombatGASTest::MakeStatModifier(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), EGameplayModOp::Override, GSTNCombatGASTest::Stat_TestSpeed));
}

UGSTN_TestExMeterCost::UGSTN_TestExMeterCost()
{
	Meter = UGSTN_MeterAttributeSet::GetExMeterAttribute();
	RebuildModifiers();
}

UGSTN_TestWeaponStatsA::UGSTN_TestWeaponStatsA()
{
	Modifiers.Add(GSTNCombatGASTest::MakeStatModifier(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), EGameplayModOp::Additive, GSTNCombatGASTest::Stat_TestSpeed));
}

UGSTN_TestWeaponStatsB::UGSTN_TestWeaponStatsB()
{
	Modifiers.Add(GSTNCombatGASTest::MakeStatModifier(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), EGameplayModOp::Additive, GSTNCombatGASTest::Stat_TestSpeed));
}
