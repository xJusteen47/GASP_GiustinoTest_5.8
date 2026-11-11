#include "Bindings/GSTN_KeyBindings.h"

#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerMappableKeySettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"

bool FGSTN_KeyBindingSet::Rebind(FName Action, const FKey& DefaultKey, const FKey& NewKey)
{
	if (Action.IsNone() || !DefaultKey.IsValid() || !NewKey.IsValid())
	{
		return false;
	}
	if (NewKey == DefaultKey)
	{
		return Reset(Action, DefaultKey);
	}
	for (FGSTN_KeyBinding& Binding : Overrides)
	{
		if (Binding.Action == Action && Binding.DefaultKey == DefaultKey)
		{
			if (Binding.Key == NewKey)
			{
				return false;
			}
			Binding.Key = NewKey;
			return true;
		}
	}
	Overrides.Add({Action, DefaultKey, NewKey});
	return true;
}

bool FGSTN_KeyBindingSet::Reset(FName Action, const FKey& DefaultKey)
{
	return Overrides.RemoveAll([&](const FGSTN_KeyBinding& Binding)
	{
		return Binding.Action == Action && Binding.DefaultKey == DefaultKey;
	}) > 0;
}

const FGSTN_KeyBinding* FGSTN_KeyBindingSet::Find(FName Action, const FKey& DefaultKey) const
{
	return Overrides.FindByPredicate([&](const FGSTN_KeyBinding& Binding)
	{
		return Binding.Action == Action && Binding.DefaultKey == DefaultKey;
	});
}

FKey FGSTN_KeyBindingSet::Resolve(FName Action, const FKey& DefaultKey) const
{
	const FGSTN_KeyBinding* Binding = Find(Action, DefaultKey);
	return Binding ? Binding->Key : DefaultKey;
}

TArray<uint8> UGSTN_KeyBindingsSaveGame::ToBytes()
{
	TArray<uint8> Bytes;
	UGameplayStatics::SaveGameToMemory(this, Bytes);
	return Bytes;
}

UGSTN_KeyBindingsSaveGame* UGSTN_KeyBindingsSaveGame::FromBytes(const TArray<uint8>& Bytes, UObject* Outer)
{
	// LoadGameFromMemory makes its object in the transient package; the caller only copies the bindings out
	(void)Outer;
	return Cast<UGSTN_KeyBindingsSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
}

void FGSTN_KeyBindingsApply::EnsureMappable(const UInputMappingContext* Context)
{
	if (!Context)
	{
		return;
	}

	// PlayerMappableKeySettings is protected on UInputAction and has no setter: reflection, as the editor does
	static FObjectPropertyBase* SettingsProperty =
		CastField<FObjectPropertyBase>(UInputAction::StaticClass()->FindPropertyByName(TEXT("PlayerMappableKeySettings")));
	if (!SettingsProperty)
	{
		return;
	}

	for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
	{
		const UInputAction* Action = Mapping.Action;
		if (!Action || Action->GetPlayerMappableKeySettings())
		{
			continue;
		}
		UInputAction* MutableAction = const_cast<UInputAction*>(Action);
		UPlayerMappableKeySettings* Settings = NewObject<UPlayerMappableKeySettings>(MutableAction, NAME_None, RF_Transient);
		Settings->Name = Action->GetFName();
		Settings->DisplayName = Action->ActionDescription.IsEmpty() ? FText::FromName(Action->GetFName()) : Action->ActionDescription;
		SettingsProperty->SetObjectPropertyValue_InContainer(MutableAction, Settings);
	}
}

FName FGSTN_KeyBindingsApply::GetMappingName(const FEnhancedActionKeyMapping& Mapping)
{
	return Mapping.IsPlayerMappable() ? Mapping.GetMappingName() : NAME_None;
}

int32 FGSTN_KeyBindingsApply::ApplyToUserSettings(UEnhancedInputUserSettings& Settings, const FGSTN_KeyBindingSet& Bindings)
{
	const FString ProfileId = Settings.GetActiveKeyProfileId();
	FGameplayTagContainer Failure;

	// From the defaults every time: an override removed since the last apply must come back to its default key
	Settings.ResetKeyProfileIdToDefault(ProfileId, Failure);

	int32 Applied = 0;
	for (const FGSTN_KeyBinding& Binding : Bindings.Overrides)
	{
		for (const FPlayerKeyMapping& Mapping : Settings.FindMappingsInRow(Binding.Action))
		{
			if (Mapping.GetDefaultKey() != Binding.DefaultKey)
			{
				continue;
			}
			FMapPlayerKeyArgs Args;
			Args.MappingName = Binding.Action;
			Args.Slot = Mapping.GetSlot();
			Args.NewKey = Binding.Key;
			Args.HardwareDeviceId = Mapping.GetHardwareDeviceId().HardwareDeviceIdentifier;
			Args.ProfileIdString = ProfileId;
			Failure.Reset();
			Settings.MapPlayerKey(Args, Failure);
			Applied += Failure.IsEmpty() ? 1 : 0;
			break;
		}
	}

	Settings.ApplySettings();
	return Applied;
}

FKey FGSTN_KeyBindingsApply::GetCurrentKey(const UEnhancedInputUserSettings& Settings, FName Action, const FKey& DefaultKey)
{
	for (const FPlayerKeyMapping& Mapping : Settings.FindMappingsInRow(Action))
	{
		if (Mapping.GetDefaultKey() == DefaultKey)
		{
			return Mapping.GetCurrentKey();
		}
	}
	return EKeys::Invalid;
}
