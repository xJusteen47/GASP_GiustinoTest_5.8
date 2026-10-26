// Destructible parts, ability-system half (no Network Prediction, no Mover).

#include "Effects/GSTN_GEComponent_RestoreParts.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"

FName UGSTN_GEComponent_RestoreParts::GetFractionKey()
{
	static const FName Key(TEXT("GSTNParts.RestoreFraction"));
	return Key;
}

FGSTN_PartRestoreRequest UGSTN_GEComponent_RestoreParts::MakeRequest(const FGameplayEffectSpec& Spec) const
{
	FGSTN_PartRestoreRequest Request;
	const int32 Level = FMath::RoundToInt(Spec.GetLevel());
	Request.PartMask = Level > 1 ? FMath::Clamp(Level - 1, 0, 0xFFFF) : FMath::Clamp(PartMask, 0, 0xFFFF);
	Request.DamageFraction = Spec.GetSetByCallerMagnitude(GetFractionKey(), false, DamageFraction);
	return Request;
}

UGSTN_GE_PartRestore::UGSTN_GE_PartRestore(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	// A default subobject, not FindOrAddComponent: that one calls NewObject, which a constructor must not
	GEComponents.Add(ObjectInitializer.CreateDefaultSubobject<UGSTN_GEComponent_RestoreParts>(this, TEXT("RestoreParts")));
}

void FGSTN_PartRestoreQueue::Collect(const UAbilitySystemComponent& ASC, TArray<FGSTN_PendingPartRestore>& Out)
{
	Out.Reset();
	FGameplayEffectQuery Query;
	Query.CustomMatchDelegate.BindLambda([](const FActiveGameplayEffect& Active)
	{
		return Active.Spec.Def && Active.Spec.Def->FindComponent<UGSTN_GEComponent_RestoreParts>() != nullptr;
	});
	for (const FActiveGameplayEffectHandle& Handle : ASC.GetActiveEffects(Query))
	{
		const FActiveGameplayEffect* Active = ASC.GetActiveGameplayEffect(Handle);
		const UGSTN_GEComponent_RestoreParts* Component = Active && Active->Spec.Def ? Active->Spec.Def->FindComponent<UGSTN_GEComponent_RestoreParts>() : nullptr;
		if (!Component)
		{
			continue;
		}
		FGSTN_PendingPartRestore& Pending = Out.AddDefaulted_GetRef();
		Pending.Handle = Handle;
		Pending.Request = Component->MakeRequest(Active->Spec);
		Pending.Instigator = Active->Spec.GetContext().GetOriginalInstigator();
	}
}

int32 FGSTN_PartRestoreQueue::Consume(UAbilitySystemComponent& ASC, TConstArrayView<FGSTN_PendingPartRestore> Handled)
{
	int32 Removed = 0;
	for (const FGSTN_PendingPartRestore& Pending : Handled)
	{
		Removed += ASC.RemoveActiveGameplayEffect(Pending.Handle) ? 1 : 0;
	}
	return Removed;
}

bool FGSTN_PartRestoreQueue::Apply(UAbilitySystemComponent& ASC, TSubclassOf<UGameplayEffect> EffectClass, const FGSTN_PartRestoreRequest& Request, AActor* Instigator)
{
	const TSubclassOf<UGameplayEffect> Class = EffectClass ? EffectClass : TSubclassOf<UGameplayEffect>(UGSTN_GE_PartRestore::StaticClass());
	FGameplayEffectContextHandle Context = ASC.MakeEffectContext();
	if (Instigator)
	{
		Context.AddInstigator(Instigator, Instigator);
	}
	FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(Class, UGSTN_GEComponent_RestoreParts::LevelForMask(static_cast<uint16>(FMath::Clamp(Request.PartMask, 0, 0xFFFF))), Context);
	if (!Spec.IsValid())
	{
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(UGSTN_GEComponent_RestoreParts::GetFractionKey(), Request.DamageFraction);
	return ASC.ApplyGameplayEffectSpecToSelf(*Spec.Data).IsValid();
}
