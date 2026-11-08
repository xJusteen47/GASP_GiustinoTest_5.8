// Bridge between parts and minions (plan P16).

#include "GSTN_PartsMinionSubsystem.h"

#include "Data/GSTN_MinionDefinition.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Engine/World.h"
#include "GSTNPartsMinionModule.h"
#include "GSTN_HitStep_PartResolveMinion.h"
#include "GSTN_PartsMinionSettings.h"
#include "Hits/GSTN_HitResolution.h"
#include "Hits/GSTN_HitVictimView.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Sync/GSTN_SyncFragment.h"

bool UGSTN_PartsMinionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// The worlds the minion subsystem lives in; it is active (and calls the hooks) only on the authority
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_PartsMinionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UGSTN_MinionCombatSubsystem* Minions = Collection.InitializeDependency<UGSTN_MinionCombatSubsystem>();
	Super::Initialize(Collection);

	TagExtensions = UGSTN_PartsMinionSettings::Get().TypeExtensions;
	TypeToResolved.Init(TypeUnresolved, 256);

	// Bound at once, active or not: the hooks only fire from the authority's batch, and a client's inactive subsystem never drains a hit
	Bind(Minions);
}

void UGSTN_PartsMinionSubsystem::Deinitialize()
{
	Unbind();
	Super::Deinitialize();
}

void UGSTN_PartsMinionSubsystem::Bind(UGSTN_MinionCombatSubsystem* Minions)
{
	if (!Minions)
	{
		return;
	}
	BoundMinions = Minions;
	ActivatedHandle = Minions->OnMinionActivated.AddUObject(this, &UGSTN_PartsMinionSubsystem::HandleMinionActivated);
	Minions->VictimViewExtension.BindUObject(this, &UGSTN_PartsMinionSubsystem::ExtendVictimView);
	Minions->ApplyOutcomeExtensions.BindUObject(this, &UGSTN_PartsMinionSubsystem::ApplyOutcome);
	// The one extra step slot. A no-op for types without parts (one pointer test), so the M8 pipeline is unchanged for them
	Minions->SetExtraVictimStep(UGSTN_HitStep_PartResolveMinion::StaticClass());
}

void UGSTN_PartsMinionSubsystem::Unbind()
{
	if (UGSTN_MinionCombatSubsystem* Minions = BoundMinions.Get())
	{
		Minions->OnMinionActivated.Remove(ActivatedHandle);
		if (Minions->VictimViewExtension.IsBoundToObject(this))
		{
			Minions->VictimViewExtension.Unbind();
		}
		if (Minions->ApplyOutcomeExtensions.IsBoundToObject(this))
		{
			Minions->ApplyOutcomeExtensions.Unbind();
		}
		Minions->SetExtraVictimStep(nullptr);
	}
	ActivatedHandle.Reset();
	BoundMinions.Reset();
}

void UGSTN_PartsMinionSubsystem::RegisterDefinition(const UGSTN_MinionDefinition* Definition, const FGSTN_PartsMinionExtension& Extension)
{
	if (!Definition)
	{
		return;
	}
	DefinitionExtensions.Add(Definition, Extension);
	// Re-resolve every type at its next activation; ResolvedTypes is append-only, so live slots keep valid indices
	TypeToResolved.Init(TypeUnresolved, 256);
}

void UGSTN_PartsMinionSubsystem::RegisterTypeTag(const FGameplayTag& TypeTag, const FGSTN_PartsMinionExtension& Extension)
{
	if (!TypeTag.IsValid())
	{
		return;
	}
	TagExtensions.Add(TypeTag, Extension);
	TypeToResolved.Init(TypeUnresolved, 256);
}

int16 UGSTN_PartsMinionSubsystem::ResolveType(int32 TypeIndex)
{
	if (!TypeToResolved.IsValidIndex(TypeIndex))
	{
		return TypeWithoutParts;
	}
	if (TypeToResolved[TypeIndex] != TypeUnresolved)
	{
		return TypeToResolved[TypeIndex];
	}
	TypeToResolved[TypeIndex] = TypeWithoutParts;

	const UGSTN_MinionCombatSubsystem* Minions = BoundMinions.Get();
	const UGSTN_MinionDefinition* Definition = Minions ? Minions->GetDefinition(TypeIndex) : nullptr;
	if (!Definition)
	{
		return TypeWithoutParts;
	}
	const FGSTN_PartsMinionExtension* Extension = DefinitionExtensions.Find(Definition);
	if (!Extension)
	{
		Extension = TagExtensions.Find(Definition->TypeTag);
	}
	if (!Extension)
	{
		return TypeWithoutParts;
	}

	// Once per type per world: a synchronous load is acceptable here (activation, not a hit). Preloading the sets is sub-gate B's (bench)
	UGSTN_PartSetDefinition* Set = Extension->PartSet.LoadSynchronous();
	if (!Set || Set->Parts.IsEmpty())
	{
		UE_LOG(LogGSTNPartsMinion, Warning, TEXT("Minion type %s: the parts extension has no part set; the type loses no parts."), *GetNameSafe(Definition));
		return TypeWithoutParts;
	}
	if (!Set->bForMinions || Set->Parts.Num() > GSTN_PartsLimits::MaxMinionParts)
	{
		UE_LOG(LogGSTNPartsMinion, Warning, TEXT("Minion type %s: part set %s is not minion-ready (bForMinions off or more than %d parts); the type loses no parts."),
			*GetNameSafe(Definition), *GetNameSafe(Set), GSTN_PartsLimits::MaxMinionParts);
		return TypeWithoutParts;
	}
	if (ResolvedTypes.Num() >= MAX_uint8)
	{
		UE_LOG(LogGSTNPartsMinion, Warning, TEXT("Minion type %s: too many resolved part types; the type loses no parts."), *GetNameSafe(Definition));
		return TypeWithoutParts;
	}

	FResolvedType& Resolved = ResolvedTypes.AddDefaulted_GetRef();
	Resolved.PartSet = Set;
	Resolved.LethalMask = Extension->ResolveLethalMask(Set->GetParts());
	ResolvedSets.AddUnique(Set);
	TypeToResolved[TypeIndex] = static_cast<int16>(ResolvedTypes.Num() - 1);
	return TypeToResolved[TypeIndex];
}

void UGSTN_PartsMinionSubsystem::EnsureColumns(int32 Capacity)
{
	if (Rows.Num() >= Capacity)
	{
		return;
	}
	Rows.SetNum(Capacity);
	SlotType.SetNumZeroed(Capacity);
	SlotGeneration.SetNumZeroed(Capacity);
	ViewExts.SetNum(Capacity);
}

int32 UGSTN_PartsMinionSubsystem::GetSlotType(int32 Slot, uint16 Generation) const
{
	if (!SlotType.IsValidIndex(Slot) || SlotType[Slot] == 0 || SlotGeneration[Slot] != Generation)
	{
		return INDEX_NONE;
	}
	return SlotType[Slot] - 1;
}

void UGSTN_PartsMinionSubsystem::HandleMinionActivated(const FGSTN_MinionHandle& Handle)
{
	const UGSTN_MinionCombatSubsystem* Minions = BoundMinions.Get();
	if (!Minions)
	{
		return;
	}
	const FGSTN_MinionStore& Store = Minions->GetStore();
	const int32 Slot = Handle.Index;
	const int16 Type = ResolveType(Store.TypeIndex.IsValidIndex(Slot) ? Store.TypeIndex[Slot] : INDEX_NONE);
	if (Type == TypeWithoutParts && !SlotType.IsValidIndex(Slot))
	{
		// No part-bearing minion has activated yet: no columns at all
		return;
	}
	EnsureColumns(Store.Capacity());
	if (!SlotType.IsValidIndex(Slot))
	{
		return;
	}
	// The reset of the slot's previous occupant (freed without an event) happens here, and the generation keeps its row unreadable meanwhile
	Rows[Slot] = FGSTN_PartsMinionRow();
	SlotGeneration[Slot] = Handle.Generation;
	SlotType[Slot] = Type == TypeWithoutParts ? 0 : static_cast<uint8>(Type + 1);
	Stats.PartActivations += Type == TypeWithoutParts ? 0 : 1;
}

void UGSTN_PartsMinionSubsystem::ExtendVictimView(int32 Slot, FGSTN_HitVictimView& View)
{
	if (!SlotType.IsValidIndex(Slot) || SlotType[Slot] == 0)
	{
		return;
	}
	const UGSTN_MinionCombatSubsystem* Minions = BoundMinions.Get();
	if (!Minions || Minions->GetStore().HandleOfSlot(Slot).Generation != SlotGeneration[Slot])
	{
		return;
	}
	const FResolvedType& Type = ResolvedTypes[SlotType[Slot] - 1];
	const FGSTN_PartsMinionRow& Row = Rows[Slot];
	ViewExts[Slot] = FGSTN_PartsSnapshotExt::FromMinionStore(Type.PartSet, Row.BrokenMask, Row.PartDamage);
	// A minion entry carries no other snapshot extension (no fragments, no contributors), so the parts one is the only slot
	const FGSTN_HitVictimView::FSnapshotExtensionRef Ref{ GSTNFeatureIds::Parts, &ViewExts[Slot] };
	View.SetSnapshotExtensions(&Ref, 1);
}

bool UGSTN_PartsMinionSubsystem::ApplyOutcome(const FGSTN_MinionHandle& Handle, const FGSTN_HitResolutionContext& Context, int32 Frame)
{
	const int32 TypeIndex = GetSlotType(Handle.Index, Handle.Generation);
	if (TypeIndex == INDEX_NONE)
	{
		return false;
	}
	const FGSTN_PartHitOutcome* Outcome = nullptr;
	for (const FInstancedStruct& Entry : Context.OutcomeExtensions)
	{
		if (const FGSTN_PartHitOutcome* Found = Entry.GetPtr<FGSTN_PartHitOutcome>())
		{
			// One part step per minion pipeline, so one entry; the last wins should a rules asset ever add a second
			Outcome = Found;
		}
	}
	if (!Outcome)
	{
		return false;
	}

	FGSTN_PartsMinionRow& Row = Rows[Handle.Index];
	const bool bFits = FGSTN_PartsSnapshotExt::ToMinionStore(Outcome->GetResultState(), Row.BrokenMask, Row.PartDamage);
	// Validation rejects a set over 8 parts and ResolveType refuses one, so the state of a minion always fits
	ensureMsgf(bFits, TEXT("A minion part outcome touched a part past the eighth"));
	++Stats.PartHits;

	// Lethality is the type's list and nothing else (the MainDamage Kill flag is not read for minions). Only a part this hit broke can kill:
	// a lethal part broken earlier already killed then
	const uint8 NewlyBroken = static_cast<uint8>(Outcome->NewlyBrokenMask & 0xFFu);
	const bool bLethal = FGSTN_PartsMinionExtension::IsLethal(NewlyBroken, ResolvedTypes[TypeIndex].LethalMask);
	Stats.LethalKills += bLethal ? 1 : 0;
	return bLethal;
}

bool UGSTN_PartsMinionSubsystem::HasParts(const FGSTN_MinionHandle& Handle) const
{
	const UGSTN_MinionCombatSubsystem* Minions = BoundMinions.Get();
	return Minions && Minions->IsValidHandle(Handle) && GetSlotType(Handle.Index, Handle.Generation) != INDEX_NONE;
}

const FGSTN_PartsMinionRow* UGSTN_PartsMinionSubsystem::GetRow(const FGSTN_MinionHandle& Handle) const
{
	return HasParts(Handle) ? &Rows[Handle.Index] : nullptr;
}

uint8 UGSTN_PartsMinionSubsystem::GetLethalMask(const FGSTN_MinionHandle& Handle) const
{
	return HasParts(Handle) ? ResolvedTypes[GetSlotType(Handle.Index, Handle.Generation)].LethalMask : 0;
}

const UGSTN_PartSetDefinition* UGSTN_PartsMinionSubsystem::GetPartSet(const FGSTN_MinionHandle& Handle) const
{
	return HasParts(Handle) ? ResolvedTypes[GetSlotType(Handle.Index, Handle.Generation)].PartSet : nullptr;
}
