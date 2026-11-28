// Per-player input options (no GAS, no Network Prediction).

#include "Input/GSTN_InputOptionsSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"

/**
 * Watches the raw input of one local player to tell which device they play with. Local presentation of a choice only: what it decides
 * reaches the simulation solely through the input profile, a loadout revision agreed with the host. Mouse movement and small stick
 * noise are ignored, so a pad player touching the desk does not switch.
 */
class FGSTN_InputDeviceWatcher : public IInputProcessor
{
public:
	explicit FGSTN_InputDeviceWatcher(UGSTN_InputOptionsSubsystem& InOwner) : Owner(&InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		Report(InKeyEvent.GetUserIndex(), InKeyEvent.GetKey().IsGamepadKey());
		return false;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override
	{
		if (InAnalogInputEvent.GetKey().IsGamepadKey() && FMath::Abs(InAnalogInputEvent.GetAnalogValue()) >= 0.5f)
		{
			Report(InAnalogInputEvent.GetUserIndex(), true);
		}
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		Report(MouseEvent.GetUserIndex(), false);
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("GSTN_InputDeviceWatcher"); }

	void Detach() { Owner.Reset(); }

private:
	void Report(int32 SlateUserIndex, bool bGamepad)
	{
		UGSTN_InputOptionsSubsystem* Subsystem = Owner.Get();
		ULocalPlayer* LocalPlayer = Subsystem ? Subsystem->GetLocalPlayer() : nullptr;
		if (!LocalPlayer)
		{
			return;
		}
		const TSharedPtr<FSlateUser> User = LocalPlayer->GetSlateUser();
		const int32 MyIndex = User.IsValid() ? User->GetUserIndex() : LocalPlayer->GetControllerId();
		if (SlateUserIndex == MyIndex)
		{
			Subsystem->NotifyDeviceUsed(bGamepad ? EGSTN_InputDevice::Gamepad : EGSTN_InputDevice::MouseKeyboard);
		}
	}

	TWeakObjectPtr<UGSTN_InputOptionsSubsystem> Owner;
};

void UGSTN_InputOptionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGSTN_InputOptionsSave* Saved = Cast<UGSTN_InputOptionsSave>(UGameplayStatics::LoadGameFromSlot(GetSlotName(), 0)))
	{
		Options = Saved->Options;
	}
	if (FSlateApplication::IsInitialized())
	{
		DeviceWatcher = MakeShared<FGSTN_InputDeviceWatcher>(*this);
		FSlateApplication::Get().RegisterInputPreProcessor(DeviceWatcher);
	}
}

void UGSTN_InputOptionsSubsystem::Deinitialize()
{
	if (DeviceWatcher.IsValid())
	{
		DeviceWatcher->Detach();
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(DeviceWatcher);
		}
		DeviceWatcher.Reset();
	}
	Super::Deinitialize();
}

FGSTN_InputProfile UGSTN_InputOptionsSubsystem::MakeProfile() const
{
	FGSTN_InputProfile Profile;
	switch (Options.DeviceChoice)
	{
	case EGSTN_InputDeviceChoice::Gamepad: Profile.Device = EGSTN_InputDevice::Gamepad; break;
	case EGSTN_InputDeviceChoice::MouseKeyboard: Profile.Device = EGSTN_InputDevice::MouseKeyboard; break;
	default: Profile.Device = DetectedDevice; break;
	}
	Profile.ButtonOverrides = Options.ButtonOverrides;
	Profile.CommandPicks = Options.CommandPicks;
	return Profile;
}

void UGSTN_InputOptionsSubsystem::NotifyDeviceUsed(const EGSTN_InputDevice Device)
{
	if (Device == DetectedDevice)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now - LastSwitchSeconds < MinSwitchSeconds)
	{
		return;
	}
	LastSwitchSeconds = Now;
	DetectedDevice = Device;
	if (Options.DeviceChoice == EGSTN_InputDeviceChoice::Auto)
	{
		OnChanged.Broadcast(Options);
	}
}

void UGSTN_InputOptionsSubsystem::SetOptions(const FGSTN_InputOptions& NewOptions)
{
	Options = NewOptions;
	Options.MoveStickDeadZone = FMath::Clamp(Options.MoveStickDeadZone, 0.f, 0.5f);
	Options.StickThresholds = Options.StickThresholds.Sanitized();
	Options.ButtonOverrides.SetNum(FMath::Min(Options.ButtonOverrides.Num(), FGSTN_InputProfile::MaxEntries));
	Options.CommandPicks.SetNum(FMath::Min(Options.CommandPicks.Num(), FGSTN_InputProfile::MaxEntries));
	if (UGSTN_InputOptionsSave* Save = Cast<UGSTN_InputOptionsSave>(UGameplayStatics::CreateSaveGameObject(UGSTN_InputOptionsSave::StaticClass())))
	{
		Save->Options = Options;
		UGameplayStatics::SaveGameToSlot(Save, GetSlotName(), 0);
	}
	OnChanged.Broadcast(Options);
}

FVector2D UGSTN_InputOptionsSubsystem::ApplyRadialDeadZone(const FVector2D& Stick, float DeadZone)
{
	const double Size = Stick.Size();
	const double Dead = FMath::Clamp(static_cast<double>(DeadZone), 0.0, 0.95);
	if (Size <= Dead || Size <= UE_DOUBLE_SMALL_NUMBER)
	{
		return FVector2D::ZeroVector;
	}
	const double Remapped = FMath::Min((Size - Dead) / (1.0 - Dead), 1.0);
	return Stick * (Remapped / Size);
}

FGSTN_InputOptions UGSTN_InputOptionsSubsystem::GetFor(const ULocalPlayer* LocalPlayer)
{
	const UGSTN_InputOptionsSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UGSTN_InputOptionsSubsystem>(LocalPlayer);
	return Subsystem ? Subsystem->Options : FGSTN_InputOptions();
}

FString UGSTN_InputOptionsSubsystem::GetSlotName() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return FString::Printf(TEXT("GSTN_InputOptions_%d"), LocalPlayer ? LocalPlayer->GetLocalPlayerIndex() : 0);
}

namespace
{
	FAutoConsoleCommandWithWorldAndArgs GInputOptionsCommand(
		TEXT("gstn.Input.Options"),
		TEXT("Local player 0's input options. No argument prints them; '<DeadZone|Neutral|Aim|Flick> <value>' sets one and saves it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ULocalPlayer* LocalPlayer = World ? World->GetFirstLocalPlayerFromController() : nullptr;
			UGSTN_InputOptionsSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UGSTN_InputOptionsSubsystem>(LocalPlayer);
			if (!Subsystem)
			{
				UE_LOG(LogTemp, Warning, TEXT("gstn.Input.Options: no local player"));
				return;
			}
			FGSTN_InputOptions Options = Subsystem->GetOptions();
			if (Args.Num() >= 2)
			{
				const float Value = FCString::Atof(*Args[1]);
				if (Args[0].Equals(TEXT("DeadZone"), ESearchCase::IgnoreCase)) { Options.MoveStickDeadZone = Value; }
				else if (Args[0].Equals(TEXT("Neutral"), ESearchCase::IgnoreCase)) { Options.StickThresholds.NeutralZone = Value; }
				else if (Args[0].Equals(TEXT("Aim"), ESearchCase::IgnoreCase)) { Options.StickThresholds.AimZone = Value; }
				else if (Args[0].Equals(TEXT("Flick"), ESearchCase::IgnoreCase)) { Options.StickThresholds.FlickFrames = FMath::RoundToInt(Value); }
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("gstn.Input.Options: unknown field %s (DeadZone, Neutral, Aim, Flick)"), *Args[0]);
					return;
				}
				Subsystem->SetOptions(Options);
				Options = Subsystem->GetOptions();
			}
			UE_LOG(LogTemp, Display, TEXT("gstn.Input.Options: DeadZone %.3f, Neutral %.3f, Aim %.3f, Flick %d"), Options.MoveStickDeadZone,
				Options.StickThresholds.NeutralZone, Options.StickThresholds.AimZone, Options.StickThresholds.FlickFrames);
		}));
}
