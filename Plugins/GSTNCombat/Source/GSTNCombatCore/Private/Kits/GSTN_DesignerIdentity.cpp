// Pure combat logic (no GAS, no Network Prediction).

#include "Kits/GSTN_DesignerIdentity.h"

#if WITH_EDITOR
#include "Engine/AssetManager.h"
#endif

void FGSTN_DesignerIdentity::GetIdentityLeaves(const FGameplayTagContainer& Tags, const FGameplayTag& Root, TArray<FGameplayTag>& OutLeaves)
{
	if (!Root.IsValid())
	{
		return;
	}
	for (const FGameplayTag& Tag : Tags)
	{
		if (Tag != Root && Tag.MatchesTag(Root))
		{
			OutLeaves.Add(Tag);
		}
	}
}

bool FGSTN_DesignerIdentity::ValidateExactlyOne(const FGameplayTagContainer& Tags, const FGameplayTag& Root, const FString& What, TArray<FText>& OutErrors)
{
	if (!Root.IsValid())
	{
		OutErrors.Add(FText::FromString(FString::Printf(TEXT("The identity root of a %s is not a registered tag."), *What)));
		return false;
	}

	TArray<FGameplayTag> Leaves;
	GetIdentityLeaves(Tags, Root, Leaves);
	if (Leaves.Num() == 1)
	{
		return true;
	}

	if (Leaves.IsEmpty())
	{
		const bool bBareRoot = Tags.HasTagExact(Root);
		OutErrors.Add(FText::FromString(FString::Printf(TEXT("A %s needs exactly one identity tag under '%s' and has none%s."), *What, *Root.ToString(),
			bBareRoot ? TEXT(" (the bare root is not an identity: pick a leaf)") : TEXT(""))));
	}
	else
	{
		TArray<FString> Names;
		for (const FGameplayTag& Leaf : Leaves)
		{
			Names.Add(Leaf.ToString());
		}
		OutErrors.Add(FText::FromString(FString::Printf(TEXT("A %s needs exactly one identity tag under '%s' and has %d: %s. Other tags may only classify it."),
			*What, *Root.ToString(), Leaves.Num(), *FString::Join(Names, TEXT(", ")))));
	}
	return false;
}

bool FGSTN_DesignerIdentity::FindDuplicateKeys(TConstArrayView<FGSTN_UniqueKeyEntry> Entries, const FString& What, TArray<FText>& OutErrors)
{
	// First-seen order, so the messages do not depend on hashing
	TArray<FName> Keys;
	TMap<FName, TArray<FName>> AssetsByKey;
	for (const FGSTN_UniqueKeyEntry& Entry : Entries)
	{
		if (Entry.Key.IsNone())
		{
			continue;
		}
		TArray<FName>* Assets = AssetsByKey.Find(Entry.Key);
		if (!Assets)
		{
			Keys.Add(Entry.Key);
			Assets = &AssetsByKey.Add(Entry.Key);
		}
		Assets->Add(Entry.Asset);
	}

	bool bUnique = true;
	for (const FName& Key : Keys)
	{
		const TArray<FName>& Assets = AssetsByKey.FindChecked(Key);
		if (Assets.Num() < 2)
		{
			continue;
		}
		bUnique = false;
		TArray<FString> Names;
		for (const FName& Asset : Assets)
		{
			Names.Add(Asset.ToString());
		}
		OutErrors.Add(FText::FromString(FString::Printf(TEXT("%s '%s' is used by %d assets (it must be unique per type): %s."), *What, *Key.ToString(), Assets.Num(), *FString::Join(Names, TEXT(", ")))));
	}
	return bUnique;
}

#if WITH_EDITOR
void FGSTN_DesignerIdentity::GatherFromPrimaryAssets(FPrimaryAssetType Type, const UObject& Self, TFunctionRef<FName(const UObject&)> GetKey, TArray<FGSTN_UniqueKeyEntry>& OutEntries)
{
	if (!UAssetManager::IsInitialized())
	{
		return;
	}

	const UAssetManager& Manager = UAssetManager::Get();
	TArray<FPrimaryAssetId> Ids;
	Manager.GetPrimaryAssetIdList(Type, Ids);
	const FSoftObjectPath SelfPath(&Self);
	for (const FPrimaryAssetId& Id : Ids)
	{
		const FSoftObjectPath Path = Manager.GetPrimaryAssetPath(Id);
		if (Path.IsNull() || Path == SelfPath)
		{
			continue;
		}
		if (const UObject* Other = Path.TryLoad())
		{
			OutEntries.Add({ Other->GetFName(), GetKey(*Other) });
		}
	}
}
#endif

bool UGSTN_DesignerIdentityUtils::FindIdentityTag(const FGameplayTagContainer& Tags, FGameplayTag Root, FGameplayTag& OutIdentity)
{
	TArray<FGameplayTag> Leaves;
	FGSTN_DesignerIdentity::GetIdentityLeaves(Tags, Root, Leaves);
	OutIdentity = Leaves.Num() == 1 ? Leaves[0] : FGameplayTag();
	return Leaves.Num() == 1;
}
