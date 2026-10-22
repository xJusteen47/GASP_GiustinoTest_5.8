// Standalone input simulation component (GAS-NPP fork).

#include "Simulation/GSTN_InputSimComponent.h"

#include "AbilitySystemGlobals.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Context/GSTN_InputContextProvider.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "GSTN_InputLog.h"
#include "GameFramework/Actor.h"
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

/**
 * Sorted before the ability system (PreKinematicMovers - 2, itself before Mover), like the combat model: the request of a
 * frame is ready when the ability system ticks that frame. The priority is PreKinematicMovers - 4 and not the combat
 * model's - 3 on purpose: this model and the combat model are never on the same pawn, but Network Prediction checks sort
 * priorities across every registered model and warns when two share one ("ModelDefs ... have same sort priority"). Keep
 * every model of this plugin family on its own number: ASC -2, combat -3, standalone input -4.
 */
class FGSTN_InputModelDef : public FNetworkPredictionModelDef
{
public:
	NP_MODEL_BODY();

	using Simulation = UGSTN_InputSimComponent;
	using StateTypes = FGSTN_InputModelStateTypes;
	using Driver = UGSTN_InputSimComponent;

	static const TCHAR* GetName() { return TEXT("GSTN_Input"); }
	static constexpr int32 GetSortPriority() { return (int32)ENetworkPredictionSortPriority::PreKinematicMovers - 4; }
};

NP_MODEL_REGISTER(FGSTN_InputModelDef);

UGSTN_InputSimComponent::UGSTN_InputSimComponent()
{
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);

	InputSource = CreateDefaultSubobject<UGSTN_PlayerInputSource>(TEXT("InputSource"));
	ContextProvider = CreateDefaultSubobject<UGSTN_MoverInputContextProvider>(TEXT("ContextProvider"));
}

void UGSTN_InputSimComponent::InitializeNetworkPredictionProxy()
{
	NetworkPredictionProxy.Init<FGSTN_InputModelDef>(GetWorld(), GetReplicationProxies(), this, this);
}

bool UGSTN_InputSimComponent::HasConflictingInputComponent() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}
	for (const UActorComponent* Other : Owner->GetComponents())
	{
		if (Other != this && Other->GetClass()->ImplementsInterface(UGSTN_CommandSourceInterface::StaticClass()))
		{
			return true;
		}
	}
	return false;
}

void UGSTN_InputSimComponent::BeginPlay()
{
	Super::BeginPlay();

	if (HasConflictingInputComponent())
	{
		// Both would sample the same buttons and each would start its own abilities. The proxy is already registered (the base
		// class does it when the component initialises), so the component stays in the model and does nothing in it
		UE_LOG(LogGSTNInput, Error, TEXT("%s: %s has another component that runs the input simulation (the combat component?). Use only one: this component is inert."),
			*GetPathName(), *GetNameSafe(GetOwner()));
		bInert = true;
		return;
	}

	CompileInput();
	BindAbilitySystem();

	if (ContextProvider)
	{
		ContextProvider->Initialize(GetOwner());
	}
	else
	{
		UE_LOG(LogGSTNInput, Warning, TEXT("%s: no context provider, the character is taken to face yaw 0 with no movement context"), *GetPathName());
	}
}

void UGSTN_InputSimComponent::CompileInput()
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
			UE_LOG(LogGSTNInput, Error, TEXT("%s: %d buttons, only the first %d are sampled"), *GetPathName(), ActiveInputConfig->Buttons.Num(), UGSTN_InputConfig::MaxButtons);
		}
	}
	else
	{
		UE_LOG(LogGSTNInput, Warning, TEXT("%s: no input config, the input has no buttons"), *GetPathName());
	}

	if (ActiveSlotBindings)
	{
		CommandTable = ActiveSlotBindings->BuildTable(ResetRules);
	}
	else
	{
		UE_LOG(LogGSTNInput, Warning, TEXT("%s: no slot bindings, no command starts an ability"), *GetPathName());
	}

	if (InputSource && !Cast<IGSTN_InputSource>(InputSource.Get()))
	{
		UE_LOG(LogGSTNInput, Error, TEXT("%s: the input source %s does not implement IGSTN_InputSource"), *GetPathName(), *InputSource->GetName());
	}
}

void UGSTN_InputSimComponent::BindAbilitySystem()
{
	AbilitySystem = Cast<UNpAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
	if (!AbilitySystem)
	{
		UE_LOG(LogGSTNInput, Warning, TEXT("%s: no UNpAbilitySystemComponent on the owner, input requests are ignored"), *GetPathName());
		return;
	}

	PreTickHandle = AbilitySystem->OnSimulationPreTick.AddUObject(this, &UGSTN_InputSimComponent::HandleAbilitySimulationPreTick);
}

void UGSTN_InputSimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystem && PreTickHandle.IsValid())
	{
		AbilitySystem->OnSimulationPreTick.Remove(PreTickHandle);
	}
	PreTickHandle.Reset();
	AbilitySystem = nullptr;

	Super::EndPlay(EndPlayReason);
}

const FGSTN_InputModelSyncState* UGSTN_InputSimComponent::GetSyncState() const
{
	return NetworkPredictionProxy.ReadSyncState<FGSTN_InputModelSyncState>();
}

void UGSTN_InputSimComponent::InitializeSimulationState(FGSTN_InputModelSyncState* Sync, FGSTN_InputModelAuxState* Aux)
{
	if (Sync)
	{
		*Sync = FGSTN_InputModelSyncState();
	}
}

void UGSTN_InputSimComponent::ProduceInput(const int32 DeltaTimeMS, FGSTN_InputCmd* Cmd)
{
	*Cmd = FGSTN_InputCmd();
	if (bInert)
	{
		return;
	}

	if (IGSTN_InputSource* Source = Cast<IGSTN_InputSource>(InputSource.Get()))
	{
		FGSTN_InputSourceContext Context;
		Context.Owner = GetOwner();
		Context.Config = ActiveInputConfig;
		Context.Table = &CommandTable;
		Source->ProduceInput(Context, DeltaTimeMS, *Cmd);
	}
}

float UGSTN_InputSimComponent::GetFacingYaw() const
{
	return ContextProvider ? ContextProvider->GetFacingYaw() : 0.f;
}

void UGSTN_InputSimComponent::GetOwnerTags(FGameplayTagContainer& OutTags) const
{
	if (AbilitySystem)
	{
		AbilitySystem->GetOwnedGameplayTags(OutTags);
	}
}

void UGSTN_InputSimComponent::AppendContextTags(FGameplayTagContainer& OutTags) const
{
	if (ContextProvider)
	{
		ContextProvider->AppendContextTags(OutTags);
	}
}

uint8 UGSTN_InputSimComponent::GetContextFingerprint() const
{
	return ContextProvider ? ContextProvider->GetContextFingerprint() : 0;
}

void UGSTN_InputSimComponent::AppendFrameEvents(uint8 PreviousFingerprint, uint8 CurrentFingerprint, FGSTN_BufferFrameEvents& Events) const
{
	if (ContextProvider)
	{
		ContextProvider->AppendFrameEvents(PreviousFingerprint, CurrentFingerprint, Events);
	}
}

bool UGSTN_InputSimComponent::CanExecuteBinding(int32 BindingIndex) const
{
	return FGSTN_InputHostUtils::CanExecuteBinding(AbilitySystem, CommandTable, BindingIndex);
}

FGSTN_ActivationContext UGSTN_InputSimComponent::GetActivationContext() const
{
	const FGSTN_InputModelSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_InputModelSyncState>(ENetworkPredictionStateRead::Simulation);
	return Sync ? FGSTN_ActivationContext::FromRequest(Sync->Input, CommandTable, Sync->Input.History.GetLatestFrame()) : FGSTN_ActivationContext();
}

bool UGSTN_InputSimComponent::IsCommandButtonHeld(int32 Button) const
{
	const FGSTN_InputModelSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_InputModelSyncState>(ENetworkPredictionStateRead::Simulation);
	return Sync && FGSTN_InputHostUtils::IsButtonHeld(Sync->Input, Button);
}

void UGSTN_InputSimComponent::SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<FGSTN_InputModelStateTypes>& Input,
	const TNetSimOutput<FGSTN_InputModelStateTypes>& Output)
{
	const FGSTN_InputModelSyncState& InSync = *Input.Sync;
	FGSTN_InputModelSyncState& OutSync = *Output.Sync;
	OutSync = InSync;

	if (bInert)
	{
		return;
	}

	FGSTN_InputTickArgs Args;
	Args.Frame = TimeStep.Frame;
	Args.Cmd = Input.Cmd;
	Args.In = &InSync.Input;
	Args.Out = &OutSync.Input;
	Args.Table = &CommandTable;
	Args.Buttons = &ButtonMap;
	Args.Settings = &UGSTN_InputSettings::Get();
	Args.GlobalResetRules = ResetRules;
	Args.Outbox = &EventOutbox;
	FGSTN_InputSimulator::Tick(Args, *this);
}

FString UGSTN_InputSimComponent::GetDebugString(int32 MaxEvents) const
{
	const FGSTN_InputModelSyncState* Sync = GetSyncState();
	if (!Sync)
	{
		return TEXT("(no input state)");
	}
	return FString::Printf(TEXT("%s\nheld 0x%x  frame %d  request %d@%d button %d"), *FGSTN_InputScript::Describe(Sync->Input.History, MaxEvents),
		Sync->Input.History.GetHeldButtons(), Sync->Input.History.GetLatestFrame(), Sync->Input.RequestedBinding, Sync->Input.RequestFrame, Sync->Input.RequestButton);
}

void UGSTN_InputSimComponent::FinalizeFrame(const FGSTN_InputModelSyncState* Sync, const FGSTN_InputModelAuxState* Aux)
{
	// Outside the simulation: the authority (a dedicated server, or the listen-server host's own pawn) publishes each event
	// once; a predicting client drops what its predictions and re-simulations wrote
	FGSTN_InputHostUtils::PublishOutbox(EventOutbox, GetOwner());
}

void UGSTN_InputSimComponent::HandleAbilitySimulationPreTick(const FAbilitySystemTimeStep& TimeStep)
{
	if (bInert)
	{
		return;
	}

	// The input model ticked this frame already: its pending state is this frame's output
	const FGSTN_InputModelSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_InputModelSyncState>(ENetworkPredictionStateRead::Simulation);
	if (Sync)
	{
		FGSTN_InputHostUtils::ExecuteRequest(AbilitySystem, CommandTable, Sync->Input, TimeStep.ServerFrame);
	}
}
