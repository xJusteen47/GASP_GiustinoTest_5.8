// Pure combat logic (no GAS, no Network Prediction).

#include "Loadout/GSTN_LoadoutData.h"
#include "GameplayTagsManager.h"

namespace
{
	/** Reads or writes a list length, bounded on read */
	bool SerializeCount(FArchive& Ar, int32& Count)
	{
		uint32 Value = static_cast<uint32>(Count);
		Ar.SerializeIntPacked(Value);
		if (Ar.IsLoading())
		{
			if (Value > static_cast<uint32>(FGSTN_LoadoutData::MaxEntries))
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
		for (FPrimaryAssetId& Id : Ids)
		{
			SerializeAssetId(Ar, Id);
		}
		return true;
	}
}

bool FGSTN_LoadoutData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = SerializeInternal(Ar);
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
	if (!SerializeInternal(Ar))
	{
		Ar.SetError();
	}
	return true;
}

bool FGSTN_LoadoutData::SerializeInternal(FArchive& Ar)
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
	for (FGSTN_SlotCustomisation& Customisation : SlotCustomisations)
	{
		uint32 WeaponIndexValue = static_cast<uint32>(FMath::Max(Customisation.WeaponIndex, 0));
		Ar.SerializeIntPacked(WeaponIndexValue);
		Customisation.WeaponIndex = static_cast<int32>(FMath::Min<uint32>(WeaponIndexValue, MaxEntries));
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
