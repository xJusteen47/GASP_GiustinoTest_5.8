// Defence, Network Prediction half.

#include "Sim/GSTN_DefenceAttach.h"

#include "Data/GSTN_DefenceKitExtension.h"
#include "Data/GSTN_DefenceRules.h"
#include "GameFramework/Actor.h"
#include "GSTNDefenceCoreModule.h"
#include "GSTN_CombatRulesComponent.h"
#include "GSTN_DefenceLibrary.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Rules/GSTN_CombatRules.h"
#include "UObject/ObjectKey.h"

namespace
{
	struct FAttachState
	{
		TMap<FObjectKey, FGSTN_DefenceEntityData> Attached;

		/** Append-only: an index must keep its meaning while a history record can still hold it */
		TArray<TStrongObjectPtr<const UGSTN_DefenceRules>> Table;
	};

	FAttachState& State()
	{
		static FAttachState Instance;
		return Instance;
	}

	/** Forgets the entries whose actor is gone, so a destroyed pawn does not keep an entry */
	void Prune()
	{
		for (auto It = State().Attached.CreateIterator(); It; ++It)
		{
			if (!It.Key().ResolveObjectPtr())
			{
				It.RemoveCurrent();
			}
		}
	}
}

const FGSTN_DefenceEntityData* FGSTN_DefenceAttach::Find(const AActor* Owner)
{
	return Owner ? State().Attached.Find(FObjectKey(Owner)) : nullptr;
}

bool FGSTN_DefenceAttach::Apply(AActor* Owner, const UGSTN_DefenceRules* Rules, bool bKitHasShield, const FGameplayTagContainer& IgnoreHitTypes)
{
	if (!Owner)
	{
		return false;
	}
	if (!Rules)
	{
		Clear(Owner);
		return false;
	}
	const uint8 Index = IndexOf(Rules);
	if (Index == TNumericLimits<uint8>::Max())
	{
		UE_LOG(LogGSTNDefence, Error, TEXT("%s: the defence rules table is full, %s is not attached"), *Owner->GetName(), *Rules->GetName());
		Clear(Owner);
		return false;
	}
	Prune();
	FGSTN_DefenceEntityData& Data = State().Attached.FindOrAdd(FObjectKey(Owner));
	Data.Rules = Rules;
	Data.RulesIndex = Index;
	Data.bKitHasShield = bKitHasShield;
	Data.BubbleIgnoreHitTypes = IgnoreHitTypes;
	return true;
}

void FGSTN_DefenceAttach::SetKitMoves(const AActor* Owner, const FGSTN_DefenceKitExtension* Extension)
{
	FGSTN_DefenceEntityData* Data = Owner ? State().Attached.Find(FObjectKey(Owner)) : nullptr;
	if (!Data)
	{
		return;
	}
	Data->GroundBounce = nullptr;
	Data->GetupRoll = nullptr;
	for (TWeakObjectPtr<UClass>& Tech : Data->TechAbilities)
	{
		Tech = nullptr;
	}
	Data->bHasTechSet = false;
	if (!Extension)
	{
		return;
	}
	// Soft classes of the kit: resolved here, where the loadout is compiled, never in a tick
	Data->GroundBounce = Extension->GetupSet.GroundBounce.IsNull() ? nullptr : Cast<UClass>(Extension->GetupSet.GroundBounce.LoadSynchronous());
	Data->GetupRoll = Extension->GetupSet.Roll.IsNull() ? nullptr : Cast<UClass>(Extension->GetupSet.Roll.LoadSynchronous());
	for (const TPair<EGSTN_TechType, TSoftClassPtr<UObject>>& Entry : Extension->Tech.Abilities)
	{
		const uint8 Type = static_cast<uint8>(Entry.Key);
		if (Type > 0 && Type < UE_ARRAY_COUNT(Data->TechAbilities) && !Entry.Value.IsNull())
		{
			Data->TechAbilities[Type] = Cast<UClass>(Entry.Value.LoadSynchronous());
			Data->bHasTechSet |= Data->TechAbilities[Type].IsValid();
		}
	}
}

void FGSTN_DefenceAttach::Clear(AActor* Owner)
{
	if (Owner)
	{
		State().Attached.Remove(FObjectKey(Owner));
	}
}

const UGSTN_DefenceRules* FGSTN_DefenceAttach::RulesAt(uint8 Index)
{
	const TArray<TStrongObjectPtr<const UGSTN_DefenceRules>>& Table = State().Table;
	return Table.IsValidIndex(Index) ? Table[Index].Get() : nullptr;
}

uint8 FGSTN_DefenceAttach::IndexOf(const UGSTN_DefenceRules* Rules)
{
	if (!Rules)
	{
		return TNumericLimits<uint8>::Max();
	}
	TArray<TStrongObjectPtr<const UGSTN_DefenceRules>>& Table = State().Table;
	for (int32 Index = 0; Index < Table.Num(); ++Index)
	{
		if (Table[Index].Get() == Rules)
		{
			return static_cast<uint8>(Index);
		}
	}
	// 255 is the null index: the table holds 255 assets at most, far beyond what a match loads
	if (Table.Num() >= TNumericLimits<uint8>::Max())
	{
		return TNumericLimits<uint8>::Max();
	}
	Table.Add(TStrongObjectPtr<const UGSTN_DefenceRules>(Rules));
	return static_cast<uint8>(Table.Num() - 1);
}

FGameplayTagContainer FGSTN_DefenceAttach::MakeBubbleIgnoreHitTypes(const UGSTN_CombatRules* CombatRules)
{
	FGameplayTagContainer Ignored;
	if (CombatRules)
	{
		for (const FGameplayTag& Tag : CombatRules->NonBlockableHitTypes)
		{
			if (Tag.IsValid())
			{
				Ignored.AddTag(Tag);
			}
		}
	}
	else
	{
		Ignored.AddTag(GSTNCombatHitTags::HitType_Unblockable);
	}
	Ignored.AddTag(GSTNCombatHitTags::HitType_Grab);
	// The link plugin's types, read by name: absent where it is not loaded, which is fine (no such hit exists then)
	for (const TCHAR* Name : { TEXT("Combat.HitType.Throw"), TEXT("Combat.HitType.Pummel") })
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Name), false);
		if (Tag.IsValid())
		{
			Ignored.AddTag(Tag);
		}
	}
	return Ignored;
}

void FGSTN_DefenceAttach::AttachFromLoadout(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input)
{
	const UObject* WorldContext = Context.WorldContext;
	const UActorComponent* Component = Cast<UActorComponent>(WorldContext);
	AActor* Owner = Component ? Component->GetOwner() : nullptr;
	if (!Owner)
	{
		// A tool or a test compiles without a pawn: nothing to attach to
		return;
	}
	const UGSTN_PrimaryKitDefinition* Kit = Context.Resolved ? Context.Resolved->Kit : nullptr;
	const FGSTN_DefenceKitExtension* Extension = UGSTN_DefenceLibrary::FindKitExtension(Kit);
	if (!Extension)
	{
		Clear(Owner);
		return;
	}
	const UGSTN_DefenceRules* Rules = UGSTN_DefenceLibrary::ResolveRules(Extension, WorldContext);
	UGSTN_CombatRulesComponent* RulesComponent = UGSTN_CombatRulesComponent::Find(WorldContext);
	if (Apply(Owner, Rules, Extension->HasShield(), MakeBubbleIgnoreHitTypes(RulesComponent ? RulesComponent->GetRules() : nullptr)))
	{
		SetKitMoves(Owner, Extension);
	}
}

FName FGSTN_DefenceAttach::GetLoadoutExtensionId()
{
	static const FName Id(TEXT("GSTNDefenceNP"));
	return Id;
}

int32 FGSTN_DefenceAttach::Num()
{
	return State().Attached.Num();
}

void FGSTN_DefenceAttach::Reset()
{
	State().Attached.Reset();
	State().Table.Reset();
}
