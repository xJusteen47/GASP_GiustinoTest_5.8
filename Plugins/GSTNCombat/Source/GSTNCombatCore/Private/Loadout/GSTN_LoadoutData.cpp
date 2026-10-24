// Pure combat logic (no GAS, no Network Prediction).

#include "Loadout/GSTN_LoadoutData.h"
#include "GameplayTagsManager.h"

namespace
{
	/**
	 * Reads or writes a list length. Bounded on read, and on write too: a longer list writes its first MaxEntries entries (the
	 * caller loops over Count, not over the container), so a writer can never produce a stream its own reader rejects.
	 */
	bool SerializeCount(FArchive& Ar, int32& Count, int32 Max = FGSTN_LoadoutData::MaxEntries)
	{
		if (Ar.IsSaving())
		{
			Count = FMath::Min(Count, Max);
		}
		uint32 Value = static_cast<uint32>(Count);
		Ar.SerializeIntPacked(Value);
		if (Ar.IsLoading())
		{
			if (Value > static_cast<uint32>(Max))
			{
				Count = 0;
				return false;
			}
			Count = static_cast<int32>(Value);
		}
		return true;
	}

	/** Asset ids travel as text: they are rare (once per loadout change) and text survives type renames */
	void SerializeAssetId(FArchive& Ar, FPrimaryAssetId& Id)
	{
		FString Text = Ar.IsSaving() && Id.IsValid() ? Id.ToString() : FString();
		Ar << Text;
		if (Ar.IsLoading())
		{
			Id = Text.IsEmpty() ? FPrimaryAssetId() : FPrimaryAssetId::FromString(Text);
		}
	}

	/**
	 * Tags travel as names in both formats: the engine's net format depends on the project's tag replication mode and, for plain
	 * names, on a package map, and a loadout is sent rarely. A tag that no longer exists is dropped on load instead of
	 * corrupting the stream.
	 */
	void SerializeTag(FArchive& Ar, FGameplayTag& Tag)
	{
		FString Name = Ar.IsSaving() && Tag.IsValid() ? Tag.ToString() : FString();
		Ar << Name;
		if (Ar.IsLoading())
		{
			Tag = Name.IsEmpty() ? FGameplayTag() : UGameplayTagsManager::Get().RequestGameplayTag(FName(*Name), false);
		}
	}

	bool SerializeAssetIdList(FArchive& Ar, TArray<FPrimaryAssetId>& Ids)
	{
		int32 Count = Ids.Num();
		if (!SerializeCount(Ar, Count))
		{
			return false;
		}
		if (Ar.IsLoading())
		{
			Ids.Reset();
			Ids.SetNum(Count);
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			SerializeAssetId(Ar, Ids[Index]);
		}
		return true;
	}
}

bool FGSTN_LoadoutData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = SerializeInternal(Ar, true);
	return true;
}

bool FGSTN_LoadoutData::Serialize(FArchive& Ar)
{
	int32 Version = SaveVersion;
	Ar << Version;
	if (Ar.IsLoading() && (Version < 1 || Version > SaveVersion))
	{
		// Written by a newer build: nothing can be read safely, so the data stays empty and the archive is flagged
		*this = FGSTN_LoadoutData();
		Ar.SetError();
		return true;
	}
	if (!SerializeInternal(Ar, Version >= 2))
	{
		Ar.SetError();
	}
	return true;
}

bool FGSTN_LoadoutData::SerializeInternal(FArchive& Ar, bool bWithExtension)
{
	if (Ar.IsLoading())
	{
		*this = FGSTN_LoadoutData();
	}

	SerializeAssetId(Ar, PrimaryKit);
	if (!SerializeAssetIdList(Ar, AdditionalWeapons) || !SerializeAssetIdList(Ar, Supers))
	{
		return false;
	}

	int32 CustomisationCount = SlotCustomisations.Num();
	if (!SerializeCount(Ar, CustomisationCount))
	{
		return false;
	}
	if (Ar.IsLoading())
	{
		SlotCustomisations.SetNum(CustomisationCount);
	}
	for (int32 CustomisationIndex = 0; CustomisationIndex < CustomisationCount; ++CustomisationIndex)
	{
		FGSTN_SlotCustomisation& Customisation = SlotCustomisations[CustomisationIndex];
		// Writing never changes the object: the clamped index goes on the wire, the member keeps its value
		uint32 WeaponIndexValue = static_cast<uint32>(FMath::Clamp(Customisation.WeaponIndex, 0, MaxEntries));
		Ar.SerializeIntPacked(WeaponIndexValue);
		if (Ar.IsLoading())
		{
			Customisation.WeaponIndex = static_cast<int32>(FMath::Min<uint32>(WeaponIndexValue, MaxEntries));
		}
		SerializeTag(Ar, Customisation.Command);

		FString ClassPath = Ar.IsSaving() ? Customisation.AlternativeAbility.ToSoftObjectPath().ToString() : FString();
		Ar << ClassPath;
		if (Ar.IsLoading())
		{
			Customisation.AlternativeAbility = TSoftClassPtr<UObject>(FSoftObjectPath(ClassPath));
		}
	}
	if (Ar.IsLoading())
	{
		// A customisation whose command no longer exists cannot be applied
		SlotCustomisations.RemoveAll([](const FGSTN_SlotCustomisation& Customisation) { return !Customisation.Command.IsValid(); });
	}

	// Tag-name order (lexical, not FastLess: FName indices differ between processes): the same loadout is the same bytes
	// whatever the insertion order of the map or the machine
	TArray<FGameplayTag> Keys;
	if (Ar.IsSaving())
	{
		DesignerValues.GenerateKeyArray(Keys);
		Keys.Sort([](const FGameplayTag& A, const FGameplayTag& B) { return A.GetTagName().LexicalLess(B.GetTagName()); });
	}
	int32 ValueCount = Keys.Num();
	if (!SerializeCount(Ar, ValueCount))
	{
		return false;
	}
	if (Ar.IsLoading())
	{
		Keys.SetNum(ValueCount);
	}
	for (int32 Index = 0; Index < ValueCount; ++Index)
	{
		FGameplayTag& Key = Keys[Index];
		SerializeTag(Ar, Key);
		FGSTN_LoadoutValue Value;
		if (Ar.IsSaving())
		{
			Value = DesignerValues.FindChecked(Key);
		}
		Ar << Value.Value;
		if (Ar.IsLoading() && Key.IsValid())
		{
			DesignerValues.Add(Key, Value);
		}
	}

	SerializeAssetId(Ar, Skin);

	if (bWithExtension)
	{
		// Layout version, payload length, payload. A reader that does not know the layout keeps the payload as it is
		uint8 Layout = ExtensionLayoutVersion;
		Ar << Layout;
		int32 ExtensionBytes = ReservedExtension.Num();
		if (!SerializeCount(Ar, ExtensionBytes, MaxExtensionBytes))
		{
			return false;
		}
		if (Ar.IsLoading())
		{
			ReservedExtension.SetNumUninitialized(ExtensionBytes);
		}
		if (ExtensionBytes > 0)
		{
			Ar.Serialize(ReservedExtension.GetData(), ExtensionBytes);
		}
	}
	return !Ar.IsError();
}

bool FGSTN_LoadoutData::operator==(const FGSTN_LoadoutData& Other) const
{
	if (PrimaryKit != Other.PrimaryKit || Skin != Other.Skin || AdditionalWeapons != Other.AdditionalWeapons
		|| Supers != Other.Supers || SlotCustomisations != Other.SlotCustomisations || DesignerValues.Num() != Other.DesignerValues.Num())
	{
		return false;
	}
	for (const TPair<FGameplayTag, FGSTN_LoadoutValue>& Pair : DesignerValues)
	{
		const FGSTN_LoadoutValue* Found = Other.DesignerValues.Find(Pair.Key);
		if (!Found || !(*Found == Pair.Value))
		{
			return false;
		}
	}
	return true;
}
