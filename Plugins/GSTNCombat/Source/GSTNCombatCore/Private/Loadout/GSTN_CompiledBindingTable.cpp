// Pure combat logic (no GAS, no Network Prediction).

#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputSlotValidation.h"
#include "Kits/GSTN_SuperInputScheme.h"
#include "NativeGameplayTags.h"
#include "Containers/StringConv.h"

namespace
{
	// The commands of the super slots. They are command vocabulary like the families next to them, so they are native here
	// and the tier of Command.Super ranks them. A kit with more super slots than this has no command for the extra ones.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SuperSlot1, "Command.Super.Slot1", "Command of the first super slot; started by the kit's super input scheme");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SuperSlot2, "Command.Super.Slot2", "Command of the second super slot");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SuperSlot3, "Command.Super.Slot3", "Command of the third super slot");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SuperSlot4, "Command.Super.Slot4", "Command of the fourth super slot");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SuperSlot5, "Command.Super.Slot5", "Command of the fifth super slot");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SuperSlot6, "Command.Super.Slot6", "Command of the sixth super slot");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SuperSlot7, "Command.Super.Slot7", "Command of the seventh super slot");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SuperSlot8, "Command.Super.Slot8", "Command of the eighth super slot");

	const FNativeGameplayTag* const SuperSlotTags[FGSTN_BindingTableCompiler::MaxSuperSlots] = {
		&SuperSlot1, &SuperSlot2, &SuperSlot3, &SuperSlot4, &SuperSlot5, &SuperSlot6, &SuperSlot7, &SuperSlot8
	};

	/** Case-sensitive on purpose: the order must not depend on how the platform folds case */
	int32 ComparePaths(const FSoftObjectPath& A, const FSoftObjectPath& B)
	{
		return A.ToString().Compare(B.ToString(), ESearchCase::CaseSensitive);
	}

	bool CommandLess(const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.GetTagName().Compare(B.GetTagName()) < 0;
	}

	/** Two slots are the same slot when everything they hold is the same, which their exported text says */
	FString SlotKey(const FGSTN_InputSlotSpec& Spec)
	{
		FString Text;
		FGSTN_InputSlotSpec::StaticStruct()->ExportText(Text, &Spec, nullptr, nullptr, PPF_None, nullptr);
		return Text;
	}

	/** A row on its way to the table: the slot is pooled and the registry index set once every row is known */
	struct FPendingEntry
	{
		FGSTN_CompiledEntry Entry;
		FGSTN_InputSlotSpec Slot;
		FSoftObjectPath TimelineOverride;
	};

	void SortByCommand(TArray<FPendingEntry>& Pending)
	{
		// Stable: rows of one command keep the order the data lists them in
		Pending.StableSort([](const FPendingEntry& A, const FPendingEntry& B) { return CommandLess(A.Entry.Command, B.Entry.Command); });
	}

	/** Appends integers and length-prefixed UTF-8 strings: the byte form of a table */
	struct FByteSink
	{
		TArray<uint8>& Bytes;

		void Int(int32 Value) { Bytes.Append(reinterpret_cast<const uint8*>(&Value), sizeof(Value)); }

		void Str(const FString& Text)
		{
			const FTCHARToUTF8 Utf8(*Text);
			Int(Utf8.Length());
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		}

		void Path(const FSoftObjectPath& Value) { Str(Value.ToString()); }

		void Tag(const FGameplayTag& Value) { Str(Value.IsValid() ? Value.GetTagName().ToString() : FString()); }

		/** Name order, so the order tags were added in does not change the bytes */
		void Tags(const FGameplayTagContainer& Value)
		{
			TArray<FName> Names;
			for (const FGameplayTag& Entry : Value)
			{
				Names.Add(Entry.GetTagName());
			}
			Names.Sort([](const FName& A, const FName& B) { return A.Compare(B) < 0; });
			Int(Names.Num());
			for (const FName& Name : Names)
			{
				Str(Name.ToString());
			}
		}
	};
}

FGameplayTag FGSTN_BindingTableCompiler::GetSuperSlotCommand(int32 Index)
{
	return Index >= 0 && Index < MaxSuperSlots ? SuperSlotTags[Index]->GetTag() : FGameplayTag();
}

FGSTN_InputSlotSpec FGSTN_CompiledBindingTable::MakeEffectiveSlot(const FGSTN_CompiledEntry& Entry) const
{
	FGSTN_InputSlotSpec Spec = Slots.IsValidIndex(Entry.SlotIndex) ? Slots[Entry.SlotIndex] : FGSTN_InputSlotSpec();
	Spec.PriorityOffset += Entry.PriorityOffset;
	if (Entry.BufferFramesOverride >= 0)
	{
		Spec.BufferFramesOverride = Entry.BufferFramesOverride;
	}
	return Spec;
}

int32 FGSTN_CompiledBindingTable::FindRegistryIndex(const FSoftObjectPath& Ability, const FSoftObjectPath& TimelineOverride) const
{
	return Registry.IndexOfByPredicate([&](const FGSTN_TimelineRegistryEntry& Entry) { return Entry.Ability == Ability && Entry.TimelineOverride == TimelineOverride; });
}

void FGSTN_CompiledBindingTable::WriteBytes(TArray<uint8>& OutBytes) const
{
	OutBytes.Reset();
	FByteSink Sink{ OutBytes };

	Sink.Int(Revision);
	Sink.Int(WeaponCount);

	Sink.Int(Slots.Num());
	for (const FGSTN_InputSlotSpec& Slot : Slots)
	{
		Sink.Str(SlotKey(Slot));
	}

	Sink.Int(Entries.Num());
	for (const FGSTN_CompiledEntry& Entry : Entries)
	{
		Sink.Int(static_cast<int32>(Entry.Source));
		Sink.Int(Entry.SourceIndex);
		Sink.Tag(Entry.Command);
		Sink.Tags(Entry.Requirements.RequireTags);
		Sink.Tags(Entry.Requirements.IgnoreTags);
		Sink.Int(Entry.SlotIndex);
		Sink.Path(Entry.Ability);
		Sink.Int(Entry.TimelineIndex);
		Sink.Int(Entry.PriorityOffset);
		Sink.Int(Entry.BufferFramesOverride);
		Sink.Int(Entry.Priority.Scores.Num());
		for (const int32 Score : Entry.Priority.Scores)
		{
			Sink.Int(Score);
		}
	}

	Sink.Int(Registry.Num());
	for (const FGSTN_TimelineRegistryEntry& Entry : Registry)
	{
		Sink.Path(Entry.Ability);
		Sink.Path(Entry.TimelineOverride);
	}

	Sink.Int(GrantedAbilities.Num());
	for (const FSoftObjectPath& Ability : GrantedAbilities)
	{
		Sink.Path(Ability);
	}
}

FGSTN_CompiledBindingTable FGSTN_BindingTableCompiler::Compile(const FGSTN_CompileInput& Input, const UGSTN_InputSettings& Settings, FGSTN_CompileReport* OutReport)
{
	FGSTN_CompileReport LocalReport;
	FGSTN_CompileReport& Report = OutReport ? *OutReport : LocalReport;

	FGSTN_CompiledBindingTable Table;
	Table.Revision = Input.Revision;
	Table.WeaponCount = Input.Weapons.Num();

	// The kit's command set: base minus removed plus added. Compose keeps the slots alive inside the base asset and the kit
	TArray<const FGSTN_InputSlotSpec*> SetSpecs;
	Input.Kit.CommandSet.Compose(Input.Kit.BaseSet, SetSpecs);
	TMap<FGameplayTag, const FGSTN_InputSlotSpec*> SetByCommand;
	for (const FGSTN_InputSlotSpec* Spec : SetSpecs)
	{
		if (Spec->Command.IsValid() && !SetByCommand.Contains(Spec->Command))
		{
			SetByCommand.Add(Spec->Command, Spec);
		}
	}

	const auto AddBindings = [&](EGSTN_EntrySource Source, int32 SourceIndex, const TArray<FGSTN_BindingInput>& Bindings, const FString& Owner, TArray<FPendingEntry>& Out)
	{
		for (int32 Index = 0; Index < Bindings.Num(); ++Index)
		{
			const FGSTN_BindingInput& Binding = Bindings[Index];
			const FString Where = FString::Printf(TEXT("%s binding %d"), *Owner, Index);

			if (!Binding.Command.IsValid())
			{
				Report.AddError(FString::Printf(TEXT("%s has no command and is skipped."), *Where));
				continue;
			}
			if (Binding.Ability.IsNull())
			{
				// Binds nothing: it only hides what a parent bound, so there is no row for it
				continue;
			}
			if (Input.Kit.CommandSet.RemovedCommands.HasTagExact(Binding.Command))
			{
				Report.AddError(FString::Printf(TEXT("%s binds %s, which the kit's command set removes, and is skipped."), *Where, *Binding.Command.ToString()));
				continue;
			}

			const FGSTN_InputSlotSpec* Slot = nullptr;
			if (Binding.bHasSlotOverride)
			{
				if (Binding.SlotOverride.Command != Binding.Command)
				{
					Report.AddError(FString::Printf(TEXT("%s overrides the slot with another command (%s) than it binds (%s) and is skipped."), *Where, *Binding.SlotOverride.Command.ToString(), *Binding.Command.ToString()));
					continue;
				}
				Slot = &Binding.SlotOverride;
			}
			else if (const FGSTN_InputSlotSpec* const* Found = SetByCommand.Find(Binding.Command))
			{
				Slot = *Found;
			}
			if (!Slot)
			{
				Report.AddError(FString::Printf(TEXT("%s binds %s, which is not in the kit's command set and has no slot override, and is skipped."), *Where, *Binding.Command.ToString()));
				continue;
			}

			FPendingEntry& Pending = Out.AddDefaulted_GetRef();
			Pending.Slot = *Slot;
			Pending.TimelineOverride = Binding.TimelineOverride;
			Pending.Entry.Source = Source;
			Pending.Entry.SourceIndex = static_cast<uint8>(SourceIndex);
			Pending.Entry.Command = Binding.Command;
			Pending.Entry.Requirements = Binding.Requirements;
			Pending.Entry.Ability = Binding.Ability;
			Pending.Entry.PriorityOffset = Binding.PriorityOffset;
			Pending.Entry.BufferFramesOverride = Binding.BufferFramesOverride;
		}
	};

	TArray<FPendingEntry> KitEntries;
	AddBindings(EGSTN_EntrySource::Kit, 0, Input.Kit.Bindings, TEXT("Kit"), KitEntries);

	TArray<TArray<FPendingEntry>> WeaponEntries;
	WeaponEntries.SetNum(Input.Weapons.Num());
	for (int32 Weapon = 0; Weapon < Input.Weapons.Num(); ++Weapon)
	{
		AddBindings(EGSTN_EntrySource::Weapon, Weapon, Input.Weapons[Weapon].Bindings, FString::Printf(TEXT("Weapon %d"), Weapon), WeaponEntries[Weapon]);
	}

	// Customisations swap the ability of every row the weapon has for the command (all its contexts), keeping the row's slot,
	// requirements and timeline override. Whether the alternative is allowed is the loadout rules' question, not the compiler's
	for (int32 Index = 0; Index < Input.Customisations.Num(); ++Index)
	{
		const FGSTN_SlotCustomisation& Customisation = Input.Customisations[Index];
		const FSoftObjectPath Alternative = Customisation.AlternativeAbility.ToSoftObjectPath();
		if (!Customisation.Command.IsValid() || Alternative.IsNull())
		{
			Report.AddWarning(FString::Printf(TEXT("Customisation %d names no command or no ability and is ignored."), Index));
			continue;
		}
		if (!WeaponEntries.IsValidIndex(Customisation.WeaponIndex))
		{
			Report.AddWarning(FString::Printf(TEXT("Customisation %d is for weapon %d, but the loadout has %d, and is ignored."), Index, Customisation.WeaponIndex, WeaponEntries.Num()));
			continue;
		}
		int32 Replaced = 0;
		for (FPendingEntry& Pending : WeaponEntries[Customisation.WeaponIndex])
		{
			if (Pending.Entry.Command == Customisation.Command)
			{
				Pending.Entry.Ability = Alternative;
				++Replaced;
			}
		}
		if (Replaced == 0)
		{
			Report.AddWarning(FString::Printf(TEXT("Customisation %d swaps %s on weapon %d, which binds nothing to it, and is ignored."), Index, *Customisation.Command.ToString(), Customisation.WeaponIndex));
		}
	}

	// Super slots. Fixed: the scheme's slot decides the input. PerSuperCode: the scheme's entry steps, then the super's own code
	TArray<FPendingEntry> SuperEntries;
	for (int32 Slot = 0; Slot < Input.Supers.Num(); ++Slot)
	{
		const FGSTN_SuperCompileInput& Super = Input.Supers[Slot];
		if (!Super.bPresent)
		{
			continue;
		}
		if (Slot >= Input.Kit.SuperSlotCount || Slot >= MaxSuperSlots)
		{
			Report.AddWarning(FString::Printf(TEXT("Super slot %d is beyond the kit's %d slots and is ignored."), Slot, Input.Kit.SuperSlotCount));
			continue;
		}
		if (Super.Ability.IsNull())
		{
			Report.AddWarning(FString::Printf(TEXT("The super in slot %d has no ability and is ignored."), Slot));
			continue;
		}
		if (!Input.SuperScheme)
		{
			Report.AddError(FString::Printf(TEXT("The super in slot %d cannot be entered: the kit has no super input scheme."), Slot));
			continue;
		}

		FGSTN_InputSlotSpec SlotSpec;
		if (Input.SuperScheme->Mode == EGSTN_SuperInputMode::FixedSlots)
		{
			if (!Input.SuperScheme->FixedSlots.IsValidIndex(Slot))
			{
				Report.AddError(FString::Printf(TEXT("The super input scheme has %d fixed slots and none for super slot %d."), Input.SuperScheme->FixedSlots.Num(), Slot));
				continue;
			}
			SlotSpec = Input.SuperScheme->FixedSlots[Slot];
			if (!SlotSpec.Command.IsValid())
			{
				SlotSpec.Command = GetSuperSlotCommand(Slot);
			}
		}
		else
		{
			SlotSpec = Input.SuperScheme->CodeEntry;
			SlotSpec.Command = GetSuperSlotCommand(Slot);
			SlotSpec.Pattern.Steps.Append(Super.InputCode.Steps);
		}

		FPendingEntry& Pending = SuperEntries.AddDefaulted_GetRef();
		Pending.TimelineOverride = Super.TimelineOverride;
		Pending.Entry.Source = EGSTN_EntrySource::Super;
		Pending.Entry.SourceIndex = static_cast<uint8>(Slot);
		Pending.Entry.Command = SlotSpec.Command;
		Pending.Entry.Requirements = Super.Requirements;
		Pending.Entry.Ability = Super.Ability;
		Pending.Slot = MoveTemp(SlotSpec);
	}

	// Order: kit, weapon 0..N, super slots; by command name inside each
	TArray<FPendingEntry> All;
	SortByCommand(KitEntries);
	All.Append(MoveTemp(KitEntries));
	for (TArray<FPendingEntry>& Weapon : WeaponEntries)
	{
		SortByCommand(Weapon);
		All.Append(MoveTemp(Weapon));
	}
	SortByCommand(SuperEntries);
	All.Append(MoveTemp(SuperEntries));

	// Timeline registry: the distinct pairs of the timeline rows, then what the kit and the weapons grant that is a timeline
	// ability, sorted and de-duplicated. Without a predicate only a row with an override is a timeline row
	const auto IsTimeline = [&Input](const FSoftObjectPath& Ability, const FSoftObjectPath& Override)
	{
		return Input.IsTimelineAbility ? Input.IsTimelineAbility(Ability) : Override.IsValid();
	};

	TArray<FGSTN_TimelineRegistryEntry> Candidates;
	for (const FPendingEntry& Pending : All)
	{
		if (IsTimeline(Pending.Entry.Ability, Pending.TimelineOverride))
		{
			Candidates.Add({ Pending.Entry.Ability, Pending.TimelineOverride });
		}
	}
	if (Input.IsTimelineAbility)
	{
		const auto AddGranted = [&](const TArray<FSoftObjectPath>& Abilities)
		{
			for (const FSoftObjectPath& Ability : Abilities)
			{
				if (!Ability.IsNull() && Input.IsTimelineAbility(Ability))
				{
					Candidates.Add({ Ability, FSoftObjectPath() });
				}
			}
		};
		AddGranted(Input.Kit.TriggeredAbilities);
		AddGranted(Input.Kit.Passives);
		for (const FGSTN_WeaponCompileInput& Weapon : Input.Weapons)
		{
			AddGranted(Weapon.PassivesWhileActive);
		}
	}
	Candidates.Sort([](const FGSTN_TimelineRegistryEntry& A, const FGSTN_TimelineRegistryEntry& B)
	{
		const int32 ByAbility = ComparePaths(A.Ability, B.Ability);
		return ByAbility != 0 ? ByAbility < 0 : ComparePaths(A.TimelineOverride, B.TimelineOverride) < 0;
	});
	for (const FGSTN_TimelineRegistryEntry& Candidate : Candidates)
	{
		if (Table.Registry.IsEmpty() || !(Table.Registry.Last().Ability == Candidate.Ability && Table.Registry.Last().TimelineOverride == Candidate.TimelineOverride))
		{
			Table.Registry.Add(Candidate);
		}
	}
	if (Table.Registry.Num() > FGSTN_CompiledBindingTable::MaxRegistryEntries)
	{
		Report.AddError(FString::Printf(TEXT("The loadout needs %d timeline registry entries and the sync state indexes %d."), Table.Registry.Num(), FGSTN_CompiledBindingTable::MaxRegistryEntries));
		Table.Registry.SetNum(FGSTN_CompiledBindingTable::MaxRegistryEntries);
	}

	// Rows: pool the slots in first-use order, rank, link the registry
	TMap<FString, int32> SlotIndexByKey;
	for (FPendingEntry& Pending : All)
	{
		const FString Key = SlotKey(Pending.Slot);
		int32* Existing = SlotIndexByKey.Find(Key);
		if (!Existing)
		{
			Existing = &SlotIndexByKey.Add(Key, Table.Slots.Num());
			Table.Slots.Add(Pending.Slot);
		}
		FGSTN_CompiledEntry Entry = MoveTemp(Pending.Entry);
		Entry.SlotIndex = *Existing;
		if (IsTimeline(Entry.Ability, Pending.TimelineOverride))
		{
			const int32 RegistryIndex = Table.FindRegistryIndex(Entry.Ability, Pending.TimelineOverride);
			Entry.TimelineIndex = RegistryIndex == INDEX_NONE ? static_cast<int16>(INDEX_NONE) : static_cast<int16>(RegistryIndex);
		}
		Entry.Priority = Table.MakeEffectiveSlot(Entry).GetPriority(Settings);
		Table.Entries.Add(MoveTemp(Entry));
	}

	// Grants: the abilities of the rows and of the kit
	TSet<FSoftObjectPath> Granted;
	for (const FGSTN_CompiledEntry& Entry : Table.Entries)
	{
		Granted.Add(Entry.Ability);
	}
	for (const FSoftObjectPath& Ability : Input.Kit.TriggeredAbilities)
	{
		if (!Ability.IsNull())
		{
			Granted.Add(Ability);
		}
	}
	for (const FSoftObjectPath& Ability : Input.Kit.Passives)
	{
		if (!Ability.IsNull())
		{
			Granted.Add(Ability);
		}
	}
	Table.GrantedAbilities = Granted.Array();
	Table.GrantedAbilities.Sort([](const FSoftObjectPath& A, const FSoftObjectPath& B) { return ComparePaths(A, B) < 0; });
	return Table;
}

void FGSTN_BindingTableCompiler::FindConflicts(const FGSTN_CompiledBindingTable& Table, const UGSTN_InputSettings& Settings, FGSTN_CompileReport& OutReport)
{
	const TArray<FGSTN_ExclusiveTagGroup> Groups = Settings.GetAllExclusiveContextGroups();

	TSet<FString> Reported;
	const auto Report = [&](bool bError, const FString& Message)
	{
		if (Reported.Contains(Message))
		{
			return;
		}
		Reported.Add(Message);
		if (bError)
		{
			OutReport.AddError(Message);
		}
		else
		{
			OutReport.AddWarning(Message);
		}
	};

	// A player is in the kit, one active weapon and the supers at once; weapons never compete with each other
	const int32 Contexts = FMath::Max(Table.WeaponCount, 1);
	for (int32 Weapon = 0; Weapon < Contexts; ++Weapon)
	{
		TArray<const FGSTN_CompiledEntry*> Active;
		for (const FGSTN_CompiledEntry& Entry : Table.Entries)
		{
			if (Entry.Source != EGSTN_EntrySource::Weapon || Entry.SourceIndex == Weapon)
			{
				Active.Add(&Entry);
			}
		}

		// One slot per pooled slot, ranked as its first row ranks it: rows of one command differ by context, not by input
		TArray<FGSTN_InputSlotSpec> Effective;
		TSet<int32> Pooled;
		for (const FGSTN_CompiledEntry* Entry : Active)
		{
			if (!Pooled.Contains(Entry->SlotIndex))
			{
				Pooled.Add(Entry->SlotIndex);
				Effective.Add(Table.MakeEffectiveSlot(*Entry));
			}
		}
		TArray<const FGSTN_InputSlotSpec*> Pointers;
		for (const FGSTN_InputSlotSpec& Spec : Effective)
		{
			Pointers.Add(&Spec);
		}
		for (const FGSTN_SlotConflict& Conflict : UGSTN_InputSlotValidation::FindConflicts(Pointers, Settings))
		{
			Report(Conflict.IsError(), Conflict.Message);
		}

		TArray<FGSTN_BindingKey> Keys;
		for (const FGSTN_CompiledEntry* Entry : Active)
		{
			Keys.Add({ Entry->Command, &Entry->Requirements });
		}
		const FString Context = Table.WeaponCount > 0 ? FString::Printf(TEXT("With weapon %d: "), Weapon) : FString(TEXT("Kit: "));
		for (const FGSTN_BindingConflict& Conflict : UGSTN_CommandBindingUtils::FindBindingConflicts(Keys, Groups))
		{
			Report(true, Context + Conflict.Message);
		}
		for (const FGSTN_BindingConflict& Impossible : UGSTN_CommandBindingUtils::FindImpossibleBindings(Keys, Groups))
		{
			Report(true, Context + Impossible.Message);
		}
	}
}
