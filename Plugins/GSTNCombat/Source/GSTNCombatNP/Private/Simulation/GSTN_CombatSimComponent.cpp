// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_CombatSimComponent.h"

#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Context/GSTN_InputContextProvider.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "Input/GSTN_InputConfig.h"
#include "Input/GSTN_InputScript.h"
#include "Input/GSTN_InputSettings.h"
#include "NetworkPredictionModelDef.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionProxyWrite.h"
#include "NetworkPredictionWorldManager.h"
#include "Simulation/GSTN_InputHostUtils.h"
#include "Sources/GSTN_InputSource.h"

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

	InputSource = CreateDefaultSubobject<UGSTN_PlayerInputSource>(TEXT("InputSource"));
	ContextProvider = CreateDefaultSubobject<UGSTN_MoverInputContextProvider>(TEXT("ContextProvider"));
}

void UGSTN_CombatSimComponent::InitializeNetworkPredictionProxy()
{
	NetworkPredictionProxy.Init<FGSTN_CombatModelDef>(GetWorld(), GetReplicationProxies(), this, this);
}

void UGSTN_CombatSimComponent::BeginPlay()
{
	Super::BeginPlay();

	CompileInput();
	BindAbilitySystem();

	if (ContextProvider)
	{
		ContextProvider->Initialize(GetOwner());
	}
	else
	{
		UE_LOG(LogGSTNCombatSim, Warning, TEXT("%s: no context provider, the character is taken to face yaw 0 with no movement context"), *GetPathName());
	}
}

void UGSTN_CombatSimComponent::CompileInput()
{
	// Everything the simulation or the input source reads every frame is resolved here, once, and held strongly
	ActiveInputConfig = InputConfig.IsNull() ? nullptr : InputConfig.LoadSynchronous();
	ActiveSlotBindings = SlotBindings.IsNull() ? nullptr : SlotBindings.LoadSynchronous();

	if (ActiveInputConfig)
	{
		ActiveInputConfig->ResolveActions();
		ButtonMap = ActiveInputConfig->MakeButtonMap();
		if (ActiveInputConfig->Buttons.Num() > UGSTN_InputConfig::MaxButtons)
		{
			UE_LOG(LogGSTNCombatSim, Error, TEXT("%s: %d buttons, only the first %d are sampled"), *GetPathName(), ActiveInputConfig->Buttons.Num(), UGSTN_InputConfig::MaxButtons);
		}
	}
	else
	{
		UE_LOG(LogGSTNCombatSim, Warning, TEXT("%s: no input config, the combat input has no buttons"), *GetPathName());
	}

	if (ActiveSlotBindings)
	{
		CommandTable = ActiveSlotBindings->BuildTable(ResetRules);
	}
	else
	{
		UE_LOG(LogGSTNCombatSim, Warning, TEXT("%s: no slot bindings, no command starts an ability"), *GetPathName());
	}

	if (InputSource && !Cast<IGSTN_InputSource>(InputSource.Get()))
	{
		UE_LOG(LogGSTNCombatSim, Error, TEXT("%s: the input source %s does not implement IGSTN_InputSource"), *GetPathName(), *InputSource->GetName());
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

void UGSTN_CombatSimComponent::ProduceInput(const int32 DeltaTimeMS, FGSTN_InputCmd* Cmd)
{
	*Cmd = FGSTN_InputCmd();

	if (IGSTN_InputSource* Source = Cast<IGSTN_InputSource>(InputSource.Get()))
	{
		FGSTN_InputSourceContext Context;
		Context.Owner = GetOwner();
		Context.Config = ActiveInputConfig;
		Context.Table = &CommandTable;
		Source->ProduceInput(Context, DeltaTimeMS, *Cmd);
	}

	// Test overrides: the scripted buttons replace what the source saw, taps are added to the presses
	if (bUseScriptedButtons)
	{
		Cmd->HeldButtons = ScriptedButtons;
		Cmd->PressedButtons = 0;
	}
	Cmd->PressedButtons |= PendingTaps;
	PendingTaps = 0;
}

float UGSTN_CombatSimComponent::GetFacingYaw() const
{
	return ContextProvider ? ContextProvider->GetFacingYaw() : 0.f;
}

void UGSTN_CombatSimComponent::GetOwnerTags(FGameplayTagContainer& OutTags) const
{
	if (AbilitySystem)
	{
		AbilitySystem->GetOwnedGameplayTags(OutTags);
	}
}

void UGSTN_CombatSimComponent::AppendContextTags(FGameplayTagContainer& OutTags) const
{
	if (ContextProvider)
	{
		ContextProvider->AppendContextTags(OutTags);
	}
}

uint8 UGSTN_CombatSimComponent::GetContextFingerprint() const
{
	return ContextProvider ? ContextProvider->GetContextFingerprint() : 0;
}

void UGSTN_CombatSimComponent::AppendFrameEvents(uint8 PreviousFingerprint, uint8 CurrentFingerprint, FGSTN_BufferFrameEvents& Events) const
{
	if (ContextProvider)
	{
		ContextProvider->AppendFrameEvents(PreviousFingerprint, CurrentFingerprint, Events);
	}
}

bool UGSTN_CombatSimComponent::CanExecuteBinding(int32 BindingIndex) const
{
	// On the end-of-previous-frame state, like the ability system's own check at execution
	return FGSTN_InputHostUtils::CanExecuteBinding(AbilitySystem, CommandTable, BindingIndex);
}

FGSTN_ActivationContext UGSTN_CombatSimComponent::GetActivationContext() const
{
	// The simulation read: while the ability system ticks frame F this is the combat state F produced, request included
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	return Sync ? FGSTN_ActivationContext::FromRequest(Sync->Input, CommandTable, Sync->Input.History.GetLatestFrame()) : FGSTN_ActivationContext();
}

bool UGSTN_CombatSimComponent::IsCommandButtonHeld(int32 Button) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	return Sync && FGSTN_InputHostUtils::IsButtonHeld(Sync->Input, Button);
}

void UGSTN_CombatSimComponent::SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<FGSTN_CombatStateTypes>& Input,
	const TNetSimOutput<FGSTN_CombatStateTypes>& Output)
{
	const FGSTN_CombatSyncState& InSync = *Input.Sync;
	FGSTN_CombatSyncState& OutSync = *Output.Sync;
	OutSync = InSync;

	const int32 Frame = TimeStep.Frame;

	FGSTN_InputTickArgs Args;
	Args.Frame = Frame;
	Args.Cmd = Input.Cmd;
	Args.In = &InSync.Input;
	Args.Out = &OutSync.Input;
	Args.Table = &CommandTable;
	Args.Buttons = &ButtonMap;
	Args.Settings = &UGSTN_InputSettings::Get();
	Args.GlobalResetRules = ResetRules;
	Args.PausedStart = InSync.HitstopStartFrame;
	Args.PausedEnd = InSync.HitstopEndFrame;
	Args.Outbox = &EventOutbox;
	const FGSTN_InputTickResult Result = FGSTN_InputSimulator::Tick(Args, *this);

	// Debug hitstop, until hits replace it: a press of the test button opens a window (not while one is already open)
	if (HitstopTestButton >= 0 && !InSync.IsInHitstop(Frame) && (Result.Pressed & (1u << HitstopTestButton)))
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
	return FString::Printf(TEXT("%s\nheld 0x%x  frame %d  request %d@%d button %d"), *FGSTN_InputScript::Describe(Sync->Input.History, MaxEvents),
		Sync->Input.History.GetHeldButtons(), Sync->Input.History.GetLatestFrame(), Sync->Input.RequestedBinding, Sync->Input.RequestFrame, Sync->Input.RequestButton);
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
	// Outside the simulation: the authority (a dedicated server, or the listen-server host's own pawn) publishes each event
	// once; a predicting client drops what its predictions and re-simulations wrote
	FGSTN_InputHostUtils::PublishOutbox(EventOutbox, GetOwner());
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

	// A request made on this frame starts its ability here, inside the ability system's tick
	FGSTN_InputHostUtils::ExecuteRequest(AbilitySystem, CommandTable, Sync->Input, Frame);

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
