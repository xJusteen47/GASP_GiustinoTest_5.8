// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Settings/GSTN_CombatEditorSettings.h"

#include "UObject/UnrealType.h"

UGSTN_CombatEditorSettings::UGSTN_CombatEditorSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("GSTN Combat Editor");
}

FLinearColor UGSTN_CombatEditorSettings::GetTrackColor(FName TrackStruct) const
{
	for (const FGSTN_TrackColor& Entry : TrackColors)
	{
		if (Entry.TrackStruct == TrackStruct)
		{
			return Entry.Color;
		}
	}
	return GenericTrackColor;
}

FGameplayTag UGSTN_CombatEditorSettings::SuggestHurtboxZone(FName BoneName) const
{
	const FString Bone = BoneName.ToString();
	for (const FGSTN_HurtboxZoneRule& Rule : HurtboxZoneRules)
	{
		// An empty fragment would match every bone: a half-edited line must suggest nothing, not everything
		if (!Rule.BoneNameContains.IsEmpty() && Bone.Contains(Rule.BoneNameContains, ESearchCase::IgnoreCase))
		{
			return Rule.Zone;
		}
	}
	return FGameplayTag();
}

float UGSTN_CombatEditorSettings::ReadFixedTickRate(const UObject* SettingsObject, float Fallback)
{
	if (SettingsObject == nullptr)
	{
		return Fallback;
	}

	// By reflection on purpose: UNetworkPredictionSettingsObject is not exported from the NetworkPrediction module (no API
	// macro on the class), so no other module can name it, and reflection also keeps this module from linking the fork
	const FStructProperty* SettingsProperty = CastField<FStructProperty>(SettingsObject->GetClass()->FindPropertyByName(TEXT("Settings")));
	if (SettingsProperty == nullptr)
	{
		return Fallback;
	}
	const FNumericProperty* RateProperty = CastField<FNumericProperty>(SettingsProperty->Struct->FindPropertyByName(TEXT("FixedTickFrameRate")));
	if (RateProperty == nullptr || !RateProperty->IsFloatingPoint())
	{
		return Fallback;
	}

	const void* SettingsValue = SettingsProperty->ContainerPtrToValuePtr<void>(SettingsObject);
	const double Rate = RateProperty->GetFloatingPointPropertyValue(RateProperty->ContainerPtrToValuePtr<void>(SettingsValue));
	return FMath::IsFinite(Rate) && Rate > 0.0 ? static_cast<float>(Rate) : Fallback;
}

float UGSTN_CombatEditorSettings::GetSimulationTickRate()
{
	const float Fallback = Get().FallbackTickRateHz > 0.f ? Get().FallbackTickRateHz : DefaultTickRateHz;

	// The class default object holds the value loaded from the project's DefaultNetworkPrediction.ini, which is what every
	// world manager copies when it starts (UNetworkPredictionWorldManager::SyncNetworkPredictionSettings), so the editor
	// and a running PIE agree without a world
	const UClass* SettingsClass = FindObject<UClass>(nullptr, TEXT("/Script/NetworkPrediction.NetworkPredictionSettingsObject"));
	return ReadFixedTickRate(SettingsClass ? SettingsClass->GetDefaultObject() : nullptr, Fallback);
}
