// Spin and drill, test module: harness helpers (never in Shipping).

#include "GSTN_SpinDrillTestLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GSTN_SpinDrillTags.h"

UGSTN_GE_TestDrillImmunity::UGSTN_GE_TestDrillImmunity()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer Tags;
	Tags.Added.AddTag(GSTNSpinDrillTags::Immunity_Drill);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
	GEComponents.Add(TargetTags);
}

bool UGSTN_SpinDrillTestLibrary::ServerSetDrillImmunity(AActor* Target, bool bImmune)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!ASC || !Target->HasAuthority())
	{
		return false;
	}
	ASC->RemoveActiveGameplayEffectBySourceEffect(UGSTN_GE_TestDrillImmunity::StaticClass(), nullptr);
	if (!bImmune)
	{
		return true;
	}
	return ASC->ApplyGameplayEffectToSelf(GetDefault<UGSTN_GE_TestDrillImmunity>(), 1.f, ASC->MakeEffectContext()).IsValid();
}
