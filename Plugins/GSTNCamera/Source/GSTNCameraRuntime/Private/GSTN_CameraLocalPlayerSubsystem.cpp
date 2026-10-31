#include "GSTN_CameraLocalPlayerSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GSTN_CameraSettings.h"
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
	return Super::ShouldCreateSubsystem(Outer) && UGSTN_CameraSettings::Get().bEnabled && !bDedicated;
}

void UGSTN_CameraLocalPlayerSubsystem::EnsureInputContext(APlayerController* Controller)
{
	if (!Controller || AddedContext.IsValid())
	{
		return;
	}
	const UGSTN_CameraSettings& Settings = UGSTN_CameraSettings::Get();
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
