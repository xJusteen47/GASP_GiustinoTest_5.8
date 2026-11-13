// Destructible parts, Network Prediction half.

#include "Sim/GSTN_PartsAttach.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Data/GSTN_PartsKitExtension.h"
#include "GameFramework/Actor.h"
#include "GSTN_PartsTags.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Math/GSTN_PartsState.h"
#include "State/GSTN_PartsFragment.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNPartsAttach, Log, All);

namespace
{
	/** The sets by entity. The strong pointer keeps the set alive for the lag compensation history, which copies a bare pointer to it */
	TMap<FObjectKey, TStrongObjectPtr<UGSTN_PartSetDefinition>>& Attached()
	{
		static TMap<FObjectKey, TStrongObjectPtr<UGSTN_PartSetDefinition>> Map;
		return Map;
	}

	/** Pending one-shot seeds by entity (SetPendingSeed). Almost always empty: HasPendingSeed tests the count first */
	TMap<FObjectKey, TArray<FGSTN_PartsAttach::FSeedEntry, TInlineAllocator<GSTN_PartsLimits::MaxParts>>>& Seeds()
	{
		static TMap<FObjectKey, TArray<FGSTN_PartsAttach::FSeedEntry, TInlineAllocator<GSTN_PartsLimits::MaxParts>>> Map;
		return Map;
	}

	/** Forgets the entries whose actor is gone, so a destroyed pawn does not keep its set alive */
	void Prune()
	{
		for (auto It = Attached().CreateIterator(); It; ++It)
		{
			if (!It.Key().ResolveObjectPtr())
			{
				It.RemoveCurrent();
			}
		}
	}
}

const UGSTN_PartSetDefinition* FGSTN_PartsAttach::FindSet(const AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	const TStrongObjectPtr<UGSTN_PartSetDefinition>* Found = Attached().Find(FObjectKey(Owner));
	return Found ? Found->Get() : nullptr;
}

UGSTN_PartSetDefinition* FGSTN_PartsAttach::ResolveKitSet(const FGSTN_KitExtensionList& Extensions)
{
	const FGSTN_PartsKitExtension* Extension = Extensions.Find<FGSTN_PartsKitExtension>();
	return Extension ? Extension->PartSet.LoadSynchronous() : nullptr;
}

bool FGSTN_PartsAttach::IsPartCountCompatible(const FGSTN_PartsFragment& Fragment, const UGSTN_PartSetDefinition& Set)
{
	const int32 Count = Set.GetParts().Num();
	if (Fragment.GetWireCount() > Count)
	{
		return false;
	}
	return Fragment.PartCount == 0 || Fragment.PartCount == Count;
}

bool FGSTN_PartsAttach::ApplyPartSet(AActor* Owner, UGSTN_PartSetDefinition* Set, UAbilitySystemComponent* ASC, const FGSTN_PartsFragment* Existing)
{
	if (!Owner)
	{
		return false;
	}
	if (!Set)
	{
		ClearPartSet(Owner, ASC);
		return false;
	}
	const int32 Count = Set->GetParts().Num();
	if (Count == 0 || Count > GSTN_PartsLimits::MaxParts)
	{
		UE_LOG(LogGSTNPartsAttach, Error, TEXT("%s: part set %s has %d parts (1 to %d allowed), not attached"), *Owner->GetName(), *Set->GetName(), Count, GSTN_PartsLimits::MaxParts);
		ClearPartSet(Owner, ASC);
		return false;
	}
	if (Existing && !IsPartCountCompatible(*Existing, *Set))
	{
		UE_LOG(LogGSTNPartsAttach, Error, TEXT("%s: part set %s has %d parts but the parts state already received holds %d (recorded count %d); the machines disagree on the set, not attached"),
			*Owner->GetName(), *Set->GetName(), Count, Existing->GetWireCount(), Existing->PartCount);
		ClearPartSet(Owner, ASC);
		return false;
	}

	Prune();
	Attached().Add(FObjectKey(Owner), TStrongObjectPtr<UGSTN_PartSetDefinition>(Set));
	if (ASC)
	{
		ASC->SetLooseGameplayTagCount(GSTNPartsTags::Entity_Destructible, 1);
	}
	return true;
}

void FGSTN_PartsAttach::ClearPartSet(AActor* Owner, UAbilitySystemComponent* ASC)
{
	if (!Owner)
	{
		return;
	}
	Attached().Remove(FObjectKey(Owner));
	if (ASC)
	{
		ASC->SetLooseGameplayTagCount(GSTNPartsTags::Entity_Destructible, 0);
	}
}

int32 FGSTN_PartsAttach::Num()
{
	return Attached().Num();
}

void FGSTN_PartsAttach::Reset()
{
	Attached().Reset();
	Seeds().Reset();
}

void FGSTN_PartsAttach::SetPendingSeed(const AActor* Owner, TConstArrayView<FSeedEntry> Entries)
{
	if (!Owner)
	{
		return;
	}
	// A seed whose owner is gone is never consumed: drop those before adding (promotions are rare, the map stays tiny)
	for (auto It = Seeds().CreateIterator(); It; ++It)
	{
		if (!It.Key().ResolveObjectPtr())
		{
			It.RemoveCurrent();
		}
	}
	auto& Seed = Seeds().FindOrAdd(FObjectKey(Owner));
	Seed.Reset();
	for (const FSeedEntry& Entry : Entries.Left(GSTN_PartsLimits::MaxParts))
	{
		if (Entry.Part != NAME_None && (Entry.bBroken || Entry.Damage != 0))
		{
			Seed.Add(Entry);
		}
	}
	if (Seed.IsEmpty())
	{
		Seeds().Remove(FObjectKey(Owner));
	}
}

bool FGSTN_PartsAttach::HasPendingSeed(const AActor* Owner)
{
	return !Seeds().IsEmpty() && Owner && Seeds().Contains(FObjectKey(Owner));
}

bool FGSTN_PartsAttach::ConsumePendingSeed(const AActor* Owner, const UGSTN_PartSetDefinition& Set, FGSTN_SyncFragmentArray& Fragments)
{
	if (!HasPendingSeed(Owner))
	{
		return false;
	}
	const auto Seed = Seeds().FindAndRemoveChecked(FObjectKey(Owner));
	const int32 Count = Set.GetParts().Num();
	if (Count == 0)
	{
		return true;
	}
	FGSTN_PartsFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_PartsFragment>(Fragments);
	FGSTN_PartsState State = Fragment.ToState();
	for (const FSeedEntry& Entry : Seed)
	{
		const int32 Index = Set.FindPartByName(Entry.Part);
		if (Index == INDEX_NONE || Index >= Count)
		{
			continue;
		}
		// Idempotent merge: a part never gets better from a seed
		State.PartDamage[Index] = FMath::Max(State.PartDamage[Index], Entry.Damage);
		if (Entry.bBroken)
		{
			State.BrokenMask |= static_cast<uint16>(1u << Index);
		}
	}
	Fragment.SetState(State, Count);
	return true;
}

UGSTN_PartsAttachComponent::UGSTN_PartsAttachComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGSTN_PartsAttachComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UGSTN_LoadoutComponent* Loadout = GetOwner() ? GetOwner()->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr)
	{
		Loadout->OnLoadoutApplied.AddDynamic(this, &UGSTN_PartsAttachComponent::HandleLoadoutApplied);
		if (Loadout->HasLoadout())
		{
			// Applied before this component began play (a client whose loadout replicated first)
			HandleLoadoutApplied(Loadout, 0);
			return;
		}
	}
	Attach(DefaultPartSet.LoadSynchronous());
}

void UGSTN_PartsAttachComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGSTN_LoadoutComponent* Loadout = GetOwner() ? GetOwner()->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr)
	{
		Loadout->OnLoadoutApplied.RemoveDynamic(this, &UGSTN_PartsAttachComponent::HandleLoadoutApplied);
	}
	FGSTN_PartsAttach::ClearPartSet(GetOwner(), UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
	Super::EndPlay(EndPlayReason);
}

void UGSTN_PartsAttachComponent::HandleLoadoutApplied(UGSTN_LoadoutComponent* Component, int32 Revision)
{
	UGSTN_PartSetDefinition* Set = nullptr;
	if (Component && Component->HasLoadout())
	{
		if (const UGSTN_PrimaryKitDefinition* Kit = Component->GetResolvedLoadout().Kit)
		{
			Set = FGSTN_PartsAttach::ResolveKitSet(Kit->Extensions);
		}
	}
	Attach(Set ? Set : DefaultPartSet.LoadSynchronous());
}

void UGSTN_PartsAttachComponent::Attach(UGSTN_PartSetDefinition* Set)
{
	AActor* Owner = GetOwner();
	FGSTN_PartsAttach::ApplyPartSet(Owner, Set, UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner));
}
