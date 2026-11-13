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

	// One user-settings row holds the alternative keys of ONE binding: when the engine rebuilds the mappings it gives every
	// mapping of a row all the row's keys of the same kind (keyboard/mouse, pad, touch). Four keyboard mappings of one action
	// with different modifiers (W/A/S/D on IA_Move) therefore all fired on every one of the four keys, and the first mapping
	// to claim a key won it: every key moved the same way. Such mappings get a row of their own, "<Action>_<DefaultKey>",
	// through the mapping's own settings (OverrideSettings); an action with one mapping per kind keeps the action-named row.
	static FProperty* BehaviorProperty = FEnhancedActionKeyMapping::StaticStruct()->FindPropertyByName(TEXT("SettingBehavior"));
	static FObjectPropertyBase* MappingSettingsProperty =
		CastField<FObjectPropertyBase>(FEnhancedActionKeyMapping::StaticStruct()->FindPropertyByName(TEXT("PlayerMappableKeySettings")));
	if (!BehaviorProperty || !MappingSettingsProperty)
	{
		return;
	}
	UInputMappingContext* MutableContext = const_cast<UInputMappingContext*>(Context);
	const TArray<FEnhancedActionKeyMapping>& Mappings = Context->GetMappings();
	for (int32 Index = 0; Index < Mappings.Num(); ++Index)
	{
		const FEnhancedActionKeyMapping& Mapping = Mappings[Index];
		if (!Mapping.Action || !Mapping.IsPlayerMappable() || Mapping.GetMappingName() != Mapping.Action->GetFName())
		{
			continue; // not rebindable, an asset-made identity, or already given its own row
		}
		const bool bSharesRow = Mappings.ContainsByPredicate([&](const FEnhancedActionKeyMapping& Other)
		{
			return &Other != &Mapping && Other.Action == Mapping.Action && IsSameKeyKind(Other.Key, Mapping.Key);
		});
		if (!bSharesRow)
		{
			continue;
		}
		FEnhancedActionKeyMapping& Mutable = MutableContext->GetMapping(Index);
		UPlayerMappableKeySettings* Own = NewObject<UPlayerMappableKeySettings>(MutableContext, NAME_None, RF_Transient);
		Own->Name = MakeRowName(Mapping.Action->GetFName(), Mapping.Key);
		Own->DisplayName = FText::Format(INVTEXT("{0} ({1})"), Mapping.Action->GetPlayerMappableKeySettings()->DisplayName, Mapping.Key.GetDisplayName());
		MappingSettingsProperty->SetObjectPropertyValue_InContainer(&Mutable, Own);
		const EPlayerMappableKeySettingBehaviors Override = EPlayerMappableKeySettingBehaviors::OverrideSettings;
		BehaviorProperty->CopySingleValue(BehaviorProperty->ContainerPtrToValuePtr<void>(&Mutable), &Override);
	}
}

bool FGSTN_KeyBindingsApply::IsSameKeyKind(const FKey& A, const FKey& B)
{
	// The kinds the engine's row query tells apart (bMatchBasicKeyTypes)
	return A.IsGamepadKey() == B.IsGamepadKey() && A.IsTouch() == B.IsTouch();
}

FName FGSTN_KeyBindingsApply::MakeRowName(FName Action, const FKey& DefaultKey)
{
	return FName(*FString::Printf(TEXT("%s_%s"), *Action.ToString(), *DefaultKey.GetFName().ToString()));
}

const FPlayerKeyMapping* FGSTN_KeyBindingsApply::FindRowMapping(const UEnhancedInputUserSettings& Settings, FName Action, const FKey& DefaultKey)
{
	// A mapping with a row of its own first, then the action's shared row
	for (const FName Row : { MakeRowName(Action, DefaultKey), Action })
	{
		for (const FPlayerKeyMapping& Mapping : Settings.FindMappingsInRow(Row))
		{
			if (Mapping.GetDefaultKey() == DefaultKey)
			{
				return &Mapping;
			}
		}
	}
	return nullptr;
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
		const FPlayerKeyMapping* Mapping = FindRowMapping(Settings, Binding.Action, Binding.DefaultKey);
		if (!Mapping)
		{
			continue;
		}
		FMapPlayerKeyArgs Args;
		Args.MappingName = Mapping->GetMappingName();
		Args.Slot = Mapping->GetSlot();
		Args.NewKey = Binding.Key;
		Args.HardwareDeviceId = Mapping->GetHardwareDeviceId().HardwareDeviceIdentifier;
		Args.ProfileIdString = ProfileId;
		Failure.Reset();
		Settings.MapPlayerKey(Args, Failure);
		Applied += Failure.IsEmpty() ? 1 : 0;
	}

	Settings.ApplySettings();
	return Applied;
}

FKey FGSTN_KeyBindingsApply::GetCurrentKey(const UEnhancedInputUserSettings& Settings, FName Action, const FKey& DefaultKey)
{
	const FPlayerKeyMapping* Mapping = FindRowMapping(Settings, Action, DefaultKey);
	return Mapping ? Mapping->GetCurrentKey() : EKeys::Invalid;
}
