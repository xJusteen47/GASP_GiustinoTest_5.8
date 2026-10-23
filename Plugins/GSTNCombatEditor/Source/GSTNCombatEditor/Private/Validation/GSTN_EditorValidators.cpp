// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Validation/GSTN_EditorValidators.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Input/GSTN_InputSettings.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Loadout/GSTN_LoadoutRules.h"
#include "Misc/DataValidation.h"
#include "Settings/GSTN_CombatEditorSettings.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Validation/GSTN_CombatValidators.h"
#include "Validation/GSTN_ValidationMessageLog.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

EDataValidationResult UGSTN_CombatValidatorBase::AddReportToContext(const FGSTN_ValidationReport& Report, const FAssetData& AssetData, FDataValidationContext& Context)
{
	// What the asset's own IsDataValid said before the validators ran. Its issues are plain texts, ours are "[Code] text", so the comparison is on the bare message
	TSet<FString> Existing;
	for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
	{
		Existing.Add(Issue.TokenizedMessage.IsValid() ? Issue.TokenizedMessage->ToText().ToString() : Issue.Message.ToString());
	}

	bool bInvalid = false;
	for (const FGSTN_ValidationFinding& Finding : Report.Findings)
	{
		bInvalid |= Finding.Severity == EGSTN_Severity::Error;
		if (Existing.Contains(Finding.Message.ToString()))
		{
			continue;
		}
		Existing.Add(Finding.Message.ToString());

		const TSharedRef<FTokenizedMessage> Message = Context.AddMessage(AssetData, GSTNValidationLog::ToMessageSeverity(Finding.Severity), GSTNValidationLog::FormatFinding(Finding));
		if (!Finding.Detail.IsEmpty())
		{
			Message->AddToken(GSTNValidationLog::MakeShowAction(Finding.Detail));
		}
	}
	return bInvalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}

namespace
{
	const UGSTN_InputSettings& GetInputSettings()
	{
		return UGSTN_InputSettings::Get();
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------

bool UGSTN_TimelineValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	return Cast<UGSTN_CombatTimeline>(InAsset) != nullptr;
}

EDataValidationResult UGSTN_TimelineValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	const UGSTN_CombatTimeline* Timeline = Cast<UGSTN_CombatTimeline>(InAsset);
	if (!Timeline)
	{
		return EDataValidationResult::NotValidated;
	}

	FGSTN_ValidationReport Report;
	const FGSTN_TimelineRuleContext Context = FGSTN_CombatValidators::MakeTimelineContext(Timeline->Data, InAsset, UGSTN_CombatEditorSettings::GetSimulationTickRate(), true);
	FGSTN_CombatValidators::ValidateTimeline(Timeline->Data, Context, Report);
	return AddReportToContext(Report, InAssetData, InContext);
}

// ---------------------------------------------------------------------------------------------------------------------------------

bool UGSTN_KitValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	return Cast<UGSTN_PrimaryKitDefinition>(InAsset) != nullptr;
}

EDataValidationResult UGSTN_KitValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	const UGSTN_PrimaryKitDefinition* Kit = Cast<UGSTN_PrimaryKitDefinition>(InAsset);
	if (!Kit)
	{
		return EDataValidationResult::NotValidated;
	}

	FGSTN_ValidationReport Report;
	FGSTN_CombatValidators::CheckKit(*Kit, GetInputSettings(), Report);
	return AddReportToContext(Report, InAssetData, InContext);
}

// ---------------------------------------------------------------------------------------------------------------------------------

bool UGSTN_WeaponValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	return Cast<UGSTN_WeaponDefinition>(InAsset) != nullptr;
}

EDataValidationResult UGSTN_WeaponValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	const UGSTN_WeaponDefinition* Weapon = Cast<UGSTN_WeaponDefinition>(InAsset);
	if (!Weapon)
	{
		return EDataValidationResult::NotValidated;
	}

	FGSTN_ValidationReport Report;
	FGSTN_CombatValidators::CheckWeaponData(*Weapon, Report);

	// Every kit that can carry the weapon: its slots are on top of the kit's, and that is where they can collide
	TArray<FAssetData> Kits;
	IAssetRegistry::GetChecked().GetAssetsByClass(UGSTN_PrimaryKitDefinition::StaticClass()->GetClassPathName(), Kits, true);
	for (const FAssetData& KitData : Kits)
	{
		const UGSTN_PrimaryKitDefinition* Kit = Cast<UGSTN_PrimaryKitDefinition>(KitData.GetAsset());
		if (Kit && FGSTN_CombatValidators::KitCanCarryWeapon(*Kit, *Weapon))
		{
			FGSTN_CombatValidators::CheckWeaponOnKit(*Weapon, *Kit, GetInputSettings(), Report);
		}
	}
	return AddReportToContext(Report, InAssetData, InContext);
}

// ---------------------------------------------------------------------------------------------------------------------------------

void UGSTN_LoadoutPresetValidator::GatherLoadouts(const UObject& Object, TArray<TPair<FName, const FGSTN_LoadoutData*>>& OutLoadouts)
{
	for (TFieldIterator<FStructProperty> It(Object.GetClass()); It; ++It)
	{
		if (It->Struct == FGSTN_LoadoutData::StaticStruct())
		{
			OutLoadouts.Emplace(It->GetFName(), It->ContainerPtrToValuePtr<FGSTN_LoadoutData>(&Object));
		}
	}
}

bool UGSTN_LoadoutPresetValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	if (!InAsset)
	{
		return false;
	}
	TArray<TPair<FName, const FGSTN_LoadoutData*>> Loadouts;
	GatherLoadouts(*InAsset, Loadouts);
	return !Loadouts.IsEmpty();
}

EDataValidationResult UGSTN_LoadoutPresetValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	if (!InAsset)
	{
		return EDataValidationResult::NotValidated;
	}
	TArray<TPair<FName, const FGSTN_LoadoutData*>> Loadouts;
	GatherLoadouts(*InAsset, Loadouts);

	FGSTN_ValidationReport Report;
	for (const TPair<FName, const FGSTN_LoadoutData*>& Loadout : Loadouts)
	{
		FGSTN_ResolvedLoadout Resolved;
		TArray<TStrongObjectPtr<UObject>> Hold;
		FGSTN_CombatValidators::ResolveLoadoutData(*Loadout.Value, Resolved, Hold);
		FGSTN_CombatValidators::CheckLoadout(Resolved, GetInputSettings(), *GetDefault<UGSTN_LoadoutRules>(), InAsset, Report);
	}
	return AddReportToContext(Report, InAssetData, InContext);
}

#undef LOCTEXT_NAMESPACE
