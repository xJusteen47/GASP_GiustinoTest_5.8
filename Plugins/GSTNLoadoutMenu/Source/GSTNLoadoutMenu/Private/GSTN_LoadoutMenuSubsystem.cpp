// Pre-mission loadout screen: one per local player.

#include "GSTN_LoadoutMenuSubsystem.h"

#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GSTN_LoadoutMenuRules.h"
#include "Input/GSTN_InputConfig.h"
#include "Input/GSTN_InputConfigSource.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Layout/GSTN_ButtonLayout.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Loadout/GSTN_LoadoutSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "SGSTN_LoadoutMenu.h"

#define LOCTEXT_NAMESPACE "GSTNLoadoutMenu"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNLoadoutMenu, Log, All);

namespace GSTNLoadoutMenu
{
	/** Button layout functions this screen reads (UGSTN_ButtonLayout); their buttons are data in DefaultGSTNInput.ini */
	const FName ConfirmFunction(TEXT("Menu.Confirm"));
	const FName BackFunction(TEXT("Menu.Back"));

	/** Above every gameplay context, so the menu's copies of the keys win and consume them */
	constexpr int32 ContextPriority = 100000;

	/** The resolved action of a layout function's button in Config, null when the layout or the config does not have it */
	const UInputAction* FindFunctionAction(const UGSTN_InputConfig& Config, FName Function)
	{
		const FGameplayTag Button = UGSTN_ButtonLayout::GetButton(Function, FGameplayTag());
		const int32 Index = Button.IsValid() ? Config.FindButtonIndex(Button) : INDEX_NONE;
		return Index != INDEX_NONE ? Config.GetButtonAction(Index) : nullptr;
	}
}

void UGSTN_LoadoutMenuSubsystem::Deinitialize()
{
	CloseMenu();
	Super::Deinitialize();
}

bool UGSTN_LoadoutMenuSubsystem::IsTickable() const
{
	return !HasAnyFlags(RF_ClassDefaultObject) && GetLocalPlayer() != nullptr;
}

UWorld* UGSTN_LoadoutMenuSubsystem::GetTickableGameObjectWorld() const
{
	const ULocalPlayer* Player = GetLocalPlayer();
	return Player ? Player->GetWorld() : nullptr;
}

TStatId UGSTN_LoadoutMenuSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_LoadoutMenuSubsystem, STATGROUP_Tickables);
}

APawn* UGSTN_LoadoutMenuSubsystem::GetPawn() const
{
	const ULocalPlayer* Player = GetLocalPlayer();
	UWorld* World = Player ? Player->GetWorld() : nullptr;
	const APlayerController* Controller = World ? Player->GetPlayerController(World) : nullptr;
	return Controller ? Controller->GetPawn() : nullptr;
}

UGSTN_LoadoutComponent* UGSTN_LoadoutMenuSubsystem::FindLoadoutComponent() const
{
	const APawn* Pawn = GetPawn();
	if (!Pawn)
	{
		return nullptr;
	}
	// The PlayerState's: the loadout survives respawn and replicates to everyone (UGSTN_LoadoutComponent); the pawn's as a fallback
	if (const APlayerState* State = Pawn->GetPlayerState())
	{
		if (UGSTN_LoadoutComponent* Found = State->FindComponentByClass<UGSTN_LoadoutComponent>())
		{
			return Found;
		}
	}
	return Pawn->FindComponentByClass<UGSTN_LoadoutComponent>();
}

bool UGSTN_LoadoutMenuSubsystem::ShouldAutoOpen(const UWorld& World) const
{
	// Automation and the command line switch keep it shut for unattended runs
	if (GIsAutomationTesting || FParse::Param(FCommandLine::Get(), TEXT("GSTNNoLoadoutMenu")))
	{
		return false;
	}
	return UGSTN_LoadoutMenuRules::Get().ShouldAutoOpenIn(World);
}

void UGSTN_LoadoutMenuSubsystem::Tick(float DeltaTime)
{
	const ULocalPlayer* Player = GetLocalPlayer();
	UWorld* World = Player ? Player->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	if (AutoOpenWorld.Get() != World)
	{
		CloseMenu();
		AutoOpenWorld = World;
		bAutoOpenDone = !ShouldAutoOpen(*World);
	}
	if (!bOpen && !bAutoOpenDone && OpenMenu())
	{
		// Once per level: the pre-mission choice. Later changes come from pickups (kits) or a game flow
		bAutoOpenDone = true;
	}
	if (bOpen)
	{
		if (!Component.IsValid() || !GetPawn())
		{
			CloseMenu();
			return;
		}
		HandleInput(DeltaTime);
	}
}

bool UGSTN_LoadoutMenuSubsystem::OpenMenu()
{
	if (bOpen)
	{
		return true;
	}
	ULocalPlayer* Player = GetLocalPlayer();
	UGSTN_LoadoutComponent* Found = FindLoadoutComponent();
	const UGSTN_InputConfig* Config = IGSTN_InputConfigSource::FindOn(GetPawn());
	if (!Player || !Player->ViewportClient || !Found || !Config || !SetUpInput(*Config))
	{
		return false;
	}
	Component = Found;
	Found->OnLoadoutRejected.AddUniqueDynamic(this, &UGSTN_LoadoutMenuSubsystem::HandleRejected);

	// Start from what the player has, else their last confirmed loadout, else the first kit the rules accept
	FGSTN_LoadoutData Start;
	if (Found->HasLoadout())
	{
		Start = Found->GetApplied();
	}
	else if (UGSTN_LoadoutSubsystem* Saved = Player->GetSubsystem<UGSTN_LoadoutSubsystem>())
	{
		Saved->LoadLoadout(UGSTN_LoadoutMenuRules::Get().LastLoadoutPreset, Start);
	}
	Draft.Begin(*Found, Start);

	SAssignNew(Widget, SGSTN_LoadoutMenu);
	Player->ViewportClient->AddViewportWidgetForPlayer(Player, Widget.ToSharedRef(), 50);
	bOpen = true;
	bConfirmHeld = true;
	bBackHeld = true;
	HeldDirection = FIntPoint::ZeroValue;
	RepeatTimer = 0.f;
	RefreshWidget();
	TArray<FString> RowNames;
	for (const FGSTN_LoadoutChoiceRow& Row : Draft.GetRows())
	{
		RowNames.Add(FString::Printf(TEXT("%s(%d)"), *Row.Id.ToString(), Row.Options.Num()));
	}
	UE_LOG(LogGSTNLoadoutMenu, Log, TEXT("%s: loadout screen open, kit %s, rows: %s"), *Player->GetName(), *Draft.GetData().PrimaryKit.ToString(), *FString::Join(RowNames, TEXT(" ")));
	return true;
}

void UGSTN_LoadoutMenuSubsystem::CloseMenu()
{
	if (!bOpen)
	{
		return;
	}
	bOpen = false;
	TearDownInput();
	ULocalPlayer* Player = GetLocalPlayer();
	if (Widget.IsValid() && Player && Player->ViewportClient)
	{
		Player->ViewportClient->RemoveViewportWidgetForPlayer(Player, Widget.ToSharedRef());
	}
	Widget.Reset();
	Feedback.Reset();
}

bool UGSTN_LoadoutMenuSubsystem::SetUpInput(const UGSTN_InputConfig& Config)
{
	ULocalPlayer* Player = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	const UEnhancedPlayerInput* PlayerInput = Input ? Input->GetPlayerInput() : nullptr;
	const UInputAction* SourceMove = Config.GetMoveAction();
	const UInputAction* SourceConfirm = GSTNLoadoutMenu::FindFunctionAction(Config, GSTNLoadoutMenu::ConfirmFunction);
	const UInputAction* SourceBack = GSTNLoadoutMenu::FindFunctionAction(Config, GSTNLoadoutMenu::BackFunction);
	if (!PlayerInput || !SourceMove || !SourceConfirm)
	{
		return false;
	}
	ConfirmHint = DescribeKeys(SourceConfirm);
	BackHint = SourceBack ? DescribeKeys(SourceBack) : FText::GetEmpty();

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name, RF_Transient);
		Action->ValueType = Type;
		Action->bConsumeInput = true;
		return Action;
	};
	MenuContext = NewObject<UInputMappingContext>(this, TEXT("IMC_GSTN_LoadoutMenu"), RF_Transient);
	MoveAction = MakeAction(TEXT("IA_GSTN_MenuMove"), EInputActionValueType::Axis2D);
	ConfirmAction = MakeAction(TEXT("IA_GSTN_MenuConfirm"), EInputActionValueType::Boolean);
	BackAction = MakeAction(TEXT("IA_GSTN_MenuBack"), EInputActionValueType::Boolean);
	SwallowAction = MakeAction(TEXT("IA_GSTN_MenuSwallow"), EInputActionValueType::Axis3D);

	// The player's keys as they are now (their remaps included), each onto a menu action; everything else is swallowed
	for (const FEnhancedActionKeyMapping& Mapping : PlayerInput->GetEnhancedActionMappingsView())
	{
		if (!Mapping.Action || !Mapping.Key.IsValid())
		{
			continue;
		}
		const UInputAction* Target = Mapping.Action == SourceMove ? MoveAction.Get()
			: Mapping.Action == SourceConfirm ? ConfirmAction.Get()
			: Mapping.Action == SourceBack ? BackAction.Get()
			: SwallowAction.Get();
		FEnhancedActionKeyMapping& Copy = MenuContext->MapKey(Target, Mapping.Key);
		if (Target == MoveAction)
		{
			// The modifiers make WASD a 2D axis (swizzle, negate) exactly as the player's move reads it
			Copy.Modifiers = Mapping.Modifiers;
		}
	}
	Input->AddMappingContext(MenuContext, GSTNLoadoutMenu::ContextPriority);
	return true;
}

void UGSTN_LoadoutMenuSubsystem::TearDownInput()
{
	ULocalPlayer* Player = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Input && MenuContext)
	{
		Input->RemoveMappingContext(MenuContext);
	}
	MenuContext = nullptr;
}

void UGSTN_LoadoutMenuSubsystem::HandleInput(float DeltaTime)
{
	const ULocalPlayer* Player = GetLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	const UEnhancedPlayerInput* PlayerInput = Input ? Input->GetPlayerInput() : nullptr;
	if (!PlayerInput)
	{
		return;
	}
	const UGSTN_LoadoutMenuRules& Rules = UGSTN_LoadoutMenuRules::Get();

	// Buttons act on the press; the first frame after opening only learns what is already held
	const bool bConfirm = ConfirmAction && PlayerInput->GetActionValue(ConfirmAction).Get<bool>();
	const bool bBack = BackAction && PlayerInput->GetActionValue(BackAction).Get<bool>();
	const bool bConfirmPressed = bConfirm && !bConfirmHeld;
	const bool bBackPressed = bBack && !bBackHeld;
	bConfirmHeld = bConfirm;
	bBackHeld = bBack;
	if (bConfirmPressed)
	{
		Confirm();
		return;
	}
	if (bBackPressed)
	{
		CloseMenu();
		return;
	}

	// The stick: the stronger axis wins, a held direction repeats
	const FVector2D Stick = MoveAction ? PlayerInput->GetActionValue(MoveAction).Get<FVector2D>() : FVector2D::ZeroVector;
	FIntPoint Direction = FIntPoint::ZeroValue;
	if (FMath::Abs(Stick.X) >= FMath::Abs(Stick.Y))
	{
		Direction.X = Stick.X > Rules.StickThreshold ? 1 : (Stick.X < -Rules.StickThreshold ? -1 : 0);
	}
	else
	{
		Direction.Y = Stick.Y > Rules.StickThreshold ? 1 : (Stick.Y < -Rules.StickThreshold ? -1 : 0);
	}
	bool bAct = false;
	if (Direction != HeldDirection)
	{
		HeldDirection = Direction;
		RepeatTimer = Rules.RepeatDelay;
		bAct = Direction != FIntPoint::ZeroValue;
	}
	else if (Direction != FIntPoint::ZeroValue)
	{
		RepeatTimer -= DeltaTime;
		if (RepeatTimer <= 0.f)
		{
			RepeatTimer = Rules.RepeatInterval;
			bAct = true;
		}
	}
	if (!bAct)
	{
		return;
	}
	Feedback.Reset();
	if (Direction.Y != 0)
	{
		// Stick up (+Y) goes to the row above
		Draft.MoveCursor(-Direction.Y);
	}
	else
	{
		Draft.Cycle(Direction.X);
	}
	RefreshWidget();
}

void UGSTN_LoadoutMenuSubsystem::Confirm()
{
	UGSTN_LoadoutComponent* Target = Component.Get();
	if (!Target)
	{
		CloseMenu();
		return;
	}
	if (!Draft.IsValid())
	{
		Feedback = Draft.GetProblem();
		RefreshWidget();
		return;
	}
	const FGSTN_LoadoutData& Chosen = Draft.GetData();
	if (!Target->HasLoadout() || !(Chosen == Target->GetApplied()))
	{
		Target->ServerRequestLoadout(Chosen);
	}
	if (UGSTN_LoadoutSubsystem* Saved = GetLocalPlayer()->GetSubsystem<UGSTN_LoadoutSubsystem>())
	{
		Saved->SaveLoadout(UGSTN_LoadoutMenuRules::Get().LastLoadoutPreset, Chosen);
	}
	UE_LOG(LogGSTNLoadoutMenu, Log, TEXT("%s: loadout sent (kit %s)"), *GetLocalPlayer()->GetName(), *Chosen.PrimaryKit.ToString());
	CloseMenu();
}

void UGSTN_LoadoutMenuSubsystem::HandleRejected(UGSTN_LoadoutComponent* Rejecting, const TArray<FString>& Errors)
{
	// The server judged it otherwise (content differs, a rule of the mode): back to the screen with the finding
	UE_LOG(LogGSTNLoadoutMenu, Warning, TEXT("Loadout refused by the server: %s"), *FString::Join(Errors, TEXT(" | ")));
	if (OpenMenu())
	{
		Feedback = Errors.IsEmpty() ? FString() : Errors[0];
		RefreshWidget();
	}
}

FText UGSTN_LoadoutMenuSubsystem::DescribeKeys(const UInputAction* Action) const
{
	const ULocalPlayer* Player = GetLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input || !Action)
	{
		return FText::GetEmpty();
	}
	// The pad first: the screen is made for it
	TArray<FKey> Keys = Input->QueryKeysMappedToAction(Action);
	Keys.StableSort([](const FKey& A, const FKey& B) { return A.IsGamepadKey() && !B.IsGamepadKey(); });
	TArray<FString> Names;
	for (const FKey& Key : Keys)
	{
		Names.AddUnique(Key.GetDisplayName(false).ToString());
	}
	return FText::FromString(FString::Join(Names, TEXT(" / ")));
}

void UGSTN_LoadoutMenuSubsystem::RefreshWidget()
{
	if (!Widget.IsValid())
	{
		return;
	}
	const FText Hint = BackHint.IsEmpty()
		? FText::Format(LOCTEXT("HintConfirm", "Levetta: scegli    {0}: conferma"), ConfirmHint)
		: FText::Format(LOCTEXT("Hint", "Levetta: scegli    {0}: conferma    {1}: esci"), ConfirmHint, BackHint);
	Widget->Refresh(Draft, Hint, Feedback);
}

#undef LOCTEXT_NAMESPACE
