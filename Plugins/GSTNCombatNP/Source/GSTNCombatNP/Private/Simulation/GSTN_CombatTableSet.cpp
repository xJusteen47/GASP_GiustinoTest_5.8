// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_CombatTableSet.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Abilities/GSTN_GA_PlatformJump.h"
#include "Algo/StableSort.h"
#include "Input/GSTN_CombatSettings.h"
#include "Input/GSTN_CommandLocks.h"
#include "Input/GSTN_InputSettings.h"
#include "Loadout/GSTN_EntrySourceMask.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryWriter.h"

namespace
{
	/** Name order, not tag index order: the index depends on the load order of the tag tables, which differs per machine */
	bool TagNameLess(const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.GetTagName().Compare(B.GetTagName()) < 0;
	}

	TMap<FName, FGSTN_CombatTableConverter::FExtension>& Extensions()
	{
		static TMap<FName, FGSTN_CombatTableConverter::FExtension> Registered;
		return Registered;
	}

	void WriteString(FArchive& Ar, const FString& Text)
	{
		FString Copy = Text;
		Ar << Copy;
	}

	void WriteTag(FArchive& Ar, const FGameplayTag& Tag)
	{
		WriteString(Ar, Tag.GetTagName().ToString());
	}
}

int32 UGSTN_CombatTableSet::AddTimelineClass(TSubclassOf<UGSTN_GA_BaseCombat> TimelineClass, int32 Tier, int32 DefaultOverrideFrames, bool bShareExisting)
{
	if (!TimelineClass)
	{
		return INDEX_NONE;
	}
	if (bShareExisting)
	{
		const int32 Existing = TimelineRegistry.IndexOfByKey(TimelineClass);
		if (Existing != INDEX_NONE)
		{
			return Existing;
		}
	}
	const int32 Index = TimelineRegistry.Add(TimelineClass);
	TimelineTier.Add(Tier);
	TimelineDefaultOverride.Add(DefaultOverrideFrames);

	// Soft content the timeline needs in memory before its first frame, loaded here and never in a tick, kept by this set
	TimelineClass->GetDefaultObject<UGSTN_GA_BaseCombat>()->LoadTimelineContent(TimelineContent);
	return Index;
}

void UGSTN_CombatTableSet::DeriveFromBindings(const UGSTN_InputSettings& InputSettings, const UGSTN_CombatSettings& CombatSettings)
{
	TimelineRegistry.Reset();
	TimelineContent.Reset();
	BindingTimeline.Reset();
	BindingJumpRole.Reset();
	TableJumpData = nullptr;
	TimelineTier.Reset();
	TimelineDefaultOverride.Reset();

	for (const FGSTN_CompiledBinding& Binding : Table.Bindings)
	{
		const TSubclassOf<UGSTN_GA_BaseCombat> TimelineClass = Binding.Ability.Get() && Binding.Ability->IsChildOf(UGSTN_GA_BaseCombat::StaticClass())
			? TSubclassOf<UGSTN_GA_BaseCombat>(Binding.Ability.Get()) : nullptr;
		int32 Index = INDEX_NONE;
		if (TimelineClass)
		{
			const int32 Tier = Table.Commands.IsValidIndex(Binding.CommandIndex) ? InputSettings.GetCommandTier(Table.Commands[Binding.CommandIndex].Tag) : 0;
			Index = AddTimelineClass(TimelineClass, Tier, CombatSettings.GetDefaultOverrideFrames(Tier), true);
		}
		BindingTimeline.Add(static_cast<int16>(Index));

		const UClass* AbilityClass = Binding.Ability.Get();
		const UGSTN_GA_PlatformJump* JumpDefault = AbilityClass && AbilityClass->IsChildOf(UGSTN_GA_PlatformJump::StaticClass()) ? AbilityClass->GetDefaultObject<UGSTN_GA_PlatformJump>() : nullptr;
		BindingJumpRole.Add(JumpDefault ? static_cast<uint8>(JumpDefault->Role) + 1 : 0);
		if (JumpDefault && JumpDefault->Role == EGSTN_PlatformJumpRole::Jump && !TableJumpData)
		{
			TableJumpData = JumpDefault;
		}
	}
}

void UGSTN_CombatTableSet::AppendAdoptable(TConstArrayView<TSubclassOf<UGSTN_GA_BaseCombat>> Adoptable)
{
	for (const TSubclassOf<UGSTN_GA_BaseCombat>& TimelineClass : Adoptable)
	{
		if (TimelineClass && !TimelineRegistry.Contains(TimelineClass))
		{
			AddTimelineClass(TimelineClass, 0, 0, false);
		}
	}
}

uint32 UGSTN_CombatTableSet::ComputeChecksum() const
{
	TArray<uint8> Bytes;
	FMemoryWriter Ar(Bytes);

	int32 RevisionCopy = Revision;
	Ar << RevisionCopy;

	TArray<uint8> CompiledBytes;
	Compiled.WriteBytes(CompiledBytes);
	Ar << CompiledBytes;

	int32 Count = Table.Commands.Num();
	Ar << Count;
	for (const FGSTN_CommandTable::FCommand& Command : Table.Commands)
	{
		WriteTag(Ar, Command.Tag);
		int32 PriorityOffset = Command.Slot ? Command.Slot->PriorityOffset : MIN_int32;
		Ar << PriorityOffset;
	}

	Count = Table.Bindings.Num();
	Ar << Count;
	for (int32 Index = 0; Index < Table.Bindings.Num(); ++Index)
	{
		const FGSTN_CompiledBinding& Binding = Table.Bindings[Index];
		WriteTag(Ar, Table.Commands.IsValidIndex(Binding.CommandIndex) ? Table.Commands[Binding.CommandIndex].Tag : FGameplayTag());
		uint32 Mask = Binding.SourceMask;
		Ar << Mask;
		WriteString(Ar, Binding.Ability ? Binding.Ability->GetPathName() : FString());
		int32 Registry = BindingTimeline.IsValidIndex(Index) ? BindingTimeline[Index] : INDEX_NONE;
		Ar << Registry;
		uint8 Role = BindingJumpRole.IsValidIndex(Index) ? BindingJumpRole[Index] : 0;
		Ar << Role;
		int32 HasOwnSlot = Binding.Slot ? 1 : 0;
		Ar << HasOwnSlot;
	}

	Count = TimelineRegistry.Num();
	Ar << Count;
	for (int32 Index = 0; Index < TimelineRegistry.Num(); ++Index)
	{
		WriteString(Ar, TimelineRegistry[Index] ? TimelineRegistry[Index]->GetPathName() : FString());
		int32 Tier = TimelineTier.IsValidIndex(Index) ? TimelineTier[Index] : 0;
		Ar << Tier;
		int32 DefaultOverride = TimelineDefaultOverride.IsValidIndex(Index) ? TimelineDefaultOverride[Index] : 0;
		Ar << DefaultOverride;
		int32 TotalFrames = TimelineRegistry[Index] ? TimelineRegistry[Index]->GetDefaultObject<UGSTN_GA_BaseCombat>()->GetTimelineData().TotalFrames : 0;
		Ar << TotalFrames;
	}

	Count = WeaponStats.Num();
	Ar << Count;
	for (const FGSTN_WeaponStatsEntry& Entry : WeaponStats)
	{
		WriteString(Ar, Entry.Effect ? Entry.Effect->GetPathName() : FString());
		int32 StatCount = Entry.Stats.Num();
		Ar << StatCount;
		for (const FGSTN_StatValue& Stat : Entry.Stats)
		{
			WriteTag(Ar, Stat.Stat);
			float Magnitude = Stat.Value;
			Ar << Magnitude;
		}
	}

	// The ricochet fallbacks: what changes the recoil two machines would play (the movement is not part of it: one curve and a flag decide the push)
	auto WriteRicochet = [&Ar](const FGSTN_RicochetResponse& Response)
	{
		int32 Frames = Response.RecoilFrames;
		Ar << Frames;
		uint8 bMovement = Response.bHasMovement ? 1 : 0;
		Ar << bMovement;
		WriteString(Ar, Response.RecoilMontage.ToSoftObjectPath().ToString());
		WriteTag(Ar, Response.CueTag);
	};
	Count = WeaponRicochet.Num();
	Ar << Count;
	for (const FGSTN_RicochetResponse& Response : WeaponRicochet)
	{
		WriteRicochet(Response);
	}
	WriteRicochet(KitRicochet);

	uint32 ExtensionCrc = ReservedExtension.IsEmpty() ? 0 : FCrc::MemCrc32(ReservedExtension.GetData(), ReservedExtension.Num());
	Ar << ExtensionCrc;
	return FCrc::MemCrc32(Bytes.GetData(), Bytes.Num());
}

void FGSTN_CombatTableConverter::RegisterExtension(FName Id, FExtension Extension)
{
	Extensions().Add(Id, MoveTemp(Extension));
}

void FGSTN_CombatTableConverter::UnregisterExtension(FName Id)
{
	Extensions().Remove(Id);
}

UGSTN_CombatTableSet* FGSTN_CombatTableConverter::Convert(UObject* Outer, const FGSTN_CompiledBindingTable& Compiled, const FGSTN_CombatTableConvertInput& Input, FGSTN_CompileReport* OutReport)
{
	check(Input.InputSettings && Input.CombatSettings);
	UGSTN_CombatTableSet* Set = NewObject<UGSTN_CombatTableSet>(Outer ? Outer : GetTransientPackage());
	Set->Revision = Compiled.Revision;
	Set->Compiled = Compiled;
	Set->WeaponStats = Input.WeaponStats;
	Set->WeaponRicochet = Input.WeaponRicochet;
	Set->KitRicochet = Input.KitRicochet;
	// The recoil montages are played from plain pointers by the simulation: loaded here, kept by the set, never loaded in a tick
	for (const FGSTN_RicochetResponse& Response : Set->WeaponRicochet)
	{
		if (Response.IsAuthored())
		{
			UGSTN_GA_BaseCombat::LoadRecoilContent(Response, Set->TimelineContent);
		}
	}
	if (Set->KitRicochet.IsAuthored())
	{
		UGSTN_GA_BaseCombat::LoadRecoilContent(Set->KitRicochet, Set->TimelineContent);
	}
	Set->ReservedExtension = Input.ReservedExtension;

	auto Error = [OutReport](const FString& Text)
	{
		if (OutReport)
		{
			OutReport->AddError(Text);
		}
	};

	// 1. The ability class of each entry, loaded once. An entry that cannot be started is dropped here, before it can take an index
	const int32 NumEntries = Compiled.Entries.Num();
	TArray<UClass*> EntryClass;
	EntryClass.Init(nullptr, NumEntries);
	TArray<int32> Kept;
	for (int32 Index = 0; Index < NumEntries; ++Index)
	{
		const FGSTN_CompiledEntry& Entry = Compiled.Entries[Index];
		UClass* Class = Entry.Ability.IsNull() ? nullptr : Input.LoadClass(Entry.Ability);
		if (!Class || !Class->IsChildOf(UGameplayAbility::StaticClass()))
		{
			Error(FString::Printf(TEXT("Entry %d (%s): ability %s does not load and is dropped"), Index, *Entry.Command.ToString(), *Entry.Ability.ToString()));
			continue;
		}
		FString Reason;
		if (!UGSTN_CommandBindingSet::IsBindableAbility(Class->GetDefaultObject<UGameplayAbility>(), &Reason))
		{
			Error(FString::Printf(TEXT("Entry %d (%s) is dropped: %s"), Index, *Entry.Command.ToString(), *Reason));
			continue;
		}
		if (GSTNEntrySourceMask::Bit(Entry.Source, Entry.SourceIndex) == 0)
		{
			Error(FString::Printf(TEXT("Entry %d (%s): source index %d has no mask bit and is dropped"), Index, *Entry.Command.ToString(), static_cast<int32>(Entry.SourceIndex)));
			continue;
		}
		if (!Compiled.Slots.IsValidIndex(Entry.SlotIndex))
		{
			Error(FString::Printf(TEXT("Entry %d (%s): no slot and is dropped"), Index, *Entry.Command.ToString()));
			continue;
		}
		EntryClass[Index] = Class;
		Kept.Add(Index);
	}

	// 2. Bindings sorted by (command name, entry order): a stable sort of the entry order keeps equal commands in the Core order
	Algo::StableSort(Kept, [&Compiled](int32 A, int32 B) { return TagNameLess(Compiled.Entries[A].Command, Compiled.Entries[B].Command); });
	if (Kept.Num() > FGSTN_CommandTable::MaxBindings)
	{
		Error(FString::Printf(TEXT("%d bindings, only the first %d are used"), Kept.Num(), FGSTN_CommandTable::MaxBindings));
		Kept.SetNum(FGSTN_CommandTable::MaxBindings);
	}

	// 3. Commands and bindings. The command's slot is the first entry's, shared by every entry of the command
	TArray<int32> BindingEntry;
	for (const int32 EntryIndex : Kept)
	{
		const FGSTN_CompiledEntry& Entry = Compiled.Entries[EntryIndex];
		if (Set->Table.Commands.IsEmpty() || Set->Table.Commands.Last().Tag != Entry.Command)
		{
			FGSTN_CommandTable::FCommand& NewCommand = Set->Table.Commands.AddDefaulted_GetRef();
			NewCommand.Tag = Entry.Command;
			NewCommand.Slot = &Set->Compiled.Slots[Entry.SlotIndex];
			NewCommand.LockTags = UGSTN_CommandLocks::GetLockTagsForCommand(Entry.Command);
			NewCommand.FirstBinding = Set->Table.Bindings.Num();
		}
		++Set->Table.Commands.Last().NumBindings;

		FGSTN_CompiledBinding& Binding = Set->Table.Bindings.AddDefaulted_GetRef();
		Binding.CommandIndex = Set->Table.Commands.Num() - 1;
		Binding.Requirements = Entry.Requirements;
		Binding.Ability = EntryClass[EntryIndex];
		Binding.SourceMask = GSTNEntrySourceMask::Bit(Entry.Source, Entry.SourceIndex);
		if (Entry.PriorityOffset != 0 || Entry.BufferFramesOverride >= 0)
		{
			Set->OwnedSlots.Add(MakeUnique<FGSTN_InputSlotSpec>(Compiled.MakeEffectiveSlot(Entry)));
			Binding.Slot = Set->OwnedSlots.Last().Get();
		}
		BindingEntry.Add(EntryIndex);
	}

	// 4. The timeline registry in the compiled order, then the adoptable timelines. Compiled.Registry entries that are not combat
	// timelines keep no slot; the remap sends their bindings to "plain ability"
	TArray<int32> RegistryRemap;
	RegistryRemap.Init(INDEX_NONE, Compiled.Registry.Num());
	if (Compiled.Registry.Num() > FGSTN_CompiledBindingTable::MaxRegistryEntries)
	{
		Error(FString::Printf(TEXT("%d timeline registry entries, the limit is %d"), Compiled.Registry.Num(), FGSTN_CompiledBindingTable::MaxRegistryEntries));
	}
	for (int32 Index = 0; Index < Compiled.Registry.Num() && Index < FGSTN_CompiledBindingTable::MaxRegistryEntries; ++Index)
	{
		UClass* Class = Input.LoadClass(Compiled.Registry[Index].Ability);
		if (Class && Class->IsChildOf(UGSTN_GA_BaseCombat::StaticClass()))
		{
			RegistryRemap[Index] = Set->AddTimelineClass(Class, 0, 0, false);
		}
		else
		{
			Error(FString::Printf(TEXT("Timeline registry entry %d (%s) is not a combat timeline ability and is dropped"), Index, *Compiled.Registry[Index].Ability.ToString()));
		}
	}

	// 5. The per-binding facts, and the tier of each registry entry from the first command bound to it
	TArray<bool> TierSet;
	TierSet.Init(false, Set->TimelineRegistry.Num());
	for (int32 BindingIndex = 0; BindingIndex < Set->Table.Bindings.Num(); ++BindingIndex)
	{
		const FGSTN_CompiledEntry& Entry = Compiled.Entries[BindingEntry[BindingIndex]];
		const FGSTN_CompiledBinding& Binding = Set->Table.Bindings[BindingIndex];
		const int32 Registry = RegistryRemap.IsValidIndex(Entry.TimelineIndex) ? RegistryRemap[Entry.TimelineIndex] : INDEX_NONE;
		Set->BindingTimeline.Add(static_cast<int16>(Registry));
		if (Registry != INDEX_NONE && !TierSet[Registry])
		{
			TierSet[Registry] = true;
			const int32 Tier = Input.InputSettings->GetCommandTier(Entry.Command);
			Set->TimelineTier[Registry] = Tier;
			Set->TimelineDefaultOverride[Registry] = Input.CombatSettings->GetDefaultOverrideFrames(Tier);
		}

		const UClass* AbilityClass = Binding.Ability.Get();
		const UGSTN_GA_PlatformJump* JumpDefault = AbilityClass->IsChildOf(UGSTN_GA_PlatformJump::StaticClass()) ? AbilityClass->GetDefaultObject<UGSTN_GA_PlatformJump>() : nullptr;
		Set->BindingJumpRole.Add(JumpDefault ? static_cast<uint8>(JumpDefault->Role) + 1 : 0);
		if (JumpDefault && JumpDefault->Role == EGSTN_PlatformJumpRole::Jump && !Set->TableJumpData)
		{
			Set->TableJumpData = JumpDefault;
		}
	}
	Set->AppendAdoptable(Input.Adoptable);

	// 6. The watched tags of the reset rules: the host's global rules, sorted by name, at most 64 (a bit mask in the sync state)
	{
		FGameplayTagContainer Collected;
		FGSTN_BufferResetRules::GetWatchedTags(Input.GlobalResetRules, Collected);
		for (const FGameplayTag& Tag : Collected)
		{
			Set->Table.WatchedTags.Add(Tag);
		}
		Set->Table.WatchedTags.Sort(TagNameLess);
		if (Set->Table.WatchedTags.Num() > FGSTN_CommandTable::MaxWatchedTags)
		{
			Error(FString::Printf(TEXT("%d watched tags, only the first %d are watched"), Set->Table.WatchedTags.Num(), FGSTN_CommandTable::MaxWatchedTags));
			Set->Table.WatchedTags.SetNum(FGSTN_CommandTable::MaxWatchedTags);
		}
	}

	// 7. Extensions in id order, then the candidates (after them, so an extension's bindings count)
	TArray<FName> ExtensionIds;
	Extensions().GetKeys(ExtensionIds);
	ExtensionIds.Sort([](const FName& A, const FName& B) { return A.Compare(B) < 0; });
	for (const FName& Id : ExtensionIds)
	{
		Extensions()[Id](Compiled, *Set);
	}
	for (int32 CommandIndex = 0; CommandIndex < Set->Table.Commands.Num(); ++CommandIndex)
	{
		if (Set->Table.Commands[CommandIndex].NumBindings > 0)
		{
			Set->Table.CandidateSlots.Add(Set->Table.Commands[CommandIndex].Slot);
			Set->Table.CandidateCommands.Add(CommandIndex);
		}
	}

	Set->Checksum = Set->ComputeChecksum();
	return Set;
}
