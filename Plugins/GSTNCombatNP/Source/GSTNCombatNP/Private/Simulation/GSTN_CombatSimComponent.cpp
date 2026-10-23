// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_CombatSimComponent.h"

#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Context/GSTN_InputContextProvider.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "Input/GSTN_DirectionClassifier.h"
#include "Input/GSTN_InputConfig.h"
#include "Input/GSTN_InputScript.h"
#include "Input/GSTN_InputSettings.h"
#include "NetworkPredictionModelDef.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionProxyWrite.h"
#include "NetworkPredictionWorldManager.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "GSTN_CombatTags.h"
#include "Input/GSTN_CombatSettings.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
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

	// The timeline registry: the timeline abilities of the bindings, in binding order, each once (the index the sync state stores)
	TimelineRegistry.Reset();
	BindingTimeline.Reset();
	TimelineTier.Reset();
	TimelineDefaultOverride.Reset();
	const UGSTN_InputSettings& InputSettings = UGSTN_InputSettings::Get();
	for (const FGSTN_CompiledBinding& Binding : CommandTable.Bindings)
	{
		const TSubclassOf<UGSTN_GA_BaseCombat> TimelineClass = Binding.Ability.Get() && Binding.Ability->IsChildOf(UGSTN_GA_BaseCombat::StaticClass())
			? TSubclassOf<UGSTN_GA_BaseCombat>(Binding.Ability.Get()) : nullptr;
		int32 Index = INDEX_NONE;
		if (TimelineClass)
		{
			Index = TimelineRegistry.IndexOfByKey(TimelineClass);
			if (Index == INDEX_NONE)
			{
				Index = TimelineRegistry.Add(TimelineClass);
				const int32 Tier = CommandTable.Commands.IsValidIndex(Binding.CommandIndex) ? InputSettings.GetCommandTier(CommandTable.Commands[Binding.CommandIndex].Tag) : 0;
				TimelineTier.Add(Tier);
				TimelineDefaultOverride.Add(UGSTN_CombatSettings::Get().GetDefaultOverrideFrames(Tier));

				// Soft content the timeline needs in memory before its first frame, loaded here and never in a tick
				UGSTN_GA_BaseCombat* Default = TimelineClass->GetDefaultObject<UGSTN_GA_BaseCombat>();
				Default->PreloadTimelineContent();
			}
		}
		BindingTimeline.Add(static_cast<int16>(Index));
	}
	// The adoptable ones after the bound ones. Their tier is none (combat-m3 6.3: an adopted timeline has the tier of its ability's command, else none)
	for (const TSubclassOf<UGSTN_GA_BaseCombat>& TimelineClass : AdoptableTimelines)
	{
		if (TimelineClass && !TimelineRegistry.Contains(TimelineClass))
		{
			TimelineRegistry.Add(TimelineClass);
			TimelineTier.Add(0);
			TimelineDefaultOverride.Add(0);
			TimelineClass->GetDefaultObject<UGSTN_GA_BaseCombat>()->PreloadTimelineContent();
		}
	}
	// Reserve for the largest timeline, so no frame allocates (the output keeps its capacity)
	for (const TSubclassOf<UGSTN_GA_BaseCombat>& TimelineClass : TimelineRegistry)
	{
		TimelineOutput.Reserve(TimelineClass->GetDefaultObject<UGSTN_GA_BaseCombat>()->GetTimelineData());
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

bool UGSTN_CombatSimComponent::ServerWriteLoadoutRevision(uint16 Revision)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}
	return NetworkPredictionProxy.WriteSyncState<FGSTN_CombatSyncState>([Revision](FGSTN_CombatSyncState& Sync)
	{
		Sync.Core.LoadoutRevision = Revision;
	}, "LoadoutRevision") != nullptr;
}

const FGSTN_CombatTimelineData* UGSTN_CombatSimComponent::GetRegistryTimeline(int32 Index) const
{
	return TimelineRegistry.IsValidIndex(Index) && TimelineRegistry[Index] ? &TimelineRegistry[Index]->GetDefaultObject<UGSTN_GA_BaseCombat>()->GetTimelineData() : nullptr;
}

int32 UGSTN_CombatSimComponent::GetSimulationFrame() const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	return Sync ? Sync->Core.Input.History.GetLatestFrame() : INDEX_NONE;
}

bool UGSTN_CombatSimComponent::IsTimelineRunning(const UClass* AbilityClass, int32& OutStartFrame) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (!Sync || Sync->Core.ActionState != EGSTN_ActionState::InTimeline || !TimelineRegistry.IsValidIndex(Sync->Core.ActiveTimeline))
	{
		return false;
	}
	OutStartFrame = Sync->Core.TimelineStartFrame;
	return TimelineRegistry[Sync->Core.ActiveTimeline] == AbilityClass;
}

bool UGSTN_CombatSimComponent::IsCommandAllowed(int32 CommandIndex) const
{
	// Outside the combat tick (or with no timeline running) nothing gates: only a running timeline closes commands
	if (!GateCore || GateCore->ActionState != EGSTN_ActionState::InTimeline || !CommandTable.Commands.IsValidIndex(CommandIndex))
	{
		return true;
	}
	const FGameplayTag& Command = CommandTable.Commands[CommandIndex].Tag;
	const int32 Running = GateCore->ActiveTimeline;
	return FGSTN_CombatTimelineLogic::IsCommandAllowed(*GateCore, GetRegistryTimeline(Running), TimelineOutput, Command,
		UGSTN_InputSettings::Get().GetCommandTier(Command), TimelineTier.IsValidIndex(Running) ? TimelineTier[Running] : 0);
}

int32 UGSTN_CombatSimComponent::GetCombatClock() const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	const int32 Frame = Sync ? Sync->Core.Input.History.GetLatestFrame() : INDEX_NONE;
	return FGSTN_CombatClock::FromFrame(Frame, Sync ? Sync->Core.PausedFramesTotal : 0);
}

bool UGSTN_CombatSimComponent::IsInTimeline() const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	return Sync && Sync->Core.ActionState == EGSTN_ActionState::InTimeline;
}

FRotator UGSTN_CombatSimComponent::GetAimRotation() const
{
	return FrameAim.bHasAim ? FRotator(FGSTN_DirectionClassifier::DequantizeAim(FrameAim.Pitch), FGSTN_DirectionClassifier::DequantizeAim(FrameAim.Yaw), 0.f) : FRotator::ZeroRotator;
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
	return Sync ? FGSTN_ActivationContext::FromRequest(Sync->Core.Input, CommandTable, Sync->Core.Input.History.GetLatestFrame()) : FGSTN_ActivationContext();
}

bool UGSTN_CombatSimComponent::IsCommandButtonHeld(int32 Button) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	return Sync && FGSTN_InputHostUtils::IsButtonHeld(Sync->Core.Input, Button);
}

void UGSTN_CombatSimComponent::SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<FGSTN_CombatStateTypes>& Input,
	const TNetSimOutput<FGSTN_CombatStateTypes>& Output)
{
	const FGSTN_CombatSyncState& InSync = *Input.Sync;
	FGSTN_CombatSyncState& OutSync = *Output.Sync;
	OutSync = InSync;

	const int32 Frame = TimeStep.Frame;

	// The aim of this frame's command, kept for the ability system's tick that follows (GetAimRotation)
	FrameAim.bHasAim = Input.Cmd->bHasAim;
	FrameAim.Yaw = Input.Cmd->AimYaw;
	FrameAim.Pitch = Input.Cmd->AimPitch;

	// Requests older than this frame are dead (the ability system executed them in their own frame's pre-tick)
	OutSync.Core.Requests.PruneBefore(Frame);

	// Step 2, the timeline clock: the running timeline reaches its end, or its ability is gone, and the combatant is Actionable again
	{
		const int32 Running = OutSync.Core.ActiveTimeline;
		bool bAbilityActive = true;
		if (OutSync.Core.ActionState == EGSTN_ActionState::InTimeline && AbilitySystem && TimelineRegistry.IsValidIndex(Running))
		{
			const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(TimelineRegistry[Running]);
			bAbilityActive = Spec && Spec->IsActive();
		}
		const bool bWasRunning = OutSync.Core.ActionState == EGSTN_ActionState::InTimeline;
		const int32 StartBefore = OutSync.Core.TimelineStartFrame;
		FGSTN_CombatTimelineLogic::AdvanceClock(OutSync.Core, Frame, GetRegistryTimeline(Running),
			TimelineDefaultOverride.IsValidIndex(Running) ? TimelineDefaultOverride[Running] : 0, bAbilityActive, TimelineOutput);
		if (bWasRunning && OutSync.Core.ActionState != EGSTN_ActionState::InTimeline)
		{
			UE_LOG(LogGSTNCombatSim, Verbose, TEXT("%s frame %d: timeline %d (start %d) ended: timeline frame %d of %d, finished %d, ability active %d"), *GetOwner()->GetName(), Frame, Running, StartBefore,
				TimelineOutput.Frame, TimelineOutput.TotalFrames, TimelineOutput.bFinished, bAbilityActive);
		}
	}
	// Step 3, adoption (combat-m3 7.4): timeline abilities that something other than a command activated in the previous frame.
	// The owner's tags are the end-of-previous-frame ones here (the ability system ticks after this model), so the loose tag the
	// waiting abilities hold makes every frame without one a single tag test
	if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(GSTNCombatTags::Ability_AwaitingAdoption))
	{
		TArray<int32, TInlineAllocator<8>> Awaiting;
		for (int32 Index = 0; Index < TimelineRegistry.Num(); ++Index)
		{
			const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(TimelineRegistry[Index]);
			const UGSTN_GA_BaseCombat* Instance = Spec && Spec->IsActive() ? Cast<UGSTN_GA_BaseCombat>(Spec->GetPrimaryInstance()) : nullptr;
			if (Instance && Instance->IsAwaitingAdoption())
			{
				Awaiting.Add(Index);
			}
		}
		const int32 Adopted = FGSTN_CombatTimelineLogic::Adopt(OutSync.Core, Frame, Awaiting);
		if (const FGSTN_CombatTimelineData* Data = GetRegistryTimeline(Adopted))
		{
			FGSTN_CombatTimelineLogic::EvaluateStart(OutSync.Core, Frame, *Data, TimelineDefaultOverride[Adopted], TimelineOutput);
		}
	}

	// The gate the input simulation asks (IsCommandAllowed) reads the state this frame is writing
	GateCore = &OutSync.Core;

	// Extension hooks at the fixed points of the tick. With nothing registered every call is an empty loop
	FGSTN_CombatTickContext HookContext(Frame, InSync, OutSync);
	const FGSTN_CombatTickExtensionRegistry& Extensions = FGSTN_CombatTickExtensionRegistry::Get();
	Extensions.Run(EGSTN_CombatHookPoint::AfterClocks, HookContext);
	Extensions.Run(EGSTN_CombatHookPoint::BeforeCandidates, HookContext);

	FGSTN_InputTickArgs Args;
	Args.Frame = Frame;
	Args.Cmd = Input.Cmd;
	Args.In = &InSync.Core.Input;
	Args.Out = &OutSync.Core.Input;
	Args.Table = &CommandTable;
	Args.Buttons = &ButtonMap;
	Args.Settings = &UGSTN_InputSettings::Get();
	Args.GlobalResetRules = ResetRules;
	Args.PausedStart = InSync.Core.HitstopStartFrame;
	Args.PausedEnd = InSync.Core.HitstopEndFrame;
	Args.Outbox = &EventOutbox;
	const FGSTN_InputTickResult Result = FGSTN_InputSimulator::Tick(Args, *this);
	GateCore = nullptr;

	// Step 8: a command that started a timeline ability replaces the running timeline (if any) and begins its own. The ability
	// itself is activated by the input request of this frame, from the ability system's pre-tick
	if (Result.HasRequest() && BindingTimeline.IsValidIndex(Result.BindingIndex) && BindingTimeline[Result.BindingIndex] == INDEX_NONE)
	{
		// A plain ability (a stance, a jump) that the gate let through ends the running timeline: its turn is over
		FGSTN_CombatTimelineLogic::CancelForOtherCommand(OutSync.Core, Frame);
	}
	else if (Result.HasRequest() && BindingTimeline.IsValidIndex(Result.BindingIndex) && BindingTimeline[Result.BindingIndex] != INDEX_NONE)
	{
		const int32 Entry = BindingTimeline[Result.BindingIndex];
		FGSTN_CombatTimelineLogic::Start(OutSync.Core, Frame, Entry, Input.Cmd->ForcedCommand != 0 ? EGSTN_ActivationSource::Forced : EGSTN_ActivationSource::Command);
		if (const FGSTN_CombatTimelineData* Data = GetRegistryTimeline(Entry))
		{
			FGSTN_CombatTimelineLogic::EvaluateStart(OutSync.Core, Frame, *Data, TimelineDefaultOverride[Entry], TimelineOutput);
		}
	}
	// A timeline that reached its commit is a timeline that started: the event of the one that was replaced inside its startup
	// override window never exists. Written on every machine and every re-simulation, published by the authority only
	if (OutSync.Core.ActionState == EGSTN_ActionState::InTimeline && OutSync.Core.Requests.Find(EGSTN_RequestType::CommitTimeline, Frame))
	{
		FGSTN_SimEvent Event;
		Event.EventType = GSTNCombatTags::Event_TimelineStarted;
		Event.Frame = Frame;
		Event.Instigator = GetOwner();
		Event.Tags.AddTag(OutSync.Core.ActiveSource == EGSTN_ActivationSource::Adopted ? GSTNCombatTags::Source_Adopted
			: OutSync.Core.ActiveSource == EGSTN_ActivationSource::Forced ? GSTNCombatTags::Source_Forced : GSTNCombatTags::Source_Command);
		if (const UGSTN_GA_BaseCombat* Default = TimelineRegistry.IsValidIndex(OutSync.Core.ActiveTimeline) ? TimelineRegistry[OutSync.Core.ActiveTimeline]->GetDefaultObject<UGSTN_GA_BaseCombat>() : nullptr)
		{
			Event.Tags.AppendTags(Default->GetAssetTags());
		}
		EventOutbox.Add(MoveTemp(Event));
	}
	Extensions.Run(EGSTN_CombatHookPoint::AfterExecute, HookContext);

	// Debug hitstop, until hits replace it: a press of the test button opens a window (not while one is already open)
	if (HitstopTestButton >= 0 && !InSync.Core.IsInHitstop(Frame) && (Result.Pressed & (1u << HitstopTestButton)))
	{
		OutSync.Core.HitstopStartFrame = Frame;
		OutSync.Core.HitstopEndFrame = Frame + HitstopTestFrames;
	}

	Extensions.Run(EGSTN_CombatHookPoint::BeforeTagProjection, HookContext);
}

FString UGSTN_CombatSimComponent::GetDebugString(int32 MaxEvents) const
{
	const FGSTN_CombatSyncState* Sync = GetSyncState();
	if (!Sync)
	{
		return TEXT("(no combat state)");
	}
	return FString::Printf(TEXT("%s\nheld 0x%x  frame %d  request %d@%d button %d\naction %d timeline %d@%d+%d source %d"), *FGSTN_InputScript::Describe(Sync->Core.Input.History, MaxEvents),
		Sync->Core.Input.History.GetHeldButtons(), Sync->Core.Input.History.GetLatestFrame(), Sync->Core.Input.RequestedBinding, Sync->Core.Input.RequestFrame, Sync->Core.Input.RequestButton,
		static_cast<int32>(Sync->Core.ActionState), static_cast<int32>(Sync->Core.ActiveTimeline), Sync->Core.TimelineStartFrame, Sync->Core.TimelinePausedFrames, static_cast<int32>(Sync->Core.ActiveSource));
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

	// A timeline replaced or cancelled this frame ends first, so its ability is gone before the replacing one starts
	if (const FGSTN_CombatRequest* Cancel = Sync->Core.Requests.Find(EGSTN_RequestType::CancelTimeline, Frame))
	{
		if (TimelineRegistry.IsValidIndex(Cancel->Arg))
		{
			const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(TimelineRegistry[Cancel->Arg]);
			if (Spec && Spec->IsActive())
			{
				AbilitySystem->CancelAbilityHandle(Spec->Handle);
			}
		}
	}

	// An adoption that another ability won: this one waited in vain and is cancelled before the winner starts
	for (const FGSTN_CombatRequest& Slot : Sync->Core.Requests.Slots)
	{
		if (Slot.Type == EGSTN_RequestType::CancelAbility && Slot.Frame == Frame && TimelineRegistry.IsValidIndex(Slot.Arg))
		{
			const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(TimelineRegistry[Slot.Arg]);
			if (Spec && Spec->IsActive())
			{
				AbilitySystem->CancelAbilityHandle(Spec->Handle);
			}
		}
	}

	// A request made on this frame starts its ability here, inside the ability system's tick
	FGSTN_InputHostUtils::ExecuteRequest(AbilitySystem, CommandTable, Sync->Core.Input, Frame);

	// The cost of a timeline commits when its startup override window ends: the ability is committed from here, so a timeline
	// that was replaced inside the window never paid
	if (const FGSTN_CombatRequest* Commit = Sync->Core.Requests.Find(EGSTN_RequestType::CommitTimeline, Frame))
	{
		if (TimelineRegistry.IsValidIndex(Commit->Arg))
		{
			const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(TimelineRegistry[Commit->Arg]);
			if (UGameplayAbility* Instance = Spec && Spec->IsActive() ? Spec->GetPrimaryInstance() : nullptr)
			{
				Instance->CommitAbility(Spec->Handle, AbilitySystem->AbilityActorInfo.Get(), Instance->GetCurrentActivationInfo());
			}
		}
	}

	// Paused on every hitstop frame with a montage (also one started during the hitstop), resumed on the first frame after.
	// The pause flag only exists with a montage: it is not replicated otherwise
	float MontageTime = 0.f;
	if (Sync->Core.IsInHitstop(Frame) && AbilitySystem->GetPlayingSyncedMontage(MontageTime))
	{
		AbilitySystem->PauseCurrentSyncedMontage();
	}
	else if (Frame == Sync->Core.HitstopEndFrame)
	{
		AbilitySystem->ResumeCurrentSyncedMontage();
	}
}
