// Test stand-in (reflected, so it compiles in every configuration; it needs no test-only tag).

#pragma once

#include "CoreMinimal.h"
#include "Bindings/GSTN_KeyBindingsSubsystem.h"
#include "GSTN_TestKeyBindingsSubsystem.generated.h"

/** Key bindings subsystem over an in-memory store shared by slot name, standing in for the disk of one machine */
UCLASS(Transient, NotBlueprintable)
class UGSTN_TestKeyBindingsSubsystem : public UGSTN_KeyBindingsSubsystem
{
	GENERATED_BODY()

public:
	int32 SlotIndex = 0;

	static TMap<FString, TArray<uint8>>& Disk()
	{
		static TMap<FString, TArray<uint8>> Store;
		return Store;
	}

protected:
	virtual bool ReadStorage(TArray<uint8>& OutBytes) const override
	{
		const TArray<uint8>* Found = Disk().Find(GetSaveSlotName());
		if (Found)
		{
			OutBytes = *Found;
		}
		return Found != nullptr;
	}

	virtual bool WriteStorage(const TArray<uint8>& Bytes) const override
	{
		Disk().Add(GetSaveSlotName(), Bytes);
		return true;
	}

	virtual int32 GetPlayerSlotIndex() const override { return SlotIndex; }
};
