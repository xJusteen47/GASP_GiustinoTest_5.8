// Pure combat logic (no GAS, no Network Prediction).

#include "Loadout/GSTN_LoadoutExtensionData.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNLoadoutExtension, Log, All);

namespace
{
	TMap<FName, TWeakObjectPtr<const UScriptStruct>>& GetTypesById()
	{
		static TMap<FName, TWeakObjectPtr<const UScriptStruct>> Types;
		return Types;
	}

	bool IsExtensionType(const UScriptStruct* Struct)
	{
		return Struct && Struct != FGSTN_LoadoutExtensionData::StaticStruct() && Struct->IsChildOf(FGSTN_LoadoutExtensionData::StaticStruct());
	}

	/** One entry's bytes: the type's own format when it has one, else its reflected properties */
	bool SerializeEntry(FArchive& Ar, const UScriptStruct* Struct, uint8* Memory)
	{
		FGSTN_LoadoutExtensionData* Data = reinterpret_cast<FGSTN_LoadoutExtensionData*>(Memory);
		if (!Data->SerializePayload(Ar))
		{
			Struct->SerializeBin(Ar, Memory);
		}
		return !Ar.IsError();
	}
}

bool FGSTN_LoadoutExtensionTypes::Register(FName WireId, const UScriptStruct* Struct)
{
	if (WireId.IsNone() || !IsExtensionType(Struct))
	{
		return false;
	}
	TWeakObjectPtr<const UScriptStruct>& Slot = GetTypesById().FindOrAdd(WireId);
	if (Slot.IsValid() && Slot.Get() != Struct)
	{
		UE_LOG(LogGSTNLoadoutExtension, Warning, TEXT("Wire id %s is already used by %s: %s not registered"), *WireId.ToString(),
			*Slot->GetName(), *Struct->GetName());
		return false;
	}
	Slot = Struct;
	return true;
}

void FGSTN_LoadoutExtensionTypes::Unregister(FName WireId)
{
	GetTypesById().Remove(WireId);
}

FName FGSTN_LoadoutExtensionTypes::GetWireId(const UScriptStruct* Struct)
{
	if (!Struct)
	{
		return NAME_None;
	}
	for (const TPair<FName, TWeakObjectPtr<const UScriptStruct>>& Pair : GetTypesById())
	{
		if (Pair.Value.Get() == Struct)
		{
			return Pair.Key;
		}
	}
	return FName(*Struct->GetPathName());
}

const UScriptStruct* FGSTN_LoadoutExtensionTypes::FindType(FName WireId)
{
	if (const TWeakObjectPtr<const UScriptStruct>* Found = GetTypesById().Find(WireId))
	{
		return Found->Get();
	}
	// An unregistered type travels under its path. Only a path is looked up, never a short name, so a stream cannot pick an
	// arbitrary struct of the build; the type must also derive from the extension base
	const FString Path = WireId.ToString();
	if (!Path.StartsWith(TEXT("/")))
	{
		return nullptr;
	}
	const UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *Path);
	return IsExtensionType(Struct) ? Struct : nullptr;
}

void GSTNLoadoutExtension::WritePayload(const TArray<FInstancedStruct>& Extensions, TArray<uint8>& OutBytes, int32 MaxBytes)
{
	OutBytes.Reset();

	// Each entry is encoded on its own first, so an entry that would overflow the bound is left out whole, never cut
	TArray<TArray<uint8>> Entries;
	int32 Total = 0;
	for (const FInstancedStruct& Extension : Extensions)
	{
		const UScriptStruct* Struct = Extension.GetScriptStruct();
		if (!IsExtensionType(Struct) || Entries.Num() >= FGSTN_LoadoutData::MaxEntries)
		{
			continue;
		}
		TArray<uint8> Payload;
		{
			FMemoryWriter PayloadWriter(Payload);
			SerializeEntry(PayloadWriter, Struct, const_cast<uint8*>(Extension.GetMemory()));
		}
		TArray<uint8> Entry;
		{
			FMemoryWriter EntryWriter(Entry);
			FString WireId = FGSTN_LoadoutExtensionTypes::GetWireId(Struct).ToString();
			EntryWriter << WireId;
			uint32 Length = static_cast<uint32>(Payload.Num());
			EntryWriter.SerializeIntPacked(Length);
			EntryWriter.Serialize(Payload.GetData(), Payload.Num());
		}
		// 5 bytes is the most a packed count can take
		if (Total + Entry.Num() + 5 > MaxBytes)
		{
			UE_LOG(LogGSTNLoadoutExtension, Warning, TEXT("Loadout extension %s left out: the extension block is full (%d bytes)"),
				*Struct->GetName(), MaxBytes);
			continue;
		}
		Total += Entry.Num();
		Entries.Add(MoveTemp(Entry));
	}

	if (Entries.Num() == 0)
	{
		return;
	}
	FMemoryWriter Writer(OutBytes);
	uint32 Count = static_cast<uint32>(Entries.Num());
	Writer.SerializeIntPacked(Count);
	for (TArray<uint8>& Entry : Entries)
	{
		Writer.Serialize(Entry.GetData(), Entry.Num());
	}
}

bool GSTNLoadoutExtension::ReadPayload(const TArray<uint8>& Bytes, TArray<FInstancedStruct>& OutExtensions)
{
	OutExtensions.Reset();
	if (Bytes.Num() == 0)
	{
		return true;
	}
	FMemoryReader Reader(Bytes);
	uint32 Count = 0;
	Reader.SerializeIntPacked(Count);
	if (Reader.IsError() || Count > static_cast<uint32>(FGSTN_LoadoutData::MaxEntries))
	{
		return false;
	}
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		FString WireId;
		Reader << WireId;
		uint32 Length = 0;
		Reader.SerializeIntPacked(Length);
		if (Reader.IsError() || Length > static_cast<uint32>(Reader.TotalSize() - Reader.Tell()))
		{
			OutExtensions.Reset();
			return false;
		}
		TArray<uint8> Payload;
		Payload.SetNumUninitialized(static_cast<int32>(Length));
		Reader.Serialize(Payload.GetData(), Payload.Num());

		const UScriptStruct* Struct = FGSTN_LoadoutExtensionTypes::FindType(FName(*WireId));
		if (!Struct)
		{
			// The type's plugin is not in this build: its entry is skipped by its length, the rest of the loadout is unaffected
			UE_LOG(LogGSTNLoadoutExtension, Warning, TEXT("Loadout extension %s dropped: no such type in this build"), *WireId);
			continue;
		}
		FInstancedStruct Entry;
		Entry.InitializeAs(Struct);
		FMemoryReader PayloadReader(Payload);
		if (!SerializeEntry(PayloadReader, Struct, Entry.GetMutableMemory()) || PayloadReader.Tell() != PayloadReader.TotalSize())
		{
			UE_LOG(LogGSTNLoadoutExtension, Warning, TEXT("Loadout extension %s dropped: its bytes do not match this build's type"), *WireId);
			continue;
		}
		OutExtensions.Add(MoveTemp(Entry));
	}
	return !Reader.IsError();
}
