// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_CombatSimComponent.h"

#include "AbilitySystemGlobals.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/GSTN_CombatInputSettings.h"
#include "Input/GSTN_DirectionClassifier.h"
#include "Input/GSTN_InputScript.h"
#include "InputAction.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "NetworkPredictionModelDef.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionProxyWrite.h"
#include "NetworkPredictionWorldManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCombatSim, Log, All);

static TAutoConsoleVariable<int32> CVarCombatDebugInput(TEXT("gstn.Combat.Debug.Input"), 0,
	TEXT("Draws the combat input history above the locally controlled pawns (the notation of FGSTN_InputScript, oldest first)."), ECVF_Default);

/**
 * Sorted right before the ability system (PreKinematicMovers - 2), itself before Mover: the combat state of a frame is
 * ready when the ability system ticks that frame
 */
class FGSTN_CombatModelDef : public FNetworkPredictionModelDef
{
public:
	NP_MODEL_BODY();

	using Simulation = UGSTN_CombatSimComponent;
	using StateTypes = FGSTN_CombatStateTypes;
	using Driver = UGSTN_CombatSimComponent;

	static const TCHAR* GetName() { return TEXT("GSTN_Combat"); }
	static constexpr int32 GetSortPriority() { return (int32)ENetworkPredictionSortPriority::PreKinematicMovers - 3; }
};

NP_MODEL_REGISTER(FGSTN_CombatModelDef);

UGSTN_CombatSimComponent::UGSTN_CombatSimComponent()
{
	// Only for the debug display; the simulation is ticked by Network Prediction
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void UGSTN_CombatSimComponent::InitializeNetworkPredictionProxy()
{
	NetworkPredictionProxy.Init<FGSTN_CombatModelDef>(GetWorld(), GetReplicationProxies(), this, this);
}

void UGSTN_CombatSimComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ButtonActions.Num() > 32)
	{
		UE_LOG(LogGSTNCombatSim, Error, TEXT("%s: %d button actions, only the first 32 are sampled"), *GetPathName(), ButtonActions.Num());
	}

	BindAbilitySystem();

	// The facing the classifier needs is the Mover's simulated one at the end of the previous frame, not the actor's
	if (UMoverComponent* Mover = GetOwner()->FindComponentByClass<UMoverComponent>())
	{
		MoverTracker = UGSTN_MoverSimStateTracker::FindOrAdd(Mover);
	}
	else
	{
		UE_LOG(LogGSTNCombatSim, Warning, TEXT("%s: no Mover component on the owner, the character is taken to face yaw 0"), *GetPathName());
	}
}

void UGSTN_CombatSimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystem && PreTickHandle.IsValid())
	{
		AbilitySystem->OnSimulationPreTick.Remove(PreTickHandle);
	}
	PreTickHandle.Reset();
	AbilitySystem = nullptr;
	MoverTracker = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UGSTN_CombatSimComponent::BindAbilitySystem()
{
	AbilitySystem = Cast<UNpAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
	if (!AbilitySystem)
	{
		UE_LOG(LogGSTNCombatSim, Warning, TEXT("%s: no UNpAbilitySystemComponent on the owner, combat requests are ignored"), *GetPathName());
		return;
	}

	PreTickHandle = AbilitySystem->OnSimulationPreTick.AddUObject(this, &UGSTN_CombatSimComponent::HandleAbilitySimulationPreTick);
}

void UGSTN_CombatSimComponent::SetScriptedButtons(int32 Mask)
{
	ScriptedButtons = static_cast<uint32>(Mask);
	bUseScriptedButtons = true;
}

void UGSTN_CombatSimComponent::ClearScriptedButtons()
{
	ScriptedButtons = 0;
	bUseScriptedButtons = false;
}

void UGSTN_CombatSimComponent::TapScriptedButtons(int32 Mask)
{
	PendingTaps |= static_cast<uint32>(Mask);
}

const FGSTN_CombatSyncState* UGSTN_CombatSimComponent::GetSyncState() const
{
	return NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>();
}

void UGSTN_CombatSimComponent::InitializeSimulationState(FGSTN_CombatSyncState* Sync, FGSTN_CombatAuxState* Aux)
{
	if (Sync)
	{
		*Sync = FGSTN_CombatSyncState();
	}
}

void UGSTN_CombatSimComponent::ProduceInput(const int32 DeltaTimeMS, FGSTN_CombatInputCmd* Cmd)
{
	Cmd->HeldButtons = bUseScriptedButtons ? ScriptedButtons : SampleHeldButtons();
	Cmd->PressedButtons = PendingTaps;
	PendingTaps = 0;
	SampleStick(*Cmd);
}

const UEnhancedPlayerInput* UGSTN_CombatSimComponent::GetPlayerInput() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	return InputSubsystem ? InputSubsystem->GetPlayerInput() : nullptr;
}

uint32 UGSTN_CombatSimComponent::SampleHeldButtons() const
{
	const UEnhancedPlayerInput* PlayerInput = GetPlayerInput();
	if (!PlayerInput)
	{
		return 0;
	}

	uint32 Held = 0;
	const int32 NumButtons = FMath::Min(ButtonActions.Num(), 32);
	for (int32 Index = 0; Index < NumButtons; ++Index)
	{
		if (ButtonActions[Index] && PlayerInput->GetActionValue(ButtonActions[Index]).Get<bool>())
		{
			Held |= 1u << Index;
		}
	}
	return Held;
}

void UGSTN_CombatSimComponent::SampleStick(FGSTN_CombatInputCmd& Cmd) const
{
	const UEnhancedPlayerInput* PlayerInput = GetPlayerInput();
	if (!PlayerInput)
	{
		return;
	}

	if (MoveAction)
	{
		FVector2D Stick = PlayerInput->GetActionValue(MoveAction).Get<FVector2D>();
		if (Stick.SizeSquared() > 1.0)
		{
			Stick = Stick.GetSafeNormal();
		}
		Cmd.StickX = FGSTN_DirectionClassifier::QuantizeStickAxis(static_cast<float>(Stick.X));
		Cmd.StickY = FGSTN_DirectionClassifier::QuantizeStickAxis(static_cast<float>(Stick.Y));
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (const AController* Controller = Pawn ? Pawn->GetController() : nullptr)
	{
		Cmd.CameraYaw = FGSTN_DirectionClassifier::QuantizeYaw(static_cast<float>(Controller->GetControlRotation().Yaw));
	}
}

float UGSTN_CombatSimComponent::GetSimulatedFacingYaw() const
{
	if (MoverTracker)
	{
		if (const FMoverDefaultSyncState* DefaultSync = MoverTracker->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
		{
			return static_cast<float>(DefaultSync->GetOrientation_WorldSpace().Yaw);
		}
	}
	return 0.f;
}

void UGSTN_CombatSimComponent::SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<FGSTN_CombatStateTypes>& Input,
	const TNetSimOutput<FGSTN_CombatStateTypes>& Output)
{
	const FGSTN_CombatSyncState& InSync = *Input.Sync;
	FGSTN_CombatSyncState& OutSync = *Output.Sync;
	OutSync = InSync;

	const int32 Frame = TimeStep.Frame;
	const FGSTN_CombatInputCmd& Cmd = *Input.Cmd;
	const UGSTN_CombatInputSettings& Settings = UGSTN_CombatInputSettings::Get();

	// This frame's sample: the buttons (a tap shorter than a frame is released on the frame it is pressed) and the stick
	// classified in every direction space with the facing the Mover had at the end of the previous frame.
	const uint32 PrevHeld = InSync.History.GetHeldButtons();
	FGSTN_InputSample Sample;
	Sample.Frame = Frame;
	Sample.Held = Cmd.HeldButtons;
	Sample.Pressed = (Cmd.HeldButtons & ~PrevHeld) | Cmd.PressedButtons;
	Sample.Released = (PrevHeld | Sample.Pressed) & ~Cmd.HeldButtons;
	Sample.Stick = FGSTN_DirectionClassifier::Classify(
		FVector2D(FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.StickX), FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.StickY)),
		FGSTN_DirectionClassifier::DequantizeYaw(Cmd.CameraYaw), GetSimulatedFacingYaw(), InSync.Classifier, Frame, Settings, OutSync.Classifier);
	OutSync.History.Append(Sample);

	const uint32 Pressed = Sample.Pressed;

	// Spike: presses during hitstop are dropped (buffering comes with the input history)
	if (Pressed == 0 || InSync.IsInHitstop(Frame))
	{
		return;
	}

	const int32 NumAbilities = FMath::Min(ButtonAbilities.Num(), 32);
	for (int32 Index = 0; Index < NumAbilities; ++Index)
	{
		if ((Pressed & (1u << Index)) && ButtonAbilities[Index])
		{
			OutSync.RequestedAction = static_cast<uint8>(Index + 1);
			OutSync.RequestFrame = Frame;
			break;
		}
	}

	if (HitstopTestButton >= 0 && (Pressed & (1u << HitstopTestButton)))
	{
		OutSync.HitstopStartFrame = Frame;
		OutSync.HitstopEndFrame = Frame + HitstopTestFrames;
	}
}

FString UGSTN_CombatSimComponent::GetDebugString(int32 MaxEvents) const
{
	const FGSTN_CombatSyncState* Sync = GetSyncState();
	if (!Sync)
	{
		return TEXT("(no combat state)");
	}
	return FString::Printf(TEXT("%s\nheld 0x%x  frame %d  request %d@%d"), *FGSTN_InputScript::Describe(Sync->History, MaxEvents),
		Sync->History.GetHeldButtons(), Sync->History.GetLatestFrame(), Sync->RequestedAction, Sync->RequestFrame);
}

void UGSTN_CombatSimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if ENABLE_DRAW_DEBUG
	if (CVarCombatDebugInput.GetValueOnGameThread() != 0)
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		if (Pawn && Pawn->IsLocallyControlled())
		{
			DrawDebugString(GetWorld(), Pawn->GetActorLocation() + FVector(0.0, 0.0, 140.0), GetDebugString(), nullptr, FColor::White, 0.f, true);
		}
	}
#endif
}

void UGSTN_CombatSimComponent::FinalizeFrame(const FGSTN_CombatSyncState* Sync, const FGSTN_CombatAuxState* Aux)
{
}

void UGSTN_CombatSimComponent::HandleAbilitySimulationPreTick(const FAbilitySystemTimeStep& TimeStep)
{
	// The combat model ticked this frame already: its pending state is this frame's output
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (!Sync)
	{
		return;
	}

	const int32 Frame = TimeStep.ServerFrame;

	if (Sync->RequestFrame == Frame && ButtonAbilities.IsValidIndex(Sync->RequestedAction - 1))
	{
		if (const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(ButtonAbilities[Sync->RequestedAction - 1]))
		{
			AbilitySystem->TryActivateAbility(Spec->Handle);
		}
	}

	// Paused on every hitstop frame with a montage (also one started during the hitstop), resumed on the first frame after.
	// The pause flag only exists with a montage: it is not replicated otherwise
	float MontageTime = 0.f;
	if (Sync->IsInHitstop(Frame) && AbilitySystem->GetPlayingSyncedMontage(MontageTime))
	{
		AbilitySystem->PauseCurrentSyncedMontage();
	}
	else if (Frame == Sync->HitstopEndFrame)
	{
		AbilitySystem->ResumeCurrentSyncedMontage();
	}
}
