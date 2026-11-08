// Bridge between parts and minions (plan P16).

#include "GSTN_PartsMinionSubsystem.h"

#include "Data/GSTN_MinionDefinition.h"
#include "Data/GSTN_PartDefinition.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "GSTNPartsMinionModule.h"
#include "GSTN_HitStep_PartResolveMinion.h"
#include "GSTN_PartsMinionPuppetSubsystem.h"
#include "GSTN_PartsMinionSettings.h"
#include "Hits/GSTN_HitResolution.h"
#include "Hits/GSTN_HitVictimView.h"
#include "Net/GSTN_MinionClientRegistry.h"
#include "Net/GSTN_MinionPack.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Sim/GSTN_PartsAttach.h"
#include "Sync/GSTN_SyncFragment.h"

namespace
{
	double CyclesToMicroseconds(uint64 Cycles, int32 Count)
	{
		return Count > 0 ? FPlatformTime::ToMilliseconds64(Cycles) * 1000.0 / Count : 0.0;
	}

	/** Every game world of this process (the PIE server and its clients share one), so a bench registration reaches all of them */
	template <typename FuncT>
	void ForEachBridge(FuncT&& Func)
	{
		if (!GEngine)
		{
			return;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
			{
				if (UGSTN_PartsMinionSubsystem* Bridge = World->GetSubsystem<UGSTN_PartsMinionSubsystem>())
				{
					Func(*Bridge);
				}
			}
		}
	}

	// Bench commands of sub-gate B (scenario_minion_parts). Test tooling: they create transient objects only, never an asset
	FAutoConsoleCommandWithWorldAndArgs GBenchPartsCommand(
		TEXT("gstn.PartsMinion.Bench.Parts"),
		TEXT("gstn.PartsMinion.Bench.Parts [TypeTag=Minion.Type.Grunt] [HeadHurtboxes=head] [HeadCap=1]: gives that minion type a transient part set ")
		TEXT("(Head on the listed hurtboxes, comma separated, not lethal; Legs on feet, unbreakable) in every game world of this process. Run it before ")
		TEXT("the type's first spawn: the type's wire format (ExtByte) is decided then."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			const FString TagName = Args.IsValidIndex(0) ? Args[0] : FString(TEXT("Minion.Type.Grunt"));
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
			TArray<FString> Names;
			(Args.IsValidIndex(1) ? Args[1] : FString(TEXT("head"))).ParseIntoArray(Names, TEXT(","));
			TArray<FName> Hurtboxes;
			for (const FString& Name : Names)
			{
				Hurtboxes.Add(FName(*Name.TrimStartAndEnd()));
			}
			const float Cap = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 1.f;
			if (!Tag.IsValid() || Hurtboxes.IsEmpty())
			{
				UE_LOG(LogGSTNPartsMinion, Warning, TEXT("PartsMinionBench: bad arguments (tag %s, %d hurtboxes)"), *TagName, Hurtboxes.Num());
				return;
			}
			int32 Worlds = 0;
			ForEachBridge([&](UGSTN_PartsMinionSubsystem& Bridge)
			{
				Worlds += Bridge.RegisterBenchParts(Tag, Hurtboxes, Cap) ? 1 : 0;
			});
			FString HurtboxList;
			for (const FName& Name : Hurtboxes)
			{
				HurtboxList += (HurtboxList.IsEmpty() ? TEXT("") : TEXT(",")) + Name.ToString();
			}
			UE_LOG(LogGSTNPartsMinion, Log, TEXT("PartsMinionBench: parts registered type=%s hurtboxes=%s headCap=%.2f worlds=%d"),
				*Tag.ToString(), *HurtboxList, Cap, Worlds);
		}));

	FAutoConsoleCommandWithWorldAndArgs GBenchReportCommand(
		TEXT("gstn.PartsMinion.Bench.Report"),
		TEXT("gstn.PartsMinion.Bench.Report: logs 'PartsMinionBench server ...' (rows, broken masks by global id, per hit cost) on the authority and ")
		TEXT("'PartsMinionBench client ...' (received ext bytes) on each client, for every game world of this process."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld*)
		{
			ForEachBridge([](UGSTN_PartsMinionSubsystem& Bridge) { Bridge.ReportBench(); });
		}));
}

bool UGSTN_PartsMinionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// The worlds the minion subsystem lives in; it is active (and calls the hooks) only on the authority
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_PartsMinionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UGSTN_MinionCombatSubsystem* Minions = Collection.InitializeDependency<UGSTN_MinionCombatSubsystem>();
	UGSTN_MinionClientRegistry* Registry = Collection.InitializeDependency<UGSTN_MinionClientRegistry>();
	Super::Initialize(Collection);

	TagExtensions = UGSTN_PartsMinionSettings::Get().TypeExtensions;
	TypeToResolved.Init(TypeUnresolved, 256);
	// The hit step's per (set, rules) cache is process-wide: a new world starts it clean
	UGSTN_HitStep_PartResolveMinion::InvalidateStepCache();

	// Bound at once, active or not: the hooks only fire from the authority's batch, and a client's inactive subsystem never drains a hit
	Bind(Minions);

	// The client half of the net flag: the registry resolves the server's type table here (it stays empty on the authority)
	if (Registry)
	{
		BoundRegistry = Registry;
		ResolvingHandle = Registry->OnDefinitionResolving.AddUObject(this, &UGSTN_PartsMinionSubsystem::HandleDefinitionNet);
	}
}

void UGSTN_PartsMinionSubsystem::Deinitialize()
{
	Unbind();
	UGSTN_HitStep_PartResolveMinion::InvalidateStepCache();
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
	RegisteringHandle = Minions->OnDefinitionRegistering.AddUObject(this, &UGSTN_PartsMinionSubsystem::HandleDefinitionNet);
	PromotedHandle = Minions->OnMinionPromoted.AddUObject(this, &UGSTN_PartsMinionSubsystem::HandleMinionPromoted);
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
		Minions->OnDefinitionRegistering.Remove(RegisteringHandle);
		Minions->OnMinionPromoted.Remove(PromotedHandle);
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
	if (UGSTN_MinionClientRegistry* Registry = BoundRegistry.Get())
	{
		Registry->OnDefinitionResolving.Remove(ResolvingHandle);
	}
	ActivatedHandle.Reset();
	RegisteringHandle.Reset();
	PromotedHandle.Reset();
	ResolvingHandle.Reset();
	BoundMinions.Reset();
	BoundRegistry.Reset();
}

void UGSTN_PartsMinionSubsystem::RegisterDefinition(const UGSTN_MinionDefinition* Definition, const FGSTN_PartsMinionExtension& Extension)
{
	if (!Definition)
	{
		return;
	}
	DefinitionExtensions.Add(Definition, Extension);
	UGSTN_HitStep_PartResolveMinion::InvalidateStepCache();
	// Re-resolve every type at its next activation; ResolvedTypes is append-only, so live slots keep valid indices
	TypeToResolved.Init(TypeUnresolved, 256);
	PresentationSets.Reset();

	// The wire format of a type is fixed at its registration (OnDefinitionRegistering). Already registered: the rows work, the byte does not
	if (const UGSTN_MinionCombatSubsystem* Minions = BoundMinions.Get())
	{
		for (int32 TypeIndex = 0; TypeIndex <= MAX_uint8; ++TypeIndex)
		{
			const UGSTN_MinionDefinition* Registered = Minions->GetDefinition(TypeIndex);
			if (!Registered)
			{
				break;
			}
			if (Registered == Definition && !Definition->bHasNetExt)
			{
				UE_LOG(LogGSTNPartsMinion, Warning, TEXT("Minion type %s got a parts extension after its first spawn: its parts work on the server, but its net item has no ")
					TEXT("extension byte for this world (register before the first spawn)."), *GetNameSafe(Definition));
				break;
			}
		}
	}
}

const FGSTN_PartsMinionExtension* UGSTN_PartsMinionSubsystem::FindExtension(const UGSTN_MinionDefinition* Definition) const
{
	if (!Definition)
	{
		return nullptr;
	}
	const FGSTN_PartsMinionExtension* Extension = DefinitionExtensions.Find(Definition);
	return Extension ? Extension : TagExtensions.Find(Definition->TypeTag);
}

UGSTN_PartSetDefinition* UGSTN_PartsMinionSubsystem::ResolveUsableSet(const FGSTN_PartsMinionExtension& Extension)
{
	// Once per type per machine: a synchronous load is acceptable here (registration or activation, never a hit)
	UGSTN_PartSetDefinition* Set = Extension.PartSet.LoadSynchronous();
	if (!Set || Set->Parts.IsEmpty())
	{
		UE_LOG(LogGSTNPartsMinion, Warning, TEXT("A minion parts extension has no part set (%s); the type loses no parts."), *Extension.PartSet.ToString());
		return nullptr;
	}
	if (!Set->bForMinions || Set->Parts.Num() > GSTN_PartsLimits::MaxMinionParts)
	{
		UE_LOG(LogGSTNPartsMinion, Warning, TEXT("Part set %s is not minion-ready (bForMinions off or more than %d parts); the type loses no parts."),
			*GetNameSafe(Set), GSTN_PartsLimits::MaxMinionParts);
		return nullptr;
	}
	return Set;
}

const UGSTN_PartSetDefinition* UGSTN_PartsMinionSubsystem::GetPresentationSet(const UGSTN_MinionDefinition* Definition)
{
	if (!Definition)
	{
		return nullptr;
	}
	if (const UGSTN_PartSetDefinition* const* Cached = PresentationSets.Find(Definition))
	{
		return *Cached;
	}
	const FGSTN_PartsMinionExtension* Extension = FindExtension(Definition);
	UGSTN_PartSetDefinition* Set = Extension ? ResolveUsableSet(*Extension) : nullptr;
	if (Set)
	{
		ResolvedSets.AddUnique(Set);
	}
	PresentationSets.Add(Definition, Set);
	return Set;
}

void UGSTN_PartsMinionSubsystem::HandleDefinitionNet(UGSTN_MinionDefinition* Definition)
{
	// Never clears the flag: a designer may own the byte for another reason, and a PIE client shares the server's (already flagged) object
	if (!Definition || Definition->bHasNetExt)
	{
		return;
	}
	const FGSTN_PartsMinionExtension* Extension = FindExtension(Definition);
	if (Extension && ResolveUsableSet(*Extension))
	{
		Definition->bHasNetExt = true;
		++Stats.NetExtFlagged;
		UE_LOG(LogGSTNPartsMinion, Log, TEXT("Minion type %s carries its broken-parts mask in the net extension byte."), *GetNameSafe(Definition));
	}
}

void UGSTN_PartsMinionSubsystem::RegisterTypeTag(const FGameplayTag& TypeTag, const FGSTN_PartsMinionExtension& Extension)
{
	if (!TypeTag.IsValid())
	{
		return;
	}
	TagExtensions.Add(TypeTag, Extension);
	UGSTN_HitStep_PartResolveMinion::InvalidateStepCache();
	TypeToResolved.Init(TypeUnresolved, 256);
	PresentationSets.Reset();
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
	const FGSTN_PartsMinionExtension* Extension = FindExtension(Definition);
	if (!Extension)
	{
		return TypeWithoutParts;
	}
	// The same rule the net flag used at registration, so a type with a byte on the wire always has rows (and the reverse)
	UGSTN_PartSetDefinition* Set = ResolveUsableSet(*Extension);
	if (!Set)
	{
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
	const int32 OldBreakFrames = LastBreakFrame.Num();
	LastBreakFrame.SetNum(Capacity);
	for (int32 Slot = OldBreakFrames; Slot < Capacity; ++Slot)
	{
		LastBreakFrame[Slot] = INDEX_NONE;
	}
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
	LastBreakFrame[Slot] = INDEX_NONE;
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
	const uint64 Start = FPlatformTime::Cycles64();
	const FGSTN_PartsMinionRow& Row = Rows[Slot];
	ViewExts[Slot] = FGSTN_PartsSnapshotExt::FromMinionStore(Type.PartSet, Row.BrokenMask, Row.PartDamage);
	// A minion entry carries no other snapshot extension (no fragments, no contributors), so the parts one is the only slot
	const FGSTN_HitVictimView::FSnapshotExtensionRef Ref{ GSTNFeatureIds::Parts, &ViewExts[Slot] };
	View.SetSnapshotExtensions(&Ref, 1);
	Stats.ViewCycles += FPlatformTime::Cycles64() - Start;
	++Stats.Views;
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

	const uint64 Start = FPlatformTime::Cycles64();
	FGSTN_PartsMinionRow& Row = Rows[Handle.Index];
	const uint8 OldMask = Row.BrokenMask;
	const bool bFits = FGSTN_PartsSnapshotExt::ToMinionStore(Outcome->GetResultState(), Row.BrokenMask, Row.PartDamage);
	// Validation rejects a set over 8 parts and ResolveType refuses one, so the state of a minion always fits
	ensureMsgf(bFits, TEXT("A minion part outcome touched a part past the eighth"));
	++Stats.PartHits;

	// The wire byte follows the mask and nothing else: a dent leaves the item clean, the damage bytes stay on the server. SetExtByte is a
	// store write the publish stage reads after this stage (the rate limiter then sends the item once for the change)
	if (Row.BrokenMask != OldMask)
	{
		if (UGSTN_MinionCombatSubsystem* Minions = BoundMinions.Get())
		{
			Minions->SetExtByte(Handle, Row.BrokenMask);
			++Stats.ExtByteWrites;
		}
		LastBreakFrame[Handle.Index] = Frame;
	}

	// Lethality is the type's list and nothing else (the MainDamage Kill flag is not read for minions). Only a part this hit broke can kill:
	// a lethal part broken earlier already killed then
	const uint8 NewlyBroken = static_cast<uint8>(Outcome->NewlyBrokenMask & 0xFFu);
	const bool bLethal = FGSTN_PartsMinionExtension::IsLethal(NewlyBroken, ResolvedTypes[TypeIndex].LethalMask);
	Stats.LethalKills += bLethal ? 1 : 0;
	Stats.OutcomeCycles += FPlatformTime::Cycles64() - Start;
	++Stats.Outcomes;
	return bLethal;
}

void UGSTN_PartsMinionSubsystem::HandleMinionPromoted(const FGSTN_MinionHandle& Handle, AActor* Fighter)
{
	// Fires before the minion is freed: the row is still the minion's
	const FGSTN_PartsMinionRow* Row = GetRow(Handle);
	const UGSTN_PartSetDefinition* Set = GetPartSet(Handle);
	if (!Row || !Set || !Fighter)
	{
		return;
	}
	TArray<FGSTN_PartsAttach::FSeedEntry, TInlineAllocator<GSTN_PartsLimits::MaxMinionParts>> Entries;
	const TConstArrayView<FGSTN_PartDefinition> Parts = Set->GetParts();
	for (int32 Index = 0; Index < Parts.Num() && Index < GSTN_PartsLimits::MaxMinionParts; ++Index)
	{
		if (Row->PartDamage[Index] == 0 && (Row->BrokenMask & (1u << Index)) == 0)
		{
			continue;
		}
		FGSTN_PartsAttach::FSeedEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Part = Parts[Index].Name;
		Entry.Damage = Row->PartDamage[Index];
		Entry.bBroken = (Row->BrokenMask & (1u << Index)) != 0;
	}
	if (Entries.IsEmpty())
	{
		// A pristine minion: the fighter starts pristine anyway
		return;
	}
	// By name: the fighter's own set (its kit) decides where each part lands, and the parts tick extension merges it at the next frame
	FGSTN_PartsAttach::SetPendingSeed(Fighter, Entries);
	++Stats.PromotionSeeds;
	UE_LOG(LogGSTNPartsMinion, Verbose, TEXT("Minion %s promoted to %s with broken mask 0x%02x"), *Handle.ToString(), *GetNameSafe(Fighter), Row->BrokenMask);
}

int32 UGSTN_PartsMinionSubsystem::GetLastBreakFrame(const FGSTN_MinionHandle& Handle) const
{
	return HasParts(Handle) ? LastBreakFrame[Handle.Index] : INDEX_NONE;
}

UGSTN_PartSetDefinition* UGSTN_PartsMinionSubsystem::RegisterBenchParts(const FGameplayTag& TypeTag, const TArray<FName>& HeadHurtboxes, float HeadCap)
{
	UGSTN_PartSetDefinition* Set = NewObject<UGSTN_PartSetDefinition>(this, NAME_None, RF_Transient);
	FGSTN_PartDefinition& Head = Set->Parts.AddDefaulted_GetRef();
	Head.Name = FName(TEXT("Head"));
	Head.Hurtboxes = HeadHurtboxes;
	Head.Bones = { FName(TEXT("head")) };
	Head.Hardness = 0;
	Head.PartCap = FMath::Max(HeadCap, 0.01f);
	// A light share of the hit reaches the health pool, so the headless minions live long enough to keep attacking in the bench
	Head.MainTransfer = 0.25f;
	FGSTN_PartDefinition& Legs = Set->Parts.AddDefaulted_GetRef();
	Legs.Name = FName(TEXT("Legs"));
	Legs.Hurtboxes = { FName(TEXT("feet")) };
	Legs.Bones = { FName(TEXT("foot_l")), FName(TEXT("foot_r")) };
	Legs.PartCap = 0.f;
	Set->bForMinions = true;
	Set->RebakeWeightCurve();
	BenchSets.Add(Set);

	FGSTN_PartsMinionExtension Extension;
	Extension.PartSet = Set;
	RegisterTypeTag(TypeTag, Extension);
	return Set;
}

void UGSTN_PartsMinionSubsystem::ReportBench() const
{
	const UWorld* World = GetWorld();
	const FString WorldName = GetNameSafe(World);
	// The puppet side of this machine, wherever something renders (listen host from the rows, remote clients from the ext bytes)
	if (const UGSTN_PartsMinionPuppetSubsystem* Puppets = World ? World->GetSubsystem<UGSTN_PartsMinionPuppetSubsystem>() : nullptr)
	{
		const FGSTN_PartsMinionPuppetStats& PuppetStats = Puppets->GetStats();
		if (PuppetStats.Tracked > 0)
		{
			UE_LOG(LogGSTNPartsMinion, Log, TEXT("PartsMinionBench puppets world=%s tracked=%d hiding=%d changes=%d missingBones=%d"),
				*WorldName, PuppetStats.Tracked, PuppetStats.Hiding, PuppetStats.Changes, PuppetStats.MissingBones);
		}
	}
	const UGSTN_MinionCombatSubsystem* Minions = BoundMinions.Get();
	if (Minions && Minions->IsActive())
	{
		const FGSTN_MinionStore& Store = Minions->GetStore();
		int32 RowsAlive = 0;
		int32 Broken = 0;
		int32 BrokenAlive = 0;
		int32 AttackedAfterBreak = 0;
		TArray<FString> Masks;
		for (int32 Slot = 0; Slot < SlotType.Num(); ++Slot)
		{
			const FGSTN_MinionHandle Handle = Store.HandleOfSlot(Slot);
			if (!HasParts(Handle))
			{
				continue;
			}
			++RowsAlive;
			const uint8 Mask = Rows[Slot].BrokenMask;
			if (Mask == 0)
			{
				continue;
			}
			++Broken;
			const EGSTN_MinionActionState State = static_cast<EGSTN_MinionActionState>(Store.ActionState[Slot]);
			const bool bAlive = State != EGSTN_MinionActionState::Dying && State != EGSTN_MinionActionState::Dead;
			BrokenAlive += bAlive ? 1 : 0;
			AttackedAfterBreak += bAlive && Store.MoveStartFrame[Slot] > LastBreakFrame[Slot] ? 1 : 0;
			const AGSTN_MinionPack* Pack = Minions->GetPackOfSlot(Slot);
			const int32 ItemId = Minions->GetItemIdOfSlot(Slot);
			if (Pack && ItemId != INDEX_NONE)
			{
				Masks.Add(FString::Printf(TEXT("%d:%d"), UGSTN_MinionClientRegistry::MakeGlobalId(Pack->GetPackId(), static_cast<uint8>(ItemId)), Mask));
			}
		}
		const FGSTN_PartsMinionStepTiming& Step = UGSTN_HitStep_PartResolveMinion::GetTiming();
		const double ViewUs = CyclesToMicroseconds(Stats.ViewCycles, Stats.Views);
		const double OutcomeUs = CyclesToMicroseconds(Stats.OutcomeCycles, Stats.Outcomes);
		const double StepUs = CyclesToMicroseconds(Step.Cycles, Step.Count);
		const FString MaskList = Masks.IsEmpty() ? FString(TEXT("-")) : FString::Join(Masks, TEXT(";"));
		UE_LOG(LogGSTNPartsMinion, Log, TEXT("PartsMinionBench server world=%s rows=%d broken=%d brokenAlive=%d attackedAfterBreak=%d partHits=%d lethal=%d extWrites=%d ")
			TEXT("netExtFlagged=%d promotionSeeds=%d viewUs=%.3f stepUs=%.3f outcomeUs=%.3f hitUs=%.3f masks=%s"),
			*WorldName, RowsAlive, Broken, BrokenAlive, AttackedAfterBreak, Stats.PartHits, Stats.LethalKills, Stats.ExtByteWrites, Stats.NetExtFlagged,
			Stats.PromotionSeeds, ViewUs, StepUs, OutcomeUs, ViewUs + StepUs + OutcomeUs, *MaskList);
		UE_LOG(LogGSTNPartsMinion, Log, TEXT("PartsMinionBench step parts cached=%d stepLookupUs=%.3f stepScanUs=%.3f stepApplyUs=%.3f stepOutcomeUs=%.3f ")
			TEXT("stepProgressUs=%.3f stepWeightUs=%.3f stepTailUs=%.3f"),
			Step.CachedCount, CyclesToMicroseconds(Step.LookupCycles, Step.CachedCount), CyclesToMicroseconds(Step.ScanCycles, Step.CachedCount),
			CyclesToMicroseconds(Step.ApplyCycles, Step.CachedCount), CyclesToMicroseconds(Step.OutcomeCycles, Step.CachedCount),
			CyclesToMicroseconds(Step.ProgressCycles, Step.CachedCount), CyclesToMicroseconds(Step.WeightCycles, Step.CachedCount),
			CyclesToMicroseconds(Step.TailCycles, Step.CachedCount));
		return;
	}

	const UGSTN_MinionClientRegistry* Registry = BoundRegistry.Get();
	if (!Registry || Registry->GetItems().IsEmpty())
	{
		return;
	}
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	const int32 PlayerId = Controller && Controller->PlayerState ? Controller->PlayerState->GetPlayerId() : INDEX_NONE;
	int32 ExtItems = 0;
	TArray<FString> Masks;
	for (const TPair<int32, FGSTN_MinionClientItem>& Pair : Registry->GetItems())
	{
		const UGSTN_MinionDefinition* Definition = Registry->GetDefinition(Pair.Value.Wire.TypeIndex);
		if (!Definition || !Definition->bHasNetExt)
		{
			continue;
		}
		++ExtItems;
		if (Pair.Value.Wire.ExtByte != 0)
		{
			Masks.Add(FString::Printf(TEXT("%d:%d"), Pair.Key, Pair.Value.Wire.ExtByte));
		}
	}
	Masks.Sort();
	const FString MaskList = Masks.IsEmpty() ? FString(TEXT("-")) : FString::Join(Masks, TEXT(";"));
	UE_LOG(LogGSTNPartsMinion, Log, TEXT("PartsMinionBench client player=%d world=%s items=%d extItems=%d netExtFlagged=%d ext=%s"),
		PlayerId, *WorldName, Registry->GetItems().Num(), ExtItems, Stats.NetExtFlagged, *MaskList);
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
