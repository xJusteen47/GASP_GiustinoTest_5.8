// Links, ability-system half (no Network Prediction, no Mover).

#include "GSTN_LinkRegistry.h"

#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "GSTNLinkCoreModule.h"
#include "Misc/Crc.h"
#include "State/GSTN_LinkMath.h"
#include "State/GSTN_LinkState.h"

namespace GSTNLinkRegistryPrivate
{
	/** Plugin reasons registered by name before any registry exists (module startups) */
	TArray<FName>& PluginReasonNames()
	{
		static TArray<FName> Names;
		return Names;
	}

	/** Tag name order, case-sensitive so it never depends on the locale: the order the wire indices come from */
	bool TagNameLess(const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.GetTagName().ToString().Compare(B.GetTagName().ToString(), ESearchCase::CaseSensitive) < 0;
	}

	constexpr int32 FirstPluginReason = static_cast<int32>(EGSTN_LinkBreakReason::FirstPlugin);
	constexpr int32 LastGenericReason = static_cast<int32>(EGSTN_LinkBreakReason::Forced);
}

// ---- FGSTN_LinkReasonTable ----

uint8 FGSTN_LinkReasonTable::Append(const FGameplayTag& Reason)
{
	if (!Reason.IsValid())
	{
		return 0;
	}
	if (const uint8 Existing = IndexOf(Reason))
	{
		return Existing;
	}
	if (GSTNLinkRegistryPrivate::FirstPluginReason + PluginReasons.Num() > MAX_uint8)
	{
		return 0;
	}

	PluginReasons.Add(Reason);
	PluginReasons.Sort(&GSTNLinkRegistryPrivate::TagNameLess);
	return IndexOf(Reason);
}

uint8 FGSTN_LinkReasonTable::IndexOf(const FGameplayTag& Reason) const
{
	if (!Reason.IsValid())
	{
		return 0;
	}

	const EGSTN_LinkBreakReason Generic = UGSTN_LinkMath::BreakReasonFromTag(Reason);
	if (Generic != EGSTN_LinkBreakReason::None)
	{
		return static_cast<uint8>(Generic);
	}

	const int32 Found = PluginReasons.IndexOfByKey(Reason);
	return Found == INDEX_NONE ? 0 : static_cast<uint8>(GSTNLinkRegistryPrivate::FirstPluginReason + Found);
}

FGameplayTag FGSTN_LinkReasonTable::TagOf(uint8 Index) const
{
	if (Index == 0)
	{
		return FGameplayTag();
	}
	if (Index <= GSTNLinkRegistryPrivate::LastGenericReason)
	{
		return UGSTN_LinkMath::BreakReasonTag(static_cast<EGSTN_LinkBreakReason>(Index));
	}

	const int32 PluginIndex = static_cast<int32>(Index) - GSTNLinkRegistryPrivate::FirstPluginReason;
	return PluginReasons.IsValidIndex(PluginIndex) ? PluginReasons[PluginIndex] : FGameplayTag();
}

// ---- UGSTN_LinkRegistry ----

UGSTN_LinkRegistry* UGSTN_LinkRegistry::Get()
{
	UGSTN_LinkRegistry* Registry = GEngine != nullptr ? GEngine->GetEngineSubsystem<UGSTN_LinkRegistry>() : nullptr;
	if (Registry != nullptr && !Registry->bBuilt)
	{
		Registry->RebuildFromAssetManager();
	}
	return Registry;
}

void UGSTN_LinkRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Assets found after the first scan (a later plugin mount, an editor import) need an explicit RebuildFromAssetManager: the table
	// must not change under a running match, so it never rebuilds by itself after this
	TWeakObjectPtr<UGSTN_LinkRegistry> WeakThis(this);
	UAssetManager::CallOrRegister_OnCompletedInitialScan(FSimpleMulticastDelegate::FDelegate::CreateLambda([WeakThis]()
	{
		if (UGSTN_LinkRegistry* Registry = WeakThis.Get())
		{
			Registry->RebuildFromAssetManager();
		}
	}));
}

void UGSTN_LinkRegistry::Deinitialize()
{
	Entries.Reset();
	AssetDefinitions.Reset();
	CodeDefinitions.Reset();
	IndexByType.Reset();
	bBuilt = false;

	Super::Deinitialize();
}

void UGSTN_LinkRegistry::RebuildFromAssetManager()
{
	AssetDefinitions.Reset();

	if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
	{
		TArray<FSoftObjectPath> Paths;
		AssetManager->GetPrimaryAssetPathList(GSTNLinkAssetTypes::LinkType(), Paths);
		// The path order does not matter (Build sorts by tag name); sorting only makes the log of a broken set reproducible
		Paths.Sort([](const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.ToString() < B.ToString(); });

		for (const FSoftObjectPath& Path : Paths)
		{
			// Definitions are small data assets; a synchronous load keeps the table complete before anything reads it
			if (UGSTN_LinkTypeDefinition* Definition = Cast<UGSTN_LinkTypeDefinition>(Path.TryLoad()))
			{
				AssetDefinitions.Add(Definition);
			}
			else
			{
				UE_LOG(LogGSTNLink, Warning, TEXT("Link registry: '%s' is listed as a link type but did not load as a UGSTN_LinkTypeDefinition."), *Path.ToString());
			}
		}
	}

	RebuildFromSources();
}

void UGSTN_LinkRegistry::AddDefinition(UGSTN_LinkTypeDefinition* Definition)
{
	if (Definition != nullptr && !CodeDefinitions.Contains(Definition))
	{
		CodeDefinitions.Add(Definition);
		RebuildFromSources();
	}
}

void UGSTN_LinkRegistry::RemoveDefinition(UGSTN_LinkTypeDefinition* Definition)
{
	if (CodeDefinitions.Remove(Definition) > 0)
	{
		RebuildFromSources();
	}
}

void UGSTN_LinkRegistry::RebuildFromSources()
{
	TArray<UGSTN_LinkTypeDefinition*> All;
	All.Reserve(AssetDefinitions.Num() + CodeDefinitions.Num());
	for (UGSTN_LinkTypeDefinition* Definition : AssetDefinitions)
	{
		All.AddUnique(Definition);
	}
	for (UGSTN_LinkTypeDefinition* Definition : CodeDefinitions)
	{
		All.AddUnique(Definition);
	}
	Build(All);
}

void UGSTN_LinkRegistry::Build(TConstArrayView<UGSTN_LinkTypeDefinition*> Definitions, TArray<FString>* OutErrors)
{
	Entries.Reset();
	IndexByType.Reset();
	BuildErrors.Reset();

	TArray<UGSTN_LinkTypeDefinition*> Valid;
	for (UGSTN_LinkTypeDefinition* Definition : Definitions)
	{
		if (Definition == nullptr)
		{
			continue;
		}

		TArray<FString> Errors;
		Definition->Validate(Errors);
		if (Errors.Num() > 0)
		{
			for (const FString& Error : Errors)
			{
				BuildErrors.Add(FString::Printf(TEXT("Skipped %s: %s"), *Definition->GetPathName(), *Error));
			}
			continue;
		}
		Valid.Add(Definition);
	}

	// Tag name first (the wire order), then the path name, so a duplicate keeps the same winner on every machine
	Valid.Sort([](const UGSTN_LinkTypeDefinition& A, const UGSTN_LinkTypeDefinition& B)
	{
		if (A.LinkType != B.LinkType)
		{
			return GSTNLinkRegistryPrivate::TagNameLess(A.LinkType, B.LinkType);
		}
		return A.GetPathName().Compare(B.GetPathName(), ESearchCase::CaseSensitive) < 0;
	});

	for (UGSTN_LinkTypeDefinition* Definition : Valid)
	{
		if (IndexByType.Contains(Definition->LinkType))
		{
			BuildErrors.Add(FString::Printf(TEXT("Skipped %s: the link type '%s' is already defined by %s."), *Definition->GetPathName(),
				*Definition->LinkType.ToString(), *GetDefinition(IndexByType[Definition->LinkType])->GetPathName()));
			continue;
		}
		if (Entries.Num() >= GSTNLinkLimits::MaxLinkTypes)
		{
			BuildErrors.Add(FString::Printf(TEXT("Skipped %s: the table is full (%d link types fit the wire byte)."), *Definition->GetPathName(),
				GSTNLinkLimits::MaxLinkTypes));
			continue;
		}

		FGSTN_LinkTypeEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Index = Entries.Num();
		Entry.LinkType = Definition->LinkType;
		Entry.Channel = Definition->Channel;
		Entry.InitiatorRoleTag = Definition->InitiatorRole.RoleTag;
		Entry.TargetRoleTag = Definition->TargetRole.RoleTag;
		Entry.ParkedRoleIndex = Definition->GetParkedRoleIndex();
		Entry.Definition = Definition;
		IndexByType.Add(Entry.LinkType, static_cast<uint8>(Entry.Index));
	}

	for (const FName& Name : GSTNLinkRegistryPrivate::PluginReasonNames())
	{
		const FGameplayTag Reason = FGameplayTag::RequestGameplayTag(Name, false);
		if (Reason.IsValid() && Reasons.Append(Reason) == 0)
		{
			BuildErrors.Add(FString::Printf(TEXT("The break reason '%s' does not fit the reason table."), *Name.ToString()));
		}
	}

	for (const FString& Error : BuildErrors)
	{
		UE_LOG(LogGSTNLink, Warning, TEXT("Link registry: %s"), *Error);
	}
	if (OutErrors != nullptr)
	{
		OutErrors->Append(BuildErrors);
	}

	UpdateSignature();
	bBuilt = true;
}

uint8 UGSTN_LinkRegistry::GetTypeIndex(const FGameplayTag& LinkType) const
{
	const uint8* Found = IndexByType.Find(LinkType);
	return Found != nullptr ? *Found : 0;
}

const FGSTN_LinkTypeEntry* UGSTN_LinkRegistry::GetEntry(uint8 TypeIndex) const
{
	return TypeIndex > 0 && TypeIndex <= Entries.Num() ? &Entries[TypeIndex - 1] : nullptr;
}

const UGSTN_LinkTypeDefinition* UGSTN_LinkRegistry::GetDefinition(uint8 TypeIndex) const
{
	const FGSTN_LinkTypeEntry* Entry = GetEntry(TypeIndex);
	return Entry != nullptr ? Entry->Definition.Get() : nullptr;
}

FGameplayTag UGSTN_LinkRegistry::GetLinkType(uint8 TypeIndex) const
{
	const FGSTN_LinkTypeEntry* Entry = GetEntry(TypeIndex);
	return Entry != nullptr ? Entry->LinkType : FGameplayTag();
}

FGameplayTag UGSTN_LinkRegistry::GetRoleTag(uint8 TypeIndex, int32 RoleIndex) const
{
	const FGSTN_LinkTypeEntry* Entry = GetEntry(TypeIndex);
	return Entry != nullptr ? Entry->GetRoleTag(RoleIndex) : FGameplayTag();
}

int32 UGSTN_LinkRegistry::FindRoleIndex(uint8 TypeIndex, const FGameplayTag& RoleTag) const
{
	const FGSTN_LinkTypeEntry* Entry = GetEntry(TypeIndex);
	if (Entry == nullptr || !RoleTag.IsValid())
	{
		return 0;
	}
	return RoleTag == Entry->InitiatorRoleTag ? 1 : (RoleTag == Entry->TargetRoleTag ? 2 : 0);
}

uint8 UGSTN_LinkRegistry::AppendReason(const FGameplayTag& Reason)
{
	const uint8 Index = Reasons.Append(Reason);
	UpdateSignature();
	return Index;
}

void UGSTN_LinkRegistry::RegisterPluginReason(FName ReasonTagName)
{
	if (!ReasonTagName.IsNone())
	{
		GSTNLinkRegistryPrivate::PluginReasonNames().AddUnique(ReasonTagName);
	}
}

void UGSTN_LinkRegistry::UnregisterPluginReason(FName ReasonTagName)
{
	GSTNLinkRegistryPrivate::PluginReasonNames().Remove(ReasonTagName);
}

void UGSTN_LinkRegistry::UpdateSignature()
{
	FString Text;
	for (const FGSTN_LinkTypeEntry& Entry : Entries)
	{
		Text += FString::Printf(TEXT("%d=%s/%d/%s/%s/%d;"), Entry.Index, *Entry.LinkType.ToString(), static_cast<int32>(Entry.Channel),
			*Entry.InitiatorRoleTag.ToString(), *Entry.TargetRoleTag.ToString(), Entry.ParkedRoleIndex);
	}
	for (int32 Index = 0; Index < Reasons.NumPluginReasons(); ++Index)
	{
		const uint8 ReasonIndex = static_cast<uint8>(GSTNLinkRegistryPrivate::FirstPluginReason + Index);
		Text += FString::Printf(TEXT("R%d=%s;"), ReasonIndex, *Reasons.TagOf(ReasonIndex).ToString());
	}
	Signature = FCrc::StrCrc32(*Text);
}
