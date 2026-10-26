// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Validation/GSTN_ValidateAll.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Misc/ScopedSlowTask.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "UObject/UObjectIterator.h"
#include "Validation/GSTN_EditorValidators.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

int32 GSTNValidateAll::FResult::GetCheckedCount() const
{
	int32 Count = 0;
	for (const FAssetResult& Asset : Assets)
	{
		Count += Asset.bValidated ? 1 : 0;
	}
	return Count;
}

int32 GSTNValidateAll::FResult::CountSeverity(EGSTN_Severity Severity) const
{
	int32 Count = 0;
	for (const FAssetResult& Asset : Assets)
	{
		Count += Asset.Report.CountSeverity(Severity);
	}
	return Count;
}

FGSTN_ValidationReport GSTNValidateAll::FResult::Flatten() const
{
	FGSTN_ValidationReport Report;
	for (const FAssetResult& Asset : Assets)
	{
		Report.Append(Asset.Report);
	}
	return Report;
}

namespace
{
	bool IsInScope(const FAssetData& Data, const GSTNValidateAll::FOptions& Options)
	{
		const FString Path = Data.PackagePath.ToString();
		if (!Options.PathPrefix.IsEmpty())
		{
			return Path.StartsWith(Options.PathPrefix);
		}
		return !Path.StartsWith(TEXT("/Engine")) && !Path.StartsWith(TEXT("/Temp"));
	}

	void AddByClass(IAssetRegistry& Registry, const UClass* Class, TSet<FSoftObjectPath>& Seen, const GSTNValidateAll::FOptions& Options, TArray<FAssetData>& Out)
	{
		TArray<FAssetData> Found;
		Registry.GetAssetsByClass(Class->GetClassPathName(), Found, true);
		for (const FAssetData& Data : Found)
		{
			if (IsInScope(Data, Options) && !Seen.Contains(Data.GetSoftObjectPath()))
			{
				Seen.Add(Data.GetSoftObjectPath());
				Out.Add(Data);
			}
		}
	}
}

void GSTNValidateAll::CollectAssets(const FOptions& Options, TArray<FAssetData>& OutAssets)
{
	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	TSet<FSoftObjectPath> Seen;

	AddByClass(Registry, UGSTN_CombatTimeline::StaticClass(), Seen, Options, OutAssets);
	AddByClass(Registry, UGSTN_HurtboxSetDefinition::StaticClass(), Seen, Options, OutAssets);
	AddByClass(Registry, UGSTN_PrimaryKitDefinition::StaticClass(), Seen, Options, OutAssets);
	AddByClass(Registry, UGSTN_WeaponDefinition::StaticClass(), Seen, Options, OutAssets);

	// Anything that holds a loadout: the preset class of a game, or its own data asset. Found by the property, as the preset validator does
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (!It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated) && It->IsChildOf(UObject::StaticClass()) && !It->IsChildOf(UBlueprint::StaticClass()))
		{
			bool bHoldsLoadout = false;
			for (TFieldIterator<FStructProperty> Property(*It); Property && !bHoldsLoadout; ++Property)
			{
				bHoldsLoadout = Property->Struct == FGSTN_LoadoutData::StaticStruct();
			}
			// Only classes that are assets: an actor or a component that happens to hold a loadout is not content to validate here
			if (bHoldsLoadout && (It->IsChildOf(UDataAsset::StaticClass()) || It->IsChildOf(UPrimaryDataAsset::StaticClass())))
			{
				AddByClass(Registry, *It, Seen, Options, OutAssets);
			}
		}
	}

	// Ability Blueprints: the registry lists the classes that derive from the ability, Blueprint ones included; an asset matches by the class it generates
	TSet<FTopLevelAssetPath> AbilityClasses;
	Registry.GetDerivedClassNames({ UGSTN_GA_BaseCombat::StaticClass()->GetClassPathName() }, TSet<FTopLevelAssetPath>(), AbilityClasses);
	TArray<FAssetData> Blueprints;
	Registry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Blueprints, true);
	for (const FAssetData& Data : Blueprints)
	{
		if (!IsInScope(Data, Options) || Seen.Contains(Data.GetSoftObjectPath()))
		{
			continue;
		}
		const FString GeneratedClass = Data.GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath);
		if (!GeneratedClass.IsEmpty() && AbilityClasses.Contains(FTopLevelAssetPath(FPackageName::ExportTextPathToObjectPath(GeneratedClass))))
		{
			Seen.Add(Data.GetSoftObjectPath());
			OutAssets.Add(Data);
		}
	}
}

GSTNValidateAll::FAssetResult GSTNValidateAll::RunOne(const FAssetData& AssetData)
{
	FAssetResult Result;
	Result.AssetData = AssetData;
	UObject* Asset = AssetData.GetAsset();
	if (Asset != nullptr)
	{
		Result.bValidated = UGSTN_CombatValidatorBase::BuildReportForAsset(Asset, Result.Report);
	}
	return Result;
}

GSTNValidateAll::FResult GSTNValidateAll::Run(const FOptions& Options)
{
	TArray<FAssetData> Assets;
	CollectAssets(Options, Assets);

	FResult Result;
	Result.Assets.Reserve(Assets.Num());

	FScopedSlowTask Progress(static_cast<float>(Assets.Num()), LOCTEXT("ValidateAllProgress", "Validating combat assets"), Options.bShowProgress);
	if (Options.bShowProgress)
	{
		Progress.MakeDialog(true);
	}
	for (const FAssetData& Data : Assets)
	{
		Progress.EnterProgressFrame(1.f, FText::FromName(Data.AssetName));
		if (Options.bShowProgress && Progress.ShouldCancel())
		{
			break;
		}
		Result.Assets.Add(RunOne(Data));
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
