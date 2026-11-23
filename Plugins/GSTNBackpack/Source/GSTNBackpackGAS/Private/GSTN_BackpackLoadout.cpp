// Backpacks, ability layer: what the backpack does to a loadout and to the pawn's look.

#include "GSTN_BackpackLoadout.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "GSTN_BackpackDefinition.h"
#include "GSTN_BackpackTypes.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "Cosmetics/GSTN_EquipmentVisual.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Loadout/GSTN_LoadoutRules.h"

const FName FGSTN_BackpackLoadout::ExtensionId(TEXT("GSTN.Backpack"));
const FName FGSTN_BackpackLoadout::AttachmentProviderId(TEXT("GSTN.Backpack"));
const FName FGSTN_BackpackLoadout::ExtraAirJumpsName(TEXT("GSTN.Backpack.ExtraAirJumps"));
const FName FGSTN_BackpackLoadout::GlideGravityScaleName(TEXT("GSTN.Backpack.GlideGravityScale"));
const FName FGSTN_BackpackLoadout::GlideMaxFallSpeedName(TEXT("GSTN.Backpack.GlideMaxFallSpeed"));

UGSTN_GE_BackpackMovement::UGSTN_GE_BackpackMovement()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	const auto AddByCaller = [this](const FGameplayAttribute& Attribute, FName Name)
	{
		FSetByCallerFloat ByCaller;
		ByCaller.DataName = Name;
		FGameplayModifierInfo Info;
		Info.Attribute = Attribute;
		Info.ModifierOp = EGameplayModOp::AddBase;
		Info.ModifierMagnitude = FGameplayEffectModifierMagnitude(ByCaller);
		Modifiers.Add(Info);
	};
	AddByCaller(UGSTN_CombatAttributeSet::GetAirJumpCountAttribute(), FGSTN_BackpackLoadout::ExtraAirJumpsName);
	AddByCaller(UGSTN_CombatAttributeSet::GetGlideGravityScaleAttribute(), FGSTN_BackpackLoadout::GlideGravityScaleName);
	AddByCaller(UGSTN_CombatAttributeSet::GetGlideMaxFallSpeedAttribute(), FGSTN_BackpackLoadout::GlideMaxFallSpeedName);
}

namespace
{
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, TArray<FActiveGameplayEffectHandle>>& GetAppliedEffects()
	{
		static TMap<TWeakObjectPtr<UAbilitySystemComponent>, TArray<FActiveGameplayEffectHandle>> Applied;
		return Applied;
	}
}

const UGSTN_BackpackDefinition* FGSTN_BackpackLoadout::ResolveBackpack(const FGSTN_LoadoutData& Loadout, TArray<FString>* OutWarnings)
{
	const FSoftObjectPath Path = FGSTN_BackpackLoadoutRules::GetBackpack(Loadout);
	if (!Path.IsValid())
	{
		return nullptr;
	}
	const UGSTN_BackpackDefinition* Backpack = Cast<UGSTN_BackpackDefinition>(Path.TryLoad());
	if (!Backpack && OutWarnings)
	{
		OutWarnings->Add(FString::Printf(TEXT("Backpack: %s is not a backpack asset; carried as nothing"), *Path.ToString()));
	}
	return Backpack;
}

void FGSTN_BackpackLoadout::ContributeCompileInput(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input)
{
	const UGSTN_BackpackDefinition* Backpack = Context.Resolved ? ResolveBackpack(Context.Resolved->Loadout) : nullptr;
	if (!Backpack)
	{
		return;
	}
	for (const FGSTN_AbilityBinding& Binding : Backpack->Abilities)
	{
		Input.Kit.Bindings.Add(FGSTN_LoadoutAssembly::MakeBindingInput(Binding));
	}
	for (const TSoftClassPtr<UGameplayAbility>& Passive : Backpack->Passives)
	{
		if (!Passive.IsNull())
		{
			Input.Kit.Passives.AddUnique(Passive.ToSoftObjectPath());
		}
	}
}

void FGSTN_BackpackLoadout::ContributeRulesView(const FGSTN_LoadoutExtensionContext& Context, FGSTN_LoadoutRulesView& View)
{
	const UGSTN_BackpackDefinition* Backpack = ResolveBackpack(View.Loadout, &View.Table.Warnings);
	if (!Backpack || Backpack->CompatibleSupportWeapons.IsEmpty())
	{
		return;
	}
	// A feeding backpack without the weapon it feeds is allowed (the player may pick one up later): a note, never an error
	bool bFeedsOne = false;
	for (int32 Index = 1; Index < View.Weapons.Num() && !bFeedsOne; ++Index)
	{
		bFeedsOne = Backpack->CompatibleSupportWeapons.Matches(View.Weapons[Index].Tags);
	}
	if (!bFeedsOne)
	{
		View.Table.Warnings.Add(FString::Printf(TEXT("Backpack: %s feeds no carried support weapon"), *Backpack->GetName()));
	}
}

void FGSTN_BackpackLoadout::OnGranted(const FGSTN_LoadoutExtensionContext& Context, UAbilitySystemComponent& AbilitySystem)
{
	const UGSTN_BackpackDefinition* Backpack = Context.Resolved ? ResolveBackpack(Context.Resolved->Loadout) : nullptr;
	if (!Backpack)
	{
		return;
	}
	TArray<FActiveGameplayEffectHandle>& Handles = GetAppliedEffects().FindOrAdd(TWeakObjectPtr<UAbilitySystemComponent>(&AbilitySystem));
	if (!Backpack->Movement.IsNone())
	{
		const FGameplayEffectSpecHandle Spec = AbilitySystem.MakeOutgoingSpec(UGSTN_GE_BackpackMovement::StaticClass(), 1.f, AbilitySystem.MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(ExtraAirJumpsName, static_cast<float>(Backpack->Movement.ExtraAirJumps));
			Spec.Data->SetSetByCallerMagnitude(GlideGravityScaleName, Backpack->Movement.GlideGravityScale);
			Spec.Data->SetSetByCallerMagnitude(GlideMaxFallSpeedName, Backpack->Movement.GlideMaxFallSpeed);
			const FActiveGameplayEffectHandle Handle = AbilitySystem.ApplyGameplayEffectSpecToSelf(*Spec.Data);
			if (Handle.IsValid())
			{
				Handles.Add(Handle);
			}
		}
	}
	for (const TSubclassOf<UGameplayEffect>& Effect : Backpack->Effects)
	{
		if (!Effect)
		{
			continue;
		}
		const FGameplayEffectSpecHandle Spec = AbilitySystem.MakeOutgoingSpec(Effect, 1.f, AbilitySystem.MakeEffectContext());
		if (!Spec.IsValid())
		{
			continue;
		}
		const FActiveGameplayEffectHandle Handle = AbilitySystem.ApplyGameplayEffectSpecToSelf(*Spec.Data);
		if (Handle.IsValid())
		{
			Handles.Add(Handle);
		}
	}
}

void FGSTN_BackpackLoadout::OnCleared(UAbilitySystemComponent& AbilitySystem)
{
	TArray<FActiveGameplayEffectHandle> Handles;
	if (GetAppliedEffects().RemoveAndCopyValue(TWeakObjectPtr<UAbilitySystemComponent>(&AbilitySystem), Handles))
	{
		for (const FActiveGameplayEffectHandle& Handle : Handles)
		{
			AbilitySystem.RemoveActiveGameplayEffect(Handle);
		}
	}
}

void FGSTN_BackpackLoadout::GatherVisual(const UGSTN_BackpackDefinition& Backpack, TArray<FGSTN_AttachmentRequest>& OutRequests)
{
	for (const FGSTN_AttachmentRequest& Part : Backpack.Visual.Parts)
	{
		FGSTN_AttachmentRequest& Request = OutRequests.Add_GetRef(Part);
		if (Request.Socket.IsNone() && Request.ParentSlot.IsNone())
		{
			Request.ParentSlot = GSTNEquipment::Slot_ImplantBack;
			Request.Socket = GSTNEquipment::Socket_ConnectorBackpack;
		}
	}
}

void FGSTN_BackpackAttachmentProvider::GatherAttachments(const FGSTN_AttachmentContext& Context, TArray<FGSTN_AttachmentRequest>& OutRequests) const
{
	if (!Context.Resolved)
	{
		return;
	}
	if (const UGSTN_BackpackDefinition* Backpack = FGSTN_BackpackLoadout::ResolveBackpack(Context.Resolved->Loadout))
	{
		FGSTN_BackpackLoadout::GatherVisual(*Backpack, OutRequests);
	}
}
