#include "GSTN_CameraLocalPlayerSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GSTN_CameraProfile.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"

bool UGSTN_CameraLocalPlayerSubsystem::ShouldCreateFor(ENetMode NetMode, bool bGameWorld, bool bEnabled)
{
	return bEnabled && bGameWorld && NetMode != NM_DedicatedServer;
}

UGSTN_CameraLocalPlayerSubsystem* UGSTN_CameraLocalPlayerSubsystem::Get(const APlayerController* Controller)
{
	const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UGSTN_CameraLocalPlayerSubsystem>() : nullptr;
}

bool UGSTN_CameraLocalPlayerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Outer);
	const UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
	// The world may not exist yet when the local player is created: only an existing dedicated server world vetoes
	const bool bDedicated = World && World->GetNetMode() == NM_DedicatedServer;
	return Super::ShouldCreateSubsystem(Outer) && !bDedicated;
}

void UGSTN_CameraLocalPlayerSubsystem::EnsureInputContext(APlayerController* Controller, const UGSTN_CameraProfile& Settings)
{
	if (!Controller || AddedContext.IsValid())
	{
		return;
	}
	if (Settings.LookMappingContext.IsNull())
	{
		return;
	}
	const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
	const UInputMappingContext* Context = Settings.LookMappingContext.LoadSynchronous();
	if (Input && Context)
	{
		Input->AddMappingContext(Context, Settings.MappingContextPriority);
		AddedContext = Context;
	}
}

void UGSTN_CameraLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadUserOptions();
}

FString UGSTN_CameraLocalPlayerSubsystem::GetSlotName(int32& OutUserIndex) const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	OutUserIndex = LocalPlayer ? FMath::Max(0, LocalPlayer->GetControllerId()) : 0;
	return FString::Printf(TEXT("GSTNCamera_Options_%d"), OutUserIndex);
}

bool UGSTN_CameraLocalPlayerSubsystem::SaveUserOptions()
{
	int32 UserIndex = 0;
	const FString Slot = GetSlotName(UserIndex);
	UGSTN_CameraOptionsSaveGame* Save = Cast<UGSTN_CameraOptionsSaveGame>(UGameplayStatics::CreateSaveGameObject(UGSTN_CameraOptionsSaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}
	Save->Options = Options;
	return UGameplayStatics::SaveGameToSlot(Save, Slot, UserIndex);
}

bool UGSTN_CameraLocalPlayerSubsystem::LoadUserOptions()
{
	int32 UserIndex = 0;
	const FString Slot = GetSlotName(UserIndex);
	if (!UGameplayStatics::DoesSaveGameExist(Slot, UserIndex))
	{
		return false;
	}
	if (const UGSTN_CameraOptionsSaveGame* Save = Cast<UGSTN_CameraOptionsSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, UserIndex)))
	{
		Options = Save->Options;
		return true;
	}
	return false;
}
