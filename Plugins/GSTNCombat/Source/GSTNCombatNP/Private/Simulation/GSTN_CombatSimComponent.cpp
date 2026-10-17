// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_CombatSimComponent.h"

#include "AbilitySystemGlobals.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "NetworkPredictionModelDef.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionProxyWrite.h"
#include "NetworkPredictionWorldManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCombatSim, Log, All);

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
	PrimaryComponentTick.bCanEverTick = false;
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
}

void UGSTN_CombatSimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystem && PreTickHandle.IsValid())
	{
		AbilitySystem->OnSimulationPreTick.Remove(PreTickHandle);
	}
	PreTickHandle.Reset();
	AbilitySystem = nullptr;

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
}

uint32 UGSTN_CombatSimComponent::SampleHeldButtons() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	const UEnhancedPlayerInput* PlayerInput = InputSubsystem ? InputSubsystem->GetPlayerInput() : nullptr;
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

void UGSTN_CombatSimComponent::SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<FGSTN_CombatStateTypes>& Input,
	const TNetSimOutput<FGSTN_CombatStateTypes>& Output)
{
	const FGSTN_CombatSyncState& InSync = *Input.Sync;
	FGSTN_CombatSyncState& OutSync = *Output.Sync;
	OutSync = InSync;

	const int32 Frame = TimeStep.Frame;
	const uint32 Held = Input.Cmd->HeldButtons;
	const uint32 Pressed = (Held & ~InSync.PrevHeldButtons) | Input.Cmd->PressedButtons;
	OutSync.PrevHeldButtons = Held;

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
