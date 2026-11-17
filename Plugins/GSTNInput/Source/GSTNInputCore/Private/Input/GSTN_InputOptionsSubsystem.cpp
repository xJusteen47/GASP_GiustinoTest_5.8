// Per-player input options (no GAS, no Network Prediction).

#include "Input/GSTN_InputOptionsSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

void UGSTN_InputOptionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGSTN_InputOptionsSave* Saved = Cast<UGSTN_InputOptionsSave>(UGameplayStatics::LoadGameFromSlot(GetSlotName(), 0)))
	{
		Options = Saved->Options;
	}
}

void UGSTN_InputOptionsSubsystem::SetOptions(const FGSTN_InputOptions& NewOptions)
{
	Options = NewOptions;
	Options.MoveStickDeadZone = FMath::Clamp(Options.MoveStickDeadZone, 0.f, 0.5f);
	Options.StickThresholds = Options.StickThresholds.Sanitized();
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
