// Links, Network Prediction half.

#include "Sim/GSTN_LinkEffects.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GSTN_LinkRegistry.h"
#include "Sim/GSTN_LinkFragment.h"

namespace
{
	/** The role effect a channel holds this frame: Active, a role, and a definition whose role names an effect */
	const FGSTN_LinkEffectDef* LiveEffect(const FGSTN_LinkFragment* Fragment, EGSTN_LinkChannel Channel, const UGSTN_LinkRegistry* Registry)
	{
		if (!Fragment || !Registry)
		{
			return nullptr;
		}
		const FGSTN_LinkState& State = Fragment->Get(Channel);
		if (!State.IsActive() || State.Role == 0)
		{
			return nullptr;
		}
		const UGSTN_LinkTypeDefinition* Definition = Registry->GetDefinition(State.LinkTypeIndex);
		const FGSTN_LinkEffectDef* Effect = Definition ? &Definition->GetRoleDef(State.Role).LinkEffect : nullptr;
		return Effect && Effect->IsSet() ? Effect : nullptr;
	}

	bool SameLink(const FGSTN_LinkState& A, const FGSTN_LinkState& B)
	{
		return A.LinkTypeIndex == B.LinkTypeIndex && A.Role == B.Role && A.Serial == B.Serial;
	}
}

void FGSTN_LinkEffects::ComputeOps(const FGSTN_LinkFragment* In, const FGSTN_LinkFragment* Out, const UGSTN_LinkRegistry* Registry,
	TArray<FOp, TInlineAllocator<2>>& OutOps)
{
	OutOps.Reset();
	TArray<FOp, TInlineAllocator<2>> Applies;
	for (const EGSTN_LinkChannel Channel : { EGSTN_LinkChannel::Body, EGSTN_LinkChannel::Hand })
	{
		const bool bWas = LiveEffect(In, Channel, Registry) != nullptr;
		const bool bIs = LiveEffect(Out, Channel, Registry) != nullptr;
		const bool bSame = bWas && bIs && SameLink(In->Get(Channel), Out->Get(Channel));
		if (bWas && !bSame)
		{
			const FGSTN_LinkState& Ended = In->Get(Channel);
			OutOps.Add({ false, Channel, Ended.LinkTypeIndex, Ended.Role, Ended.Payload });
		}
		if (bIs && !bSame)
		{
			const FGSTN_LinkState& Started = Out->Get(Channel);
			Applies.Add({ true, Channel, Started.LinkTypeIndex, Started.Role, Started.Payload });
		}
	}
	OutOps.Append(Applies);
}

void FGSTN_LinkEffects::Step(UAbilitySystemComponent* AbilitySystem, const FGSTN_LinkFragment* In, const FGSTN_LinkFragment* Out, const UGSTN_LinkRegistry* Registry)
{
	if (!AbilitySystem || !Registry || (!In && !Out))
	{
		return;
	}
	TArray<FOp, TInlineAllocator<2>> Ops;
	ComputeOps(In, Out, Registry, Ops);
	for (const FOp& Op : Ops)
	{
		const UGSTN_LinkTypeDefinition* Definition = Registry->GetDefinition(Op.LinkTypeIndex);
		if (!Definition)
		{
			continue;
		}
		const FGSTN_LinkEffectDef& Effect = Definition->GetRoleDef(Op.Role).LinkEffect;
		if (Op.bApply)
		{
			Apply(AbilitySystem, Effect, Op.Payload);
		}
		else
		{
			Remove(AbilitySystem, Effect);
		}
	}
}

TSubclassOf<UGameplayEffect> FGSTN_LinkEffects::ResolveEffectClass(const FGSTN_LinkEffectDef& Effect)
{
	if (!Effect.IsSet())
	{
		return nullptr;
	}
	// Content: a small class asset, loaded once and then found in memory. A path that is not a gameplay effect is ignored with a warning
	UClass* Class = Effect.Effect.ResolveClass();
	if (!Class)
	{
		Class = Effect.Effect.TryLoadClass<UGameplayEffect>();
	}
	if (Class && !Class->IsChildOf(UGameplayEffect::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("Link effect '%s' is not a gameplay effect: ignored"), *Effect.Effect.ToString());
		return nullptr;
	}
	return Class;
}

bool FGSTN_LinkEffects::Apply(UAbilitySystemComponent* AbilitySystem, const FGSTN_LinkEffectDef& Effect, uint8 Payload)
{
	const TSubclassOf<UGameplayEffect> Class = ResolveEffectClass(Effect);
	if (!AbilitySystem || !Class)
	{
		return false;
	}
	AbilitySystem->RemoveActiveGameplayEffectBySourceEffect(Class, nullptr);
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(Class, 1.f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return false;
	}
	if (Effect.SetByCallerTag.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(Effect.SetByCallerTag, Magnitude(Payload));
	}
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	return true;
}

void FGSTN_LinkEffects::Remove(UAbilitySystemComponent* AbilitySystem, const FGSTN_LinkEffectDef& Effect)
{
	const TSubclassOf<UGameplayEffect> Class = ResolveEffectClass(Effect);
	if (AbilitySystem && Class)
	{
		AbilitySystem->RemoveActiveGameplayEffectBySourceEffect(Class, nullptr);
	}
}
