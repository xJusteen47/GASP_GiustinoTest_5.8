// Kit mods, ability layer: what the mods do to a loadout.

#include "GSTN_KitModLoadout.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "GSTN_KitModBaking.h"
#include "GSTN_KitModDefinition.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Loadout/GSTN_LoadoutRules.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "UObject/GCObject.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNKitMods, Log, All);

const FName FGSTN_KitModLoadout::ExtensionId(TEXT("GSTN.KitMods"));

namespace
{
	/** Keeps the derived timelines alive: the compiled table names them by path only */
	class FDerivedTimelineKeeper : public FGCObject
	{
	public:
		TMap<FName, TObjectPtr<UGSTN_CombatTimeline>> Timelines;

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Collector.AddReferencedObjects(Timelines); }
		virtual FString GetReferencerName() const override { return TEXT("GSTN_KitModDerivedTimelines"); }
	};

	FDerivedTimelineKeeper& GetKeeper()
	{
		static FDerivedTimelineKeeper Keeper;
		return Keeper;
	}

	TMap<TWeakObjectPtr<UAbilitySystemComponent>, TArray<FActiveGameplayEffectHandle>>& GetAppliedEffects()
	{
		static TMap<TWeakObjectPtr<UAbilitySystemComponent>, TArray<FActiveGameplayEffectHandle>> Applied;
		return Applied;
	}

	/** The kit's tags plus its primary weapon's: what CompatibleKits is matched against */
	FGameplayTagContainer GetKitMatchTags(const FGSTN_ResolvedLoadout& Resolved)
	{
		FGameplayTagContainer Tags;
		if (Resolved.Kit)
		{
			Tags.AppendTags(Resolved.Kit->KitTags);
		}
		if (Resolved.Weapons.Num() > 0 && Resolved.Weapons[0])
		{
			Tags.AppendTags(Resolved.Weapons[0]->WeaponTags);
		}
		return Tags;
	}

	/** Ability asset tags plus the command: what a move filter is matched against */
	FGameplayTagContainer GetMoveTags(const FGSTN_BindingInput& Binding)
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(Binding.Command);
		if (const UClass* Class = TSoftClassPtr<UGameplayAbility>(Binding.Ability).LoadSynchronous())
		{
			if (const UGameplayAbility* Default = Class->GetDefaultObject<UGameplayAbility>())
			{
				Tags.AppendTags(Default->GetAssetTags());
			}
		}
		return Tags;
	}
}

void FGSTN_KitModLoadout::ResolveMods(const FGSTN_ResolvedLoadout& Resolved, FGSTN_ResolvedMods& Out)
{
	Out = FGSTN_ResolvedMods();
	const FGSTN_LoadoutModsExt* Ext = GSTNLoadoutExtension::Find<FGSTN_LoadoutModsExt>(Resolved.Loadout.Extensions);
	if (!Ext || !Resolved.Kit)
	{
		return;
	}
	const FGSTN_KitModConfig* Config = FGSTN_KitModBaking::SelectConfig(*Ext, Resolved.Kit->KitTags);
	if (!Config || Config->Mods.IsEmpty())
	{
		return;
	}
	Out.KitSlots = Resolved.Kit->Extensions.Find<FGSTN_KitModSlots>();
	if (!Out.KitSlots)
	{
		Out.Warnings.Add(FString::Printf(TEXT("Mods: kit %s takes no mod (no Kit mod slots extension); %d mod(s) ignored"), *Resolved.Kit->GetName(), Config->Mods.Num()));
		return;
	}

	// Each kit slot holds one mod: a mod takes the first free slot of its type, in the order the player listed them
	const FGameplayTagContainer KitTags = GetKitMatchTags(Resolved);
	TArray<const UGSTN_KitModDefinition*> BySlot;
	TArray<FSoftObjectPath> IdsBySlot;
	BySlot.Init(nullptr, Out.KitSlots->Slots.Num());
	IdsBySlot.SetNum(Out.KitSlots->Slots.Num());
	for (const FGSTN_EquippedMod& Equipped : Config->Mods)
	{
		const UGSTN_KitModDefinition* Mod = Cast<UGSTN_KitModDefinition>(Equipped.Mod.TryLoad());
		if (!Mod)
		{
			Out.Warnings.Add(FString::Printf(TEXT("Mods: %s is not a kit mod asset; ignored"), *Equipped.Mod.ToString()));
			continue;
		}
		if (!Mod->CompatibleKits.IsEmpty() && !Mod->CompatibleKits.Matches(KitTags))
		{
			Out.Warnings.Add(FString::Printf(TEXT("Mods: %s does not fit kit %s; ignored"), *Mod->GetName(), *Resolved.Kit->GetName()));
			continue;
		}
		const FGameplayTag SlotType = Equipped.Slot.IsValid() ? Equipped.Slot : Mod->Slot;
		if (SlotType != Mod->Slot)
		{
			Out.Warnings.Add(FString::Printf(TEXT("Mods: %s fits %s, not %s; ignored"), *Mod->GetName(), *Mod->Slot.ToString(), *SlotType.ToString()));
			continue;
		}
		int32 Free = INDEX_NONE;
		for (int32 Index = 0; Index < Out.KitSlots->Slots.Num() && Free == INDEX_NONE; ++Index)
		{
			Free = Out.KitSlots->Slots[Index] == SlotType && !BySlot[Index] ? Index : INDEX_NONE;
		}
		if (Free == INDEX_NONE)
		{
			Out.Warnings.Add(FString::Printf(TEXT("Mods: no free %s slot on kit %s for %s; ignored"), *SlotType.ToString(), *Resolved.Kit->GetName(), *Mod->GetName()));
			continue;
		}
		BySlot[Free] = Mod;
		IdsBySlot[Free] = Equipped.Mod;
	}
	for (int32 Index = 0; Index < BySlot.Num(); ++Index)
	{
		if (BySlot[Index])
		{
			Out.Mods.Add(BySlot[Index]);
			Out.ModIds.Add(IdsBySlot[Index]);
		}
	}
}

bool FGSTN_KitModLoadout::FindBaseTimeline(const FSoftObjectPath& Ability, const FSoftObjectPath& TimelineOverride, FGSTN_CombatTimelineData& OutData)
{
	if (TimelineOverride.IsValid())
	{
		if (const UGSTN_CombatTimeline* Timeline = Cast<UGSTN_CombatTimeline>(TimelineOverride.TryLoad()))
		{
			OutData = Timeline->Data;
			return true;
		}
		return false;
	}
	const UClass* Class = TSoftClassPtr<UGameplayAbility>(Ability).LoadSynchronous();
	const UObject* Default = Class ? Class->GetDefaultObject() : nullptr;
	if (!Default)
	{
		return false;
	}
	// UGSTN_GA_BaseCombat::GetTimelineData, read by name: TimelineAsset wins over the inline Timeline
	if (const FSoftObjectProperty* AssetProperty = CastField<FSoftObjectProperty>(Class->FindPropertyByName(TEXT("TimelineAsset"))))
	{
		const FSoftObjectPtr& Soft = *AssetProperty->ContainerPtrToValuePtr<FSoftObjectPtr>(Default);
		if (!Soft.IsNull())
		{
			if (const UGSTN_CombatTimeline* Timeline = Cast<UGSTN_CombatTimeline>(Soft.LoadSynchronous()))
			{
				OutData = Timeline->Data;
				return true;
			}
		}
	}
	const FStructProperty* InlineProperty = CastField<FStructProperty>(Class->FindPropertyByName(TEXT("Timeline")));
	if (InlineProperty && InlineProperty->Struct == FGSTN_CombatTimelineData::StaticStruct())
	{
		OutData = *InlineProperty->ContainerPtrToValuePtr<FGSTN_CombatTimelineData>(Default);
		return true;
	}
	return false;
}

UGSTN_CombatTimeline* FGSTN_KitModLoadout::FindOrBakeDerived(const FSoftObjectPath& BasePath, const FGSTN_CombatTimelineData& Base, TConstArrayView<FSoftObjectPath> ModIds,
	const FGSTN_CombinedMoveModifier& Modifier)
{
	const FName Name = FGSTN_KitModBaking::MakeDerivedName(BasePath, ModIds);
	TObjectPtr<UGSTN_CombatTimeline>& Slot = GetKeeper().Timelines.FindOrAdd(Name);
	if (!Slot)
	{
		Slot = FindObject<UGSTN_CombatTimeline>(GetTransientPackage(), *Name.ToString());
	}
	if (!Slot)
	{
		Slot = NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), Name, RF_Transient);
	}
	FGSTN_KitModBaking::BakeTimeline(Base, Modifier, Slot->Data);
	return Slot;
}

void FGSTN_KitModLoadout::ResetDerivedCache()
{
	GetKeeper().Timelines.Reset();
}

void FGSTN_KitModLoadout::ContributeCompileInput(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input)
{
	if (!Context.Resolved)
	{
		return;
	}
	FGSTN_ResolvedMods Resolved;
	ResolveMods(*Context.Resolved, Resolved);
	if (Resolved.Mods.IsEmpty())
	{
		return;
	}

	// 1. Verbs: removed commands first (from every source), then the mods' own bindings and passives on the kit
	FGameplayTagContainer Removed;
	for (const UGSTN_KitModDefinition* Mod : Resolved.Mods)
	{
		Removed.AppendTags(Mod->RemovedCommands);
	}
	if (!Removed.IsEmpty())
	{
		auto IsRemoved = [&Removed](const FGSTN_BindingInput& Binding) { return Removed.HasTagExact(Binding.Command); };
		Input.Kit.Bindings.RemoveAll(IsRemoved);
		for (FGSTN_WeaponCompileInput& Weapon : Input.Weapons)
		{
			Weapon.Bindings.RemoveAll(IsRemoved);
		}
	}
	for (const UGSTN_KitModDefinition* Mod : Resolved.Mods)
	{
		for (const FGSTN_AbilityBinding& Binding : Mod->AddedBindings)
		{
			Input.Kit.Bindings.Add(FGSTN_LoadoutAssembly::MakeBindingInput(Binding));
		}
		for (const TSoftClassPtr<UGameplayAbility>& Passive : Mod->Passives)
		{
			if (!Passive.IsNull())
			{
				Input.Kit.Passives.AddUnique(Passive.ToSoftObjectPath());
			}
		}
	}

	// 2. Moves: every binding a modifier selects starts a derived timeline instead of its base
	const int32 MaxDerived = Resolved.KitSlots->MaxDerivedTimelines;
	TSet<FName> Derived;
	bool bCapWarned = false;
	auto ModifyBinding = [&](FGSTN_BindingInput& Binding)
	{
		if (!Binding.Ability.IsValid())
		{
			return;
		}
		const FGameplayTagContainer MoveTags = GetMoveTags(Binding);
		TArray<const FGSTN_MoveModifier*> Selected;
		for (const UGSTN_KitModDefinition* Mod : Resolved.Mods)
		{
			for (const FGSTN_MoveModifier& Modifier : Mod->MoveModifiers)
			{
				if (FGSTN_KitModBaking::MatchesFilter(Modifier.MoveFilter, MoveTags))
				{
					Selected.Add(&Modifier);
				}
			}
		}
		if (Selected.IsEmpty())
		{
			return;
		}
		const FGSTN_CombinedMoveModifier Combined = FGSTN_KitModBaking::Combine(Selected, Resolved.KitSlots->Limits);
		FGSTN_CombatTimelineData Base;
		if (Combined.IsIdentity() || !FindBaseTimeline(Binding.Ability, Binding.TimelineOverride, Base))
		{
			return;
		}
		const FSoftObjectPath BasePath = Binding.TimelineOverride.IsValid() ? Binding.TimelineOverride : Binding.Ability;
		const FName Name = FGSTN_KitModBaking::MakeDerivedName(BasePath, Resolved.ModIds);
		if (!Derived.Contains(Name) && Derived.Num() >= MaxDerived)
		{
			if (!bCapWarned)
			{
				UE_LOG(LogGSTNKitMods, Warning, TEXT("Mods: kit %s reached MaxDerivedTimelines (%d); further moves keep their base timeline"),
					*Context.Resolved->Kit->GetName(), MaxDerived);
				bCapWarned = true;
			}
			return;
		}
		Derived.Add(Name);
		if (const UGSTN_CombatTimeline* Timeline = FindOrBakeDerived(BasePath, Base, Resolved.ModIds, Combined))
		{
			Binding.TimelineOverride = FSoftObjectPath(Timeline);
		}
	};
	for (FGSTN_BindingInput& Binding : Input.Kit.Bindings)
	{
		ModifyBinding(Binding);
	}
	for (FGSTN_WeaponCompileInput& Weapon : Input.Weapons)
	{
		for (FGSTN_BindingInput& Binding : Weapon.Bindings)
		{
			ModifyBinding(Binding);
		}
	}
}

void FGSTN_KitModLoadout::ContributeRulesView(const FGSTN_LoadoutExtensionContext& Context, FGSTN_LoadoutRulesView& View)
{
	if (!Context.Resolved)
	{
		return;
	}
	FGSTN_ResolvedMods Resolved;
	ResolveMods(*Context.Resolved, Resolved);
	View.Table.Warnings.Append(Resolved.Warnings);
	if (View.Weapons.IsEmpty())
	{
		return;
	}
	FGSTN_LoadoutWeaponView& Primary = View.Weapons[0];
	for (const UGSTN_KitModDefinition* Mod : Resolved.Mods)
	{
		for (const TPair<FGameplayTag, FGSTN_AbilityClassList>& Pair : Mod->UnlockedAlternatives)
		{
			FGSTN_AllowedAlternativeView* Allowed = Primary.AllowedAlternatives.FindByPredicate([&Pair](const FGSTN_AllowedAlternativeView& Entry) { return Entry.Command == Pair.Key; });
			if (!Allowed)
			{
				Allowed = &Primary.AllowedAlternatives.AddDefaulted_GetRef();
				Allowed->Command = Pair.Key;
			}
			for (const TSoftClassPtr<UGameplayAbility>& Ability : Pair.Value.Abilities)
			{
				if (!Ability.IsNull())
				{
					Allowed->Abilities.AddUnique(Ability.ToSoftObjectPath());
				}
			}
		}
	}
}

void FGSTN_KitModLoadout::OnGranted(const FGSTN_LoadoutExtensionContext& Context, UAbilitySystemComponent& AbilitySystem)
{
	if (!Context.Resolved)
	{
		return;
	}
	FGSTN_ResolvedMods Resolved;
	ResolveMods(*Context.Resolved, Resolved);
	TArray<FActiveGameplayEffectHandle>& Handles = GetAppliedEffects().FindOrAdd(TWeakObjectPtr<UAbilitySystemComponent>(&AbilitySystem));
	for (const UGSTN_KitModDefinition* Mod : Resolved.Mods)
	{
		if (!Mod->StatsEffect)
		{
			continue;
		}
		FGameplayEffectSpecHandle Spec = AbilitySystem.MakeOutgoingSpec(Mod->StatsEffect, 1.f, AbilitySystem.MakeEffectContext());
		if (!Spec.IsValid())
		{
			continue;
		}
		for (const TPair<FGameplayTag, float>& Magnitude : Mod->StatsMagnitudes)
		{
			Spec.Data->SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value);
		}
		const FActiveGameplayEffectHandle Handle = AbilitySystem.ApplyGameplayEffectSpecToSelf(*Spec.Data);
		if (Handle.IsValid())
		{
			Handles.Add(Handle);
		}
	}
}

void FGSTN_KitModLoadout::OnCleared(UAbilitySystemComponent& AbilitySystem)
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
