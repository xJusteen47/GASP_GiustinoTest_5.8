// Destructible parts, ability-system half: test fixtures (see the header).

#include "Tests/GSTN_PartsGASTestTypes.h"

#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace GSTNPartsGASTest
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_PartsTestSpeed, "Combat.SetByCaller.Stat.PartsTestSpeed", "Automation test stat of the parts plugin: walk speed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_PartsTestFastFall, "Combat.SetByCaller.Stat.PartsTestFastFall", "Automation test stat of the parts plugin: fast fall gravity scale");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_PartsTestOther, "Combat.SetByCaller.Stat.PartsTestOther", "Automation test stat of the parts plugin: a key no modifier reads");

	namespace
	{
		FGameplayModifierInfo MakeStatModifier(const FGameplayAttribute& Attribute, const FGameplayTag& Stat)
		{
			FSetByCallerFloat Magnitude;
			Magnitude.DataTag = Stat;
			FGameplayModifierInfo Modifier;
			Modifier.Attribute = Attribute;
			Modifier.ModifierOp = EGameplayModOp::Additive;
			Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
			return Modifier;
		}
	}

	FRig::FRig()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNPartsGASTestWorld"));
		World->AddToRoot();
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		AActor* Owner = World->SpawnActor<AActor>();
		ASC = NewObject<UAbilitySystemComponent>(Owner);
		ASC->RegisterComponent();
		ASC->InitAbilityActorInfo(Owner, Owner);
		ASC->AddSet<UGSTN_CombatAttributeSet>();
	}

	FRig::~FRig()
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		World->RemoveFromRoot();
	}
}

UGSTN_TestPartStats::UGSTN_TestPartStats()
{
	Modifiers.Add(GSTNPartsGASTest::MakeStatModifier(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), GSTNPartsGASTest::Stat_PartsTestSpeed));
	Modifiers.Add(GSTNPartsGASTest::MakeStatModifier(UGSTN_CombatAttributeSet::GetFastFallGravityScaleAttribute(), GSTNPartsGASTest::Stat_PartsTestFastFall));
}
