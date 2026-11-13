#include "Bindings/GSTN_KeyBindingsSubsystem.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "GSTN_InputLog.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerMappableKeySettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace GSTN_KeyBindingsPrivate
{
	/**
	 * Contexts the player has now. UEnhancedPlayerInput keeps them in a protected reflected map with no public getter,
	 * so it is read through reflection (keys only, read only).
	 */
	static TArray<const UInputMappingContext*> GetAppliedContexts(const UEnhancedPlayerInput* PlayerInput)
	{
		TArray<const UInputMappingContext*> Contexts;
		static FMapProperty* MapProperty =
			CastField<FMapProperty>(UEnhancedPlayerInput::StaticClass()->FindPropertyByName(TEXT("AppliedInputContextData")));
		const FObjectPropertyBase* KeyProperty = MapProperty ? CastField<FObjectPropertyBase>(MapProperty->KeyProp) : nullptr;
		if (!PlayerInput || !KeyProperty)
		{
			return Contexts;
		}
		FScriptMapHelper Helper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(const_cast<UEnhancedPlayerInput*>(PlayerInput)));
		for (FScriptMapHelper::FIterator It(Helper); It; ++It)
		{
			if (const UInputMappingContext* Context = Cast<UInputMappingContext>(KeyProperty->GetObjectPropertyValue(Helper.GetKeyPtr(It))))
			{
				Contexts.Add(Context);
			}
		}
		return Contexts;
	}
}

void UGSTN_KeyBindingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(UEnhancedInputLocalPlayerSubsystem::StaticClass());

	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			// Fires after every key mapping rebuild, so contexts added at possession (ability input, camera) are picked up
			EnhancedInput->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &ThisClass::HandleControlMappingsRebuilt);
		}
	}
	Load();
}

void UGSTN_KeyBindingsSubsystem::Deinitialize()
{
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			EnhancedInput->ControlMappingsRebuiltDelegate.RemoveAll(this);
		}
	}
	RegisteredContexts.Reset();
	Super::Deinitialize();
}

TArray<FGSTN_BindableMapping> UGSTN_KeyBindingsSubsystem::ListBindableMappings() const
{
	TArray<FGSTN_BindableMapping> Result;
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	const UEnhancedPlayerInput* PlayerInput = EnhancedInput ? EnhancedInput->GetPlayerInput() : nullptr;
	if (!PlayerInput)
	{
		return Result;
	}

	for (const UInputMappingContext* Context : GSTN_KeyBindingsPrivate::GetAppliedContexts(PlayerInput))
	{
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			// The menu names a mapping by its action and default key, whatever row the user settings keep it in
			const FName Name = FGSTN_KeyBindingsApply::GetMappingName(Mapping).IsNone() || !Mapping.Action ? NAME_None : Mapping.Action->GetFName();
			const bool bKnown = Result.ContainsByPredicate([&](const FGSTN_BindableMapping& Row)
			{
				return Row.Action == Name && Row.DefaultKey == Mapping.Key;
			});
			if (Name.IsNone() || bKnown)
			{
				continue;
			}
			FGSTN_BindableMapping& Row = Result.AddDefaulted_GetRef();
			Row.Action = Name;
			Row.DisplayName = Mapping.GetDisplayName();
			Row.DefaultKey = Mapping.Key;
			Row.CurrentKey = Bindings.Resolve(Name, Mapping.Key);
			Row.Context = Context->GetFName();
		}
	}
	return Result;
}

bool UGSTN_KeyBindingsSubsystem::Rebind(FName Action, FKey DefaultKey, FKey NewKey)
{
	if (!Bindings.Rebind(Action, DefaultKey, NewKey))
	{
		return false;
	}
	ApplyToEnhancedInput();
	OnBindingsChanged.Broadcast();
	return true;
}

bool UGSTN_KeyBindingsSubsystem::ResetBinding(FName Action, FKey DefaultKey)
{
	if (!Bindings.Reset(Action, DefaultKey))
	{
		return false;
	}
	ApplyToEnhancedInput();
	OnBindingsChanged.Broadcast();
	return true;
}

void UGSTN_KeyBindingsSubsystem::ResetAllToDefault()
{
	SetBindings(FGSTN_KeyBindingSet());
}

void UGSTN_KeyBindingsSubsystem::SetBindings(const FGSTN_KeyBindingSet& NewBindings)
{
	Bindings = NewBindings;
	ApplyToEnhancedInput();
	OnBindingsChanged.Broadcast();
}

FString UGSTN_KeyBindingsSubsystem::GetSaveSlotName() const
{
	return FString::Printf(TEXT("GSTNInputBindings_P%d"), FMath::Max(0, GetPlayerSlotIndex()));
}

int32 UGSTN_KeyBindingsSubsystem::GetPlayerSlotIndex() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetLocalPlayerIndex() : 0;
}

bool UGSTN_KeyBindingsSubsystem::Save()
{
	UGSTN_KeyBindingsSaveGame* SaveGame = NewObject<UGSTN_KeyBindingsSaveGame>(this);
	SaveGame->Bindings = Bindings;
	return WriteStorage(SaveGame->ToBytes());
}

bool UGSTN_KeyBindingsSubsystem::Load()
{
	TArray<uint8> Bytes;
	FGSTN_KeyBindingSet Loaded;
	bool bRead = false;
	if (ReadStorage(Bytes) && Bytes.Num() > 0)
	{
		if (const UGSTN_KeyBindingsSaveGame* SaveGame = UGSTN_KeyBindingsSaveGame::FromBytes(Bytes, this))
		{
			Loaded = SaveGame->Bindings;
			bRead = true;
		}
		else
		{
			UE_LOG(LogGSTNInput, Warning, TEXT("Key bindings slot %s is unreadable: defaults used"), *GetSaveSlotName());
		}
	}
	SetBindings(Loaded);
	return bRead;
}

bool UGSTN_KeyBindingsSubsystem::ReadStorage(TArray<uint8>& OutBytes) const
{
	// The user index is ignored by the PC save system: the slot name carries the player
	const FString Slot = GetSaveSlotName();
	return UGameplayStatics::DoesSaveGameExist(Slot, 0) && UGameplayStatics::LoadDataFromSlot(OutBytes, Slot, 0);
}

bool UGSTN_KeyBindingsSubsystem::WriteStorage(const TArray<uint8>& Bytes) const
{
	return UGameplayStatics::SaveDataToSlot(Bytes, GetSaveSlotName(), 0);
}

void UGSTN_KeyBindingsSubsystem::ApplyToEnhancedInput()
{
	if (bApplying)
	{
		return;
	}
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	UEnhancedInputUserSettings* Settings = EnhancedInput ? EnhancedInput->GetUserSettings() : nullptr;
	const UEnhancedPlayerInput* PlayerInput = EnhancedInput ? EnhancedInput->GetPlayerInput() : nullptr;
	if (!Settings || !PlayerInput)
	{
		// No controller yet: the first rebuild after possession applies the overrides
		return;
	}

	TGuardValue<bool> Guard(bApplying, true);
	for (const UInputMappingContext* Context : GSTN_KeyBindingsPrivate::GetAppliedContexts(PlayerInput))
	{
		if (RegisteredContexts.Contains(Context))
		{
			continue;
		}
		FGSTN_KeyBindingsApply::EnsureMappable(Context);
		Settings->RegisterInputMappingContext(Context);
		RegisteredContexts.Add(Context);
	}
	FGSTN_KeyBindingsApply::ApplyToUserSettings(*Settings, Bindings);
	EnhancedInput->RequestRebuildControlMappings();
}

void UGSTN_KeyBindingsSubsystem::HandleControlMappingsRebuilt()
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	const UEnhancedPlayerInput* PlayerInput = EnhancedInput ? EnhancedInput->GetPlayerInput() : nullptr;
	if (bApplying || !PlayerInput)
	{
		return;
	}
	// Only a context this player has not registered yet needs work; otherwise this rebuild is our own and ends the chain
	for (const UInputMappingContext* Context : GSTN_KeyBindingsPrivate::GetAppliedContexts(PlayerInput))
	{
		if (!RegisteredContexts.Contains(Context))
		{
			ApplyToEnhancedInput();
			return;
		}
	}
}
