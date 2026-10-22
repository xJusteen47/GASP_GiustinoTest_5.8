// Producers of the input command (GAS-NPP fork).

#include "Sources/GSTN_InputSource.h"

#include "Bindings/GSTN_CommandBindingSet.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/GSTN_DirectionClassifier.h"
#include "Input/GSTN_InputConfig.h"
#include "InputAction.h"

void UGSTN_PlayerInputSource::ProduceInput(const FGSTN_InputSourceContext& Context, int32 DeltaTimeMS, FGSTN_InputCmd& Out)
{
	const APawn* Pawn = Cast<APawn>(Context.Owner);
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	const UEnhancedPlayerInput* PlayerInput = InputSubsystem ? InputSubsystem->GetPlayerInput() : nullptr;
	if (!PlayerInput || !Context.Config)
	{
		return;
	}

	// The input component exists only once the pawn is possessed locally; until then the held read below is all there is
	if (UEnhancedInputComponent* InputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
	{
		EnsureBound(*InputComponent, *Context.Config);
	}

	// The actions were resolved when the host set up (UGSTN_InputConfig::ResolveActions): nothing is loaded here
	const int32 NumButtons = FMath::Min(Context.Config->Buttons.Num(), UGSTN_InputConfig::MaxButtons);
	for (int32 Index = 0; Index < NumButtons; ++Index)
	{
		const UInputAction* Action = Context.Config->GetButtonAction(Index);
		if (Action && PlayerInput->GetActionValue(Action).Get<bool>())
		{
			Out.HeldButtons |= 1u << Index;
		}
	}
	Out.PressedButtons |= PendingTaps;
	PendingTaps = 0;

	if (const UInputAction* MoveAction = Context.Config->GetMoveAction())
	{
		FVector2D Stick = PlayerInput->GetActionValue(MoveAction).Get<FVector2D>();
		if (Stick.SizeSquared() > 1.0)
		{
			Stick = Stick.GetSafeNormal();
		}
		Out.StickX = FGSTN_DirectionClassifier::QuantizeStickAxis(static_cast<float>(Stick.X));
		Out.StickY = FGSTN_DirectionClassifier::QuantizeStickAxis(static_cast<float>(Stick.Y));
	}

	Out.CameraYaw = FGSTN_DirectionClassifier::QuantizeYaw(static_cast<float>(PlayerController->GetControlRotation().Yaw));
}

void UGSTN_PlayerInputSource::EnsureBound(UEnhancedInputComponent& InputComponent, const UGSTN_InputConfig& Config)
{
	if (BoundComponent.Get() == &InputComponent && BoundConfig.Get() == &Config)
	{
		return;
	}

	Unbind();
	BoundComponent = &InputComponent;
	BoundConfig = &Config;

	const int32 NumButtons = FMath::Min(Config.Buttons.Num(), UGSTN_InputConfig::MaxButtons);
	for (int32 Index = 0; Index < NumButtons; ++Index)
	{
		if (const UInputAction* Action = Config.GetButtonAction(Index))
		{
			BindingHandles.Add(InputComponent.BindAction(Action, ETriggerEvent::Started, this, &UGSTN_PlayerInputSource::HandleButtonStarted).GetHandle());
		}
	}
}

void UGSTN_PlayerInputSource::Unbind()
{
	if (UEnhancedInputComponent* InputComponent = BoundComponent.Get())
	{
		for (const uint32 Handle : BindingHandles)
		{
			InputComponent->RemoveBindingByHandle(Handle);
		}
	}
	BindingHandles.Reset();
	BoundComponent.Reset();
	BoundConfig.Reset();
}

void UGSTN_PlayerInputSource::HandleButtonStarted(const FInputActionInstance& Instance)
{
	const UGSTN_InputConfig* Config = BoundConfig.Get();
	if (!Config)
	{
		return;
	}

	const int32 NumButtons = FMath::Min(Config->Buttons.Num(), UGSTN_InputConfig::MaxButtons);
	for (int32 Index = 0; Index < NumButtons; ++Index)
	{
		if (Config->GetButtonAction(Index) == Instance.GetSourceAction().Get())
		{
			PendingTaps |= 1u << Index;
		}
	}
}

void UGSTN_PlayerInputSource::BeginDestroy()
{
	Unbind();
	Super::BeginDestroy();
}

void UGSTN_AIInputSource::ProduceInput(const FGSTN_InputSourceContext& Context, int32 DeltaTimeMS, FGSTN_InputCmd& Out)
{
	Out.HeldButtons = Held;
	Out.PressedButtons = Taps;
	Taps = 0;

	FVector2D Clamped = Stick;
	if (Clamped.SizeSquared() > 1.0)
	{
		Clamped = Clamped.GetSafeNormal();
	}
	Out.StickX = FGSTN_DirectionClassifier::QuantizeStickAxis(static_cast<float>(Clamped.X));
	Out.StickY = FGSTN_DirectionClassifier::QuantizeStickAxis(static_cast<float>(Clamped.Y));

	if (ForcedCommand.IsValid() && Context.Table)
	{
		Out.ForcedCommand = Context.Table->ToForcedCommand(ForcedCommand);
	}
	ForcedCommand = FGameplayTag();
}

void UGSTN_AIInputSource::SetHeld(int32 Mask)
{
	Held = static_cast<uint32>(Mask);
}

void UGSTN_AIInputSource::Tap(int32 Mask)
{
	Taps |= static_cast<uint32>(Mask);
}

void UGSTN_AIInputSource::SetStick(FVector2D WorldPlane)
{
	Stick = WorldPlane;
}

void UGSTN_AIInputSource::RequestCommand(FGameplayTag Command)
{
	ForcedCommand = Command;
}
