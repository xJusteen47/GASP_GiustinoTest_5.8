// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_CombatSimComponent.h"

#include "Loadout/GSTN_LoadoutComponent.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Abilities/GSTN_GA_PlatformJump.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimMontage.h"
#include "AttributeSet.h"
#include "GSTN_CombatNPTags.h"
#include "Movement/GSTN_LayeredMove_Freeze.h"
#include "Movement/GSTN_LayeredMove_Knockback.h"
#include "Movement/GSTN_LayeredMove_Launch.h"
#include "Movement/GSTN_LayeredMove_PushBack.h"
#include "Input/GSTN_InputTags.h"
#include "Context/GSTN_InputContextProvider.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "GSTN_CombatTags.h"
#include "GSTN_CombatRulesComponent.h"
#include "Input/GSTN_CombatSettings.h"
#include "Input/GSTN_DirectionClassifier.h"
#include "Input/GSTN_InputConfig.h"
#include "Input/GSTN_InputScript.h"
#include "Input/GSTN_InputSettings.h"
#include "Movement/GSTN_StatsMovementModifier.h"
#include "MoverComponent.h"
#include "NetworkPredictionModelDef.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionProxyWrite.h"
#include "NetworkPredictionWorldManager.h"
#include "Curves/CurveVector.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "Simulation/GSTN_TimelineTrackHook.h"
#include "Presentation/GSTN_HitPresentationComponent.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "Hits/GSTN_ImpactTypes.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Rules/GSTN_CombatRules.h"
#include "Debug/GSTN_CombatDebugDraw.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "Simulation/GSTN_LoadoutSwapLogic.h"
#include "Loadout/GSTN_EntrySourceMask.h"
#include "Simulation/GSTN_InputHostUtils.h"
#include "Sources/GSTN_InputSource.h"
#include "Timeline/GSTN_WeaponSwitchRules.h"

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
	FreezeEffect = UGSTN_GE_Freeze::StaticClass();
	FreezeAtRestEffect = UGSTN_GE_FreezeZeroVelocity::StaticClass();
	KnockbackEffect = UGSTN_GE_Knockback::StaticClass();
	SurfaceLaunchEffect = UGSTN_GE_SurfaceLaunch::StaticClass();
	PushBackEffect = UGSTN_GE_PushBack::StaticClass();

	InputSource = CreateDefaultSubobject<UGSTN_PlayerInputSource>(TEXT("InputSource"));
	ContextProvider = CreateDefaultSubobject<UGSTN_MoverInputContextProvider>(TEXT("ContextProvider"));
}

void UGSTN_CombatSimComponent::InitializeNetworkPredictionProxy()
{
	// The ability system's sync state lists the attribute sets by position, and machines add them in different orders: one fixed
	// order (by class name) before the simulation starts, the same rule the harness applied by hand
	if (UAbilitySystemComponent* ASC = GetOwner() ? GetOwner()->FindComponentByClass<UAbilitySystemComponent>() : nullptr)
	{
		TArray<UAttributeSet*> Sets = ASC->GetSpawnedAttributes();
		Sets.Sort([](const UAttributeSet& A, const UAttributeSet& B) { return A.GetClass()->GetName() < B.GetClass()->GetName(); });
		ASC->SetSpawnedAttributes(Sets);
	}
	NetworkPredictionProxy.Init<FGSTN_CombatModelDef>(GetWorld(), GetReplicationProxies(), this, this);
}

void UGSTN_CombatSimComponent::BeginPlay()
{
	Super::BeginPlay();

	CompileInput();
	BindAbilitySystem();
	PollLoadoutComponent();

	ActiveHurtboxSet = HurtboxSet.LoadSynchronous();
	if (!CombatRules)
	{
		// The match's rules component (on the GameState, placed by the rules provider or a Blueprint) first; a client whose GameState has not
		// arrived yet, or a world without one, plays the settings' rules, which are what an empty component resolves to as well
		UGSTN_CombatRulesComponent* RulesComponent = UGSTN_CombatRulesComponent::Find(this);
		CombatRules = const_cast<UGSTN_CombatRules*>(RulesComponent != nullptr ? RulesComponent->GetRules() : UGSTN_CombatSettings::Get().ResolveDefaultRules());
	}
	if (CombatRules && !ImpactTable && !CombatRules->SurfaceImpactTable.IsNull())
	{
		ImpactTable = CombatRules->SurfaceImpactTable.LoadSynchronous();
	}
	SetCombatWorldSubsystem(UGSTN_CombatWorldSubsystem::Get(GetWorld()));

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

	// Revision 0: the Blueprint's binding set, which a pawn without a loadout runs on for the whole match. A loadout's table is added as
	// a further set by RegisterTableSet and takes over at its revision boundary (FGSTN_LoadoutSwapLogic)
	EmptyTableSet = NewObject<UGSTN_CombatTableSet>(this);
	UGSTN_CombatTableSet* Blueprint = NewObject<UGSTN_CombatTableSet>(this);
	if (ActiveSlotBindings)
	{
		Blueprint->Table = ActiveSlotBindings->BuildTable(ResetRules);
	}
	else
	{
		UE_LOG(LogGSTNCombatSim, Warning, TEXT("%s: no slot bindings, no command starts an ability"), *GetPathName());
	}
	Blueprint->WeaponStats = WeaponStats;
	// The timeline registry: the timeline abilities of the bindings, in binding order, each once (the index the sync state stores),
	// then the adoptable ones, which have no tier (combat-m3 6.3: an adopted timeline has the tier of its ability's command, else none)
	Blueprint->DeriveFromBindings(UGSTN_InputSettings::Get(), UGSTN_CombatSettings::Get());
	Blueprint->AppendAdoptable(AdoptableTimelines);
	Blueprint->Checksum = Blueprint->ComputeChecksum();
	TableSets.Reset();
	AdoptTableSet(Blueprint);

	if (InputSource && !Cast<IGSTN_InputSource>(InputSource.Get()))
	{
		UE_LOG(LogGSTNCombatSim, Error, TEXT("%s: the input source %s does not implement IGSTN_InputSource"), *GetPathName(), *InputSource->GetName());
	}
}

void UGSTN_CombatSimComponent::AdoptTableSet(UGSTN_CombatTableSet* Set)
{
	TableSets.Add(Set);

	// Reserve for the largest timeline, so no frame allocates (the output keeps its capacity)
	for (const TSubclassOf<UGSTN_GA_BaseCombat>& TimelineClass : Set->TimelineRegistry)
	{
		TimelineOutput.Reserve(TimelineClass->GetDefaultObject<UGSTN_GA_BaseCombat>()->GetTimelineData());
	}

	// The registry, the tiers and the window defaults come from local ini and class default data, and must be the same on every
	// machine (they are not in the sync state). The checksum in the log is what two machines' logs are compared by
	UE_LOG(LogGSTNCombatSim, Log, TEXT("%s: table set revision %d: %d commands, %d bindings, timeline registry of %d entries, checksum %08x"), GetOwner() ? *GetOwner()->GetName() : TEXT("?"),
		Set->Revision, Set->Table.Commands.Num(), Set->Table.Bindings.Num(), Set->TimelineRegistry.Num(), Set->Checksum);
}

void UGSTN_CombatSimComponent::RegisterTableSet(UGSTN_CombatTableSet* Set)
{
	if (!Set)
	{
		return;
	}
	// The same revision again (a re-applied loadout) replaces the old set; the frame pointers are dropped so none can dangle
	TableSets.RemoveAll([Set](const TObjectPtr<UGSTN_CombatTableSet>& Existing) { return Existing && Existing != Set && Existing->Revision != 0 && static_cast<uint16>(Existing->Revision) == static_cast<uint16>(Set->Revision); });
	AdoptTableSet(Set);

	// Revision 0 stays; of the others only the newest MaxRetainedTableRevisions
	const int32 Keep = FMath::Max(UGSTN_CombatSettings::Get().MaxRetainedTableRevisions, 1);
	int32 Others = 0;
	for (int32 Index = TableSets.Num() - 1; Index >= 0; --Index)
	{
		if (TableSets[Index] && TableSets[Index]->Revision != 0 && ++Others > Keep)
		{
			TableSets.RemoveAt(Index);
		}
	}
	FrameTables = nullptr;
	FramePrevTables = nullptr;
}

const UGSTN_CombatTableSet* UGSTN_CombatSimComponent::FindTableSet(int32 Revision) const
{
	for (const TObjectPtr<UGSTN_CombatTableSet>& Set : TableSets)
	{
		if (Set && static_cast<uint16>(Set->Revision) == static_cast<uint16>(Revision))
		{
			return Set;
		}
	}
	return nullptr;
}

const UGSTN_CombatTableSet& UGSTN_CombatSimComponent::Tables() const
{
	if (FrameTables)
	{
		return *FrameTables;
	}
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (const UGSTN_CombatTableSet* Found = Sync ? FindTableSet(Sync->Core.TableRevision) : nullptr)
	{
		return *Found;
	}
	if (const UGSTN_CombatTableSet* Blueprint = FindTableSet(0))
	{
		return *Blueprint;
	}
	check(EmptyTableSet);
	return *EmptyTableSet;
}

const FGSTN_CommandTable& UGSTN_CombatSimComponent::GetCommandTable() const
{
	return Tables().Table;
}

void UGSTN_CombatSimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGSTN_CombatWorldSubsystem* Subsystem = WorldSubsystem.Get())
	{
		if (RegisteredEntityId != INDEX_NONE)
		{
			Subsystem->UnregisterEntity(RegisteredEntityId);
		}
	}
	RegisteredEntityId = INDEX_NONE;
	WorldSubsystem.Reset();

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

void UGSTN_CombatSimComponent::PollLoadoutComponent()
{
	if (BoundLoadout.IsValid() || (LoadoutPollTicks++ % 15) != 0)
	{
		return;
	}
	AActor* Owner = GetOwner();
	UGSTN_LoadoutComponent* Found = Owner ? Owner->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr;
	if (!Found)
	{
		if (const APawn* Pawn = Cast<APawn>(Owner))
		{
			if (const APlayerState* PlayerState = Pawn->GetPlayerState())
			{
				Found = PlayerState->FindComponentByClass<UGSTN_LoadoutComponent>();
			}
		}
	}
	if (Found)
	{
		BindLoadoutComponent(Found);
	}
}

void UGSTN_CombatSimComponent::BindLoadoutComponent(UGSTN_LoadoutComponent* Loadout)
{
	if (!Loadout || BoundLoadout.Get() == Loadout)
	{
		return;
	}
	if (UGSTN_LoadoutComponent* Old = BoundLoadout.Get())
	{
		Old->OnLoadoutApplied.RemoveDynamic(this, &UGSTN_CombatSimComponent::HandleLoadoutApplied);
	}
	BoundLoadout = Loadout;
	Loadout->OnLoadoutApplied.AddUniqueDynamic(this, &UGSTN_CombatSimComponent::HandleLoadoutApplied);

	// A loadout that was applied before this component looked (the client's OnRep ran when the component replicated in)
	if (Loadout->HasLoadout())
	{
		HandleLoadoutApplied(Loadout, Loadout->GetRevision());
	}
}

TArray<FGSTN_WeaponStatsEntry> UGSTN_CombatSimComponent::BuildWeaponStats(const FGSTN_ResolvedLoadout& Resolved)
{
	TArray<FGSTN_WeaponStatsEntry> Entries;
	for (const UGSTN_WeaponDefinition* Weapon : Resolved.Weapons)
	{
		FGSTN_WeaponStatsEntry& Entry = Entries.AddDefaulted_GetRef();
		if (Weapon)
		{
			Entry.Effect = Weapon->StatsEffect.IsNull() ? nullptr : Weapon->StatsEffect.LoadSynchronous();
			Entry.Stats = Weapon->StatModifiers;
		}
	}
	return Entries;
}

void UGSTN_CombatSimComponent::BuildRicochetFallbacks(const FGSTN_ResolvedLoadout& Resolved, TArray<FGSTN_RicochetResponse>& OutWeapons, FGSTN_RicochetResponse& OutKit)
{
	OutWeapons.Reset();
	for (const UGSTN_WeaponDefinition* Weapon : Resolved.Weapons)
	{
		const FGSTN_RicochetResponse* Fallback = Weapon ? Weapon->GetRicochetFallback() : nullptr;
		OutWeapons.Add(Fallback ? *Fallback : FGSTN_RicochetResponse());
	}
	const FGSTN_RicochetResponse* KitFallback = Resolved.Kit ? Resolved.Kit->GetRicochetFallback() : nullptr;
	OutKit = KitFallback ? *KitFallback : FGSTN_RicochetResponse();
}

void UGSTN_CombatSimComponent::HandleLoadoutApplied(UGSTN_LoadoutComponent* Component, int32 Revision)
{
	if (!Component || !Component->HasLoadout())
	{
		return;
	}
	// The victim layer of the surface impacts is the kit's, when it authors one
	KitImpactResponse.Reset();
	if (const UGSTN_PrimaryKitDefinition* Kit = Component->GetResolvedLoadout().Kit)
	{
		if (const FGSTN_ImpactResponseExtension* Extension = Kit->Extensions.Find<FGSTN_ImpactResponseExtension>())
		{
			KitImpactResponse = Extension->Response;
		}
	}

	const FGSTN_CompiledBindingTable& Compiled = Component->GetCompiledTable();
	if (Compiled.Entries.IsEmpty())
	{
		// The loadout did not compile on this machine: no set, so a demand for its revision keeps the gate closed instead of running a wrong table
		UE_LOG(LogGSTNCombatSim, Warning, TEXT("%s: loadout revision %d has no usable table on this machine, the combat simulation stays gated"), *GetOwner()->GetName(), Revision);
		return;
	}
	if (FindTableSet(Compiled.Revision))
	{
		// A revision's loadout never changes; a repeated apply (a pawn that was set again) has nothing new
		return;
	}

	FGSTN_CombatTableConvertInput Input([](const FSoftObjectPath& Path) -> UClass* { return TSoftClassPtr<UGameplayAbility>(Path).LoadSynchronous(); });
	Input.InputSettings = &UGSTN_InputSettings::Get();
	Input.CombatSettings = &UGSTN_CombatSettings::Get();
	Input.GlobalResetRules = ResetRules;
	Input.Adoptable = AdoptableTimelines;
	Input.WeaponStats = BuildWeaponStats(Component->GetResolvedLoadout());
	BuildRicochetFallbacks(Component->GetResolvedLoadout(), Input.WeaponRicochet, Input.KitRicochet);
	Input.ReservedExtension = Component->GetApplied().ReservedExtension;

	FGSTN_CompileReport Report;
	UGSTN_CombatTableSet* Set = FGSTN_CombatTableConverter::Convert(this, Compiled, Input, &Report);
	for (const FText& Error : Report.Errors)
	{
		UE_LOG(LogGSTNCombatSim, Warning, TEXT("%s: loadout revision %d: %s"), *GetOwner()->GetName(), Revision, *Error.ToString());
	}
	RegisterTableSet(Set);
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

bool UGSTN_CombatSimComponent::ServerApplyLoadoutChange(UGSTN_LoadoutComponent* Loadout, const FGSTN_LoadoutData& Requested, TArray<FString>& OutErrors)
{
	OutErrors.Reset();
	if (!Loadout || !GetOwner() || !GetOwner()->HasAuthority())
	{
		OutErrors.Add(TEXT("A loadout change is applied on the server only, on a loadout component."));
		return false;
	}
	// The table set must be registered by the time the loadout applies (AcceptLoadout broadcasts), on this machine as on the clients
	BindLoadoutComponent(Loadout);

	// A refused loadout changes nothing, not even the move in progress
	if (!Loadout->ValidateLoadout(Requested, OutErrors))
	{
		return false;
	}

	const bool bChanges = !Loadout->HasLoadout() || Requested != Loadout->GetApplied();
	const int32 NowFrame = GetSimulationFrame();
	if (bChanges && LastLoadoutChangeFrame != INDEX_NONE && NowFrame != INDEX_NONE && NowFrame - LastLoadoutChangeFrame < UGSTN_CombatSettings::Get().MinFramesBetweenLoadoutChanges)
	{
		// Two table sets must not expire inside the rollback window, and the first swap must be settled before the second starts
		OutErrors.Add(FString::Printf(TEXT("A loadout change within %d frames of the previous one is refused (%d frames ago)."), UGSTN_CombatSettings::Get().MinFramesBetweenLoadoutChanges, NowFrame - LastLoadoutChangeFrame));
		return false;
	}

	if (bChanges)
	{
		// The running timeline belongs to the old kit: its ability is cancelled and the action state returns to actionable
		const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
		const int32 Running = Sync && Sync->Core.ActionState == EGSTN_ActionState::InTimeline ? static_cast<int32>(Sync->Core.ActiveTimeline) : INDEX_NONE;
		const UGSTN_CombatTableSet& Current = Tables();
		if (AbilitySystem && Current.TimelineRegistry.IsValidIndex(Running))
		{
			if (const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(Current.TimelineRegistry[Running]))
			{
				AbilitySystem->CancelAbilityHandle(Spec->Handle);
			}
		}
		NetworkPredictionProxy.WriteSyncState<FGSTN_CombatSyncState>([](FGSTN_CombatSyncState& State)
		{
			FGSTN_CombatTimelineLogic::ClearTimeline(State.Core);
		}, "LoadoutChangeClearTimeline");
	}

	if (!Loadout->AcceptLoadout(Requested, OutErrors))
	{
		return false;
	}
	if (bChanges)
	{
		LastLoadoutChangeFrame = NowFrame;
	}
	return ServerWriteLoadoutRevision(static_cast<uint16>(Loadout->GetRevision()));
}

const FGSTN_CombatTimelineData* UGSTN_CombatSimComponent::GetRegistryTimeline(const UGSTN_CombatTableSet& Set, int32 Index)
{
	return Set.TimelineRegistry.IsValidIndex(Index) && Set.TimelineRegistry[Index] ? &Set.TimelineRegistry[Index]->GetDefaultObject<UGSTN_GA_BaseCombat>()->GetTimelineData() : nullptr;
}

int32 UGSTN_CombatSimComponent::GetSimulationFrame() const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	return Sync ? Sync->Core.Input.History.GetLatestFrame() : INDEX_NONE;
}

bool UGSTN_CombatSimComponent::IsTimelineRunning(const UClass* AbilityClass, int32& OutStartFrame) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	const UGSTN_CombatTableSet& Current = Tables();
	if (!Sync || Sync->Core.ActionState != EGSTN_ActionState::InTimeline || !Current.TimelineRegistry.IsValidIndex(Sync->Core.ActiveTimeline))
	{
		return false;
	}
	OutStartFrame = Sync->Core.TimelineStartFrame;
	return Current.TimelineRegistry[Sync->Core.ActiveTimeline] == AbilityClass;
}

bool UGSTN_CombatSimComponent::GetRecoil(int32& OutStartFrame, const FGSTN_RicochetResponse*& OutResponse) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (!Sync || Sync->Core.ActionState != EGSTN_ActionState::InTimeline || Sync->Core.RecoilStartFrame == INDEX_NONE)
	{
		return false;
	}
	const UGSTN_CombatTableSet& Current = Tables();
	OutResponse = FGSTN_CombatTimelineLogic::ResolveRicochet(GetRegistryTimeline(Current, Sync->Core.ActiveTimeline), Current.GetRicochetFallback(Sync->Core.ActiveWeaponIndex));
	OutStartFrame = Sync->Core.RecoilStartFrame;
	return OutResponse != nullptr;
}

void UGSTN_CombatSimComponent::ExecuteRecoilMove(const FGSTN_TimelineTrack_Movement& Track, int32 RecoilFrames, float StepMs)
{
	// A push back, the slide of the victim side reused (S 3b: "optional push-back movement track"): the curve's value at its start is the
	// initial velocity in the local space of the pose the previous frame ended on, and it falls linearly to zero over the track. The
	// curve is loaded with the response (never here): a curve that is not in memory is no movement
	const UCurveVector* Curve = Track.VelocityCurve.Get();
	FTransform Pose;
	FVector Up;
	if (!AbilitySystem || !PushBackEffect || !Curve || !ReadOwnPose(Pose, Up))
	{
		return;
	}
	const int32 Frames = FMath::Clamp(Track.EndFrame - Track.StartFrame, 1, FMath::Max(RecoilFrames - Track.StartFrame, 1));
	const FVector Velocity = Pose.TransformVectorNoScale(Curve->GetVectorValue(0.f));
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(PushBackEffect, 1.f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, Velocity.X);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, Velocity.Y);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, Velocity.Z);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveDecay, 1.f / Frames);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveGravityScale, Track.GravityScale);
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveSeconds, Frames * StepMs * 0.001f);
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

bool UGSTN_CombatSimComponent::IsCommandAllowed(int32 CommandIndex) const
{
	// Outside the combat tick (or with no timeline running) nothing gates: only a running timeline closes commands
	// A victim in hitstun or tumble acts again when its state says Actionable; the first frame of that state is the first frame of the tail
	const UGSTN_CombatTableSet& Current = Tables();
	if (bFrameActionsBlocked)
	{
		// An extension's state forbids acting (BlocksActions): no command starts, whatever the reaction or the timeline would allow
		return false;
	}
	if (FrameAdmitTag.IsValid())
	{
		// An extension's state admits only the bindings made for it (a downed entity's get-ups): a command without one does not start
		const FGSTN_CommandTable::FCommand* Admitting = Current.Table.Commands.IsValidIndex(CommandIndex) ? &Current.Table.Commands[CommandIndex] : nullptr;
		bool bAdmitted = false;
		for (int32 Offset = 0; Admitting && Offset < Admitting->NumBindings && !bAdmitted; ++Offset)
		{
			bAdmitted = IsBindingAdmitted(Current.Table, Admitting->FirstBinding + Offset);
		}
		UE_LOG(LogGSTNCombatSim, Verbose, TEXT("%s: command %d (%s) %s while only bindings requiring %s are admitted"), *GetNameSafe(GetOwner()), CommandIndex,
			Admitting ? *Admitting->Tag.ToString() : TEXT("?"), bAdmitted ? TEXT("admitted") : TEXT("refused"), *FrameAdmitTag.ToString());
		if (!bAdmitted)
		{
			return false;
		}
	}
	if (GateCore && FGSTN_CombatVictim::IsActionLocked(*GateCore))
	{
		// The one exception: an ability tagged Combat.Ability.IgnoreHitstunOnActivation, in a reaction (a KO is never bypassed). The gate only asks
		// whether the command has such a binding; CanExecuteBinding checks the binding the context actually picks, and the ability's own blocked tags decide
		if (!FGSTN_CombatVictim::IsInReaction(*GateCore) || !Current.Table.Commands.IsValidIndex(CommandIndex))
		{
			return false;
		}
		const FGSTN_CommandTable::FCommand& Locked = Current.Table.Commands[CommandIndex];
		for (int32 Offset = 0; Offset < Locked.NumBindings; ++Offset)
		{
			const int32 Binding = Locked.FirstBinding + Offset;
			if (Current.BindingReaction.IsValidIndex(Binding) && (Current.BindingReaction[Binding] & UGSTN_CombatTableSet::ReactionIgnoresHitstun))
			{
				return true;
			}
		}
		return false;
	}
	if (!GateCore || GateCore->ActionState != EGSTN_ActionState::InTimeline || !Current.Table.Commands.IsValidIndex(CommandIndex))
	{
		return true;
	}
	const FGameplayTag& Command = Current.Table.Commands[CommandIndex].Tag;
	const int32 Running = GateCore->ActiveTimeline;
	return FGSTN_CombatTimelineLogic::IsCommandAllowed(*GateCore, GetRegistryTimeline(Current, Running), TimelineOutput, Command,
		UGSTN_InputSettings::Get().GetCommandTier(Command), Current.TimelineTier.IsValidIndex(Running) ? Current.TimelineTier[Running] : 0, FrameCancelConditions);
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
	// DequantizeAim gives [0, 360); a rotation is read as [-180, 180]
	return FrameAim.bHasAim ? FRotator(FRotator::NormalizeAxis(FGSTN_DirectionClassifier::DequantizeAim(FrameAim.Pitch)),
		FRotator::NormalizeAxis(FGSTN_DirectionClassifier::DequantizeAim(FrameAim.Yaw)), 0.f) : FRotator::ZeroRotator;
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
		Context.Table = &GetCommandTable();
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
	if (bFrameInterrupted)
	{
		Events.EventTags.AddTag(GSTNInputTags::BufferEvent_Interrupted);
	}
	if (ContextProvider)
	{
		ContextProvider->AppendFrameEvents(PreviousFingerprint, CurrentFingerprint, Events);
	}
}

bool UGSTN_CombatSimComponent::CanExecuteBinding(int32 BindingIndex) const
{
	if (!IsJumpBindingAllowed(BindingIndex) || !IsBindingAdmitted(Tables().Table, BindingIndex))
	{
		UE_LOG(LogGSTNCombatSim, Verbose, TEXT("%s: binding %d refused (jump gate %d, admitted %d)"), *GetNameSafe(GetOwner()), BindingIndex, IsJumpBindingAllowed(BindingIndex) ? 1 : 0,
			IsBindingAdmitted(Tables().Table, BindingIndex) ? 1 : 0);
		return false;
	}
	// In a hit reaction only an ability that says so may start (the gate of IsCommandAllowed passed the command, not necessarily this binding)
	if (GateCore && FGSTN_CombatVictim::IsActionLocked(*GateCore))
	{
		const UGSTN_CombatTableSet& Current = Tables();
		if (!FGSTN_CombatVictim::IsInReaction(*GateCore) || !Current.BindingReaction.IsValidIndex(BindingIndex)
			|| !(Current.BindingReaction[BindingIndex] & UGSTN_CombatTableSet::ReactionIgnoresHitstun))
		{
			return false;
		}
	}
	// On the end-of-previous-frame state, like the ability system's own check at execution
	const bool bCanExecute = FGSTN_InputHostUtils::CanExecuteBinding(AbilitySystem, GetCommandTable(), BindingIndex);
	UE_CLOG(!bCanExecute && FrameAdmitTag.IsValid(), LogGSTNCombatSim, Verbose, TEXT("%s: binding %d cannot execute (its ability refuses)"), *GetNameSafe(GetOwner()), BindingIndex);
	return bCanExecute;
}

FGSTN_ActivationContext UGSTN_CombatSimComponent::GetActivationContext() const
{
	// The simulation read: while the ability system ticks frame F this is the combat state F produced, request included
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	return Sync ? FGSTN_ActivationContext::FromRequest(Sync->Core.Input, GetCommandTable(), Sync->Core.Input.History.GetLatestFrame()) : FGSTN_ActivationContext();
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

	// The step snapshot is built by the first combat tick of the frame, before this or any other entity writes its state of the frame: every
	// read of it is the end of the previous frame, whoever ticks first (combat-m4-hits 5)
	UGSTN_CombatWorldSubsystem* World = WorldSubsystem.Get();
	if (World && CombatRules && CombatRules->bUseLagCompensation && GetOwner() && GetOwner()->HasAuthority())
	{
		World->RequestHistory(CombatRules->MaxRewindMs);
	}
	const FGSTN_StepSnapshot* StepSnapshot = World ? &World->EnsureSnapshot(Frame, static_cast<float>(TimeStep.TotalSimulationTime), static_cast<float>(TimeStep.StepMS)) : nullptr;

	// The aim of this frame's command, kept for the ability system's tick that follows (GetAimRotation)
	FrameAim.bHasAim = Input.Cmd->bHasAim;
	FrameAim.Yaw = Input.Cmd->AimYaw;
	FrameAim.Pitch = Input.Cmd->AimPitch;

	// Requests older than this frame are dead (the ability system executed them in their own frame's pre-tick)
	OutSync.Core.Requests.PruneBefore(Frame);

	// The victim side (combat-m4-hits 7): the hits the attackers wrote on the previous frame are resolved before anything else of this frame, so
	// the hitstop window they open already stands this frame (its first frame is the frame after the hit, like the attacker's), and a cancelled
	// move or a hitstun state is what the rest of the tick sees
	RunVictimStep(TimeStep, InSync, OutSync, StepSnapshot, *Input.Cmd);

	// The loadout table swap (loadout-table-swap-design 2c). The active table is the set of the demanded revision; the frame that first has
	// it is the boundary on every machine, and every later step of the frame (clock, adoption, jump, input, step 8) reads that one table.
	// A demand whose set is not here yet (the loadout has not reached this machine) closes the gate: the clocks keep running on the table
	// the state is on, the input history keeps recording, and no command resolves
	const UGSTN_CombatTableSet* DemandedTables = FindTableSet(InSync.Core.LoadoutRevision);
	const UGSTN_CombatTableSet* PreviousTables = FindTableSet(InSync.Core.TableRevision);
	const EGSTN_LoadoutSwapResult SwapResult = FGSTN_LoadoutSwapLogic::Step(OutSync.Core, DemandedTables != nullptr, Frame);
	bFrameGateClosed = SwapResult == EGSTN_LoadoutSwapResult::Closed;
	FrameTables = bFrameGateClosed ? PreviousTables : DemandedTables;
	if (!FrameTables)
	{
		FrameTables = EmptyTableSet;
	}
	FramePrevTables = PreviousTables ? PreviousTables : FrameTables;
	const UGSTN_CombatTableSet& FrameSet = *FrameTables;
	if (bFrameGateClosed)
	{
		++GateClosedFrames;
	}
	if (SwapResult == EGSTN_LoadoutSwapResult::Swapped)
	{
		// Written on every machine and every re-simulation, published by the authority only
		FGSTN_SimEvent Event;
		Event.EventType = GSTNCombatTags::Event_LoadoutSwapped;
		Event.Frame = Frame;
		Event.Instigator = GetOwner();
		EventOutbox.Add(MoveTemp(Event));
	}

	// A forced command is a table index with no revision: one made for the frame of the boundary is dropped (the harness and AI force
	// commands; they see the new table from the next frame)
	FGSTN_InputCmd FrameCmd = *Input.Cmd;
	if (SwapResult == EGSTN_LoadoutSwapResult::Swapped)
	{
		FrameCmd.ForcedCommand = 0;
	}

	// Hitstop (combat-m3 7.1): a frame inside the window of the previous state stands still on the combat clock. The window the
	// debug button opens on frame F pauses from F+1 (F itself was simulated with the clock running when it opened)
	const bool bHitstopFrame = OutSync.Core.AdvanceHitstopClock(Frame);

	// What the track hooks of the plugins may read besides the input: the button map and the fragments as this frame writes them (M6 F7)
	FGSTN_TrackHookEnvironment HookEnvironment;
	HookEnvironment.Buttons = &ButtonMap;
	HookEnvironment.Fragments = &OutSync.Fragments;
	// The attacker side of hits (M4 H8) is not run on a hitstop frame either: it reads the same bHitstopFrame

	// Step 2, the timeline clock: the running timeline reaches its end, or its ability is gone, and the combatant is Actionable again
	{
		const int32 Running = OutSync.Core.ActiveTimeline;
		bool bAbilityActive = true;
		if (OutSync.Core.ActionState == EGSTN_ActionState::InTimeline && AbilitySystem && FrameSet.TimelineRegistry.IsValidIndex(Running))
		{
			const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(FrameSet.TimelineRegistry[Running]);
			bAbilityActive = Spec && Spec->IsActive();
		}
		const bool bWasRunning = OutSync.Core.ActionState == EGSTN_ActionState::InTimeline;
		const int32 StartBefore = OutSync.Core.TimelineStartFrame;
		FGSTN_CombatTimelineLogic::AdvanceClock(OutSync.Core, Frame, GetRegistryTimeline(FrameSet, Running),
			FrameSet.TimelineDefaultOverride.IsValidIndex(Running) ? FrameSet.TimelineDefaultOverride[Running] : 0, bAbilityActive, TimelineOutput, bHitstopFrame,
			FrameSet.GetRicochetFallback(OutSync.Core.ActiveWeaponIndex), &HookEnvironment);
		if (bWasRunning && OutSync.Core.ActionState != EGSTN_ActionState::InTimeline)
		{
			UE_LOG(LogGSTNCombatSim, Verbose, TEXT("%s frame %d: timeline %d (start %d) ended: timeline frame %d of %d, finished %d, ability active %d"), *GetOwner()->GetName(), Frame, Running, StartBefore,
				TimelineOutput.Frame, TimelineOutput.TotalFrames, TimelineOutput.bFinished, bAbilityActive);
		}
	}
	// Step 3, adoption (combat-m3 7.4): timeline abilities that something other than a command activated in the previous frame.
	// The owner's tags are the end-of-previous-frame ones here (the ability system ticks after this model), so the loose tag the
	// waiting abilities hold makes every frame without one a single tag test
	if (!bFrameGateClosed && AbilitySystem && AbilitySystem->HasMatchingGameplayTag(GSTNCombatTags::Ability_AwaitingAdoption))
	{
		TArray<int32, TInlineAllocator<8>> Awaiting;
		for (int32 Index = 0; Index < FrameSet.TimelineRegistry.Num(); ++Index)
		{
			const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(FrameSet.TimelineRegistry[Index]);
			const UGSTN_GA_BaseCombat* Instance = Spec && Spec->IsActive() ? Cast<UGSTN_GA_BaseCombat>(Spec->GetPrimaryInstance()) : nullptr;
			if (Instance && Instance->IsAwaitingAdoption())
			{
				Awaiting.Add(Index);
			}
		}
		const int32 Adopted = FGSTN_CombatTimelineLogic::Adopt(OutSync.Core, Frame, Awaiting);
		if (const FGSTN_CombatTimelineData* Data = GetRegistryTimeline(FrameSet, Adopted))
		{
			FGSTN_CombatTimelineLogic::EvaluateStart(OutSync.Core, Frame, *Data, FrameSet.TimelineDefaultOverride[Adopted], TimelineOutput, &HookEnvironment);
			StampMoveOrigin(OutSync.Core, Frame);
		}
	}

	// The gate the input simulation asks (IsCommandAllowed) reads the state this frame is writing
	GateCore = &OutSync.Core;

	// Extension hooks at the fixed points of the tick. With nothing registered every call is an empty loop
	FGSTN_CombatTickContext HookContext(Frame, InSync, OutSync);
	{
		FTransform Pose;
		FVector Up = FVector::UpVector;
		ReadOwnPose(Pose, Up);
		FillHookContext(HookContext, FrameCmd, Up);
	}
	HookContext.Timeline = &TimelineOutput;
	// What an extension may ask the model to do in this tick (StartForcedTimeline): this frame's state, tables and hook environment
	ServiceSync = &OutSync;
	ServiceFrame = Frame;
	ServiceHookEnvironment = &HookEnvironment;
	HookContext.Services = this;
	const FGSTN_CombatTickExtensionRegistry& Extensions = FGSTN_CombatTickExtensionRegistry::Get();
	Extensions.Run(EGSTN_CombatHookPoint::AfterClocks, HookContext);
	Extensions.Run(EGSTN_CombatHookPoint::BeforeCandidates, HookContext);
	// The can-act veto of the extensions, from the fragments as the hooks above left them; it closes the gate the candidates ask
	bFrameActionsBlocked = Extensions.BlocksActions(HookContext);
	FrameCancelConditions = Extensions.HeldCancelConditions(HookContext);
	FrameAdmitTag = Extensions.AdmitsOnlyBindingsRequiring(HookContext);

	// The attacker side (combat-m4-hits 6), before the candidates: a hit of this frame confirms the run for the cancel gate of the same frame
	if (!bHitstopFrame && !bFrameGateClosed)
	{
		RunAttackerStep(TimeStep, OutSync.Core, StepSnapshot, HookContext.HitGroupMask);
	}

	// Step 5a, the jump bookkeeping: before the input tick, because the gate asks whether a jump or an air jump is available this frame
	StepJump(Frame, Input.Cmd->HeldButtons, OutSync.Core);

	FGSTN_InputTickArgs Args;
	Args.Frame = Frame;
	Args.Cmd = &FrameCmd;
	Args.In = &InSync.Core.Input;
	Args.Out = &OutSync.Core.Input;
	// A closed gate resolves against no commands; the sample is appended and aged all the same
	Args.Table = bFrameGateClosed ? &EmptyTableSet->Table : &FrameSet.Table;
	Args.ActiveSourceMask = GSTNEntrySourceMask::ActiveMask(InSync.Core.ActiveWeaponIndex);
	Args.Buttons = &ButtonMap;
	Args.Settings = &UGSTN_InputSettings::Get();
	Args.GlobalResetRules = ResetRules;
	Args.PausedStart = InSync.Core.HitstopStartFrame;
	Args.PausedEnd = InSync.Core.HitstopEndFrame;
	Args.Outbox = &EventOutbox;
	const FGSTN_InputTickResult Result = FGSTN_InputSimulator::Tick(Args, *this);
	GateCore = nullptr;
	bFrameActionsBlocked = false;
	FrameCancelConditions = 0;
	FrameAdmitTag = FGameplayTag();

	// A burst: the ability that starts in a reaction and says it ends it. The state is rewritten here, in the frame of the request, so it rolls
	// back with the rest; the ability system's pre-tick removes the freeze of the window (EndReaction request)
	if (Result.HasRequest() && FGSTN_CombatVictim::IsInReaction(OutSync.Core) && FrameSet.BindingReaction.IsValidIndex(Result.BindingIndex)
		&& (FrameSet.BindingReaction[Result.BindingIndex] & UGSTN_CombatTableSet::ReactionEndsReaction))
	{
		FGSTN_CombatVictim::EndReaction(OutSync.Core, Frame);
		OutSync.Core.Requests.Add(EGSTN_RequestType::EndReaction, Frame, 0);
	}

	// Step 5b: a jump or fast fall command that the gate let through changes the jump state in the frame of the request
	if (Result.HasRequest())
	{
		ApplyJumpRequest(Frame, Result.BindingIndex, OutSync.Core);
	}

	// Step 8: a command that started a timeline ability replaces the running timeline (if any) and begins its own. The ability
	// itself is activated by the input request of this frame, from the ability system's pre-tick
	if (Result.HasRequest() && FrameSet.BindingTimeline.IsValidIndex(Result.BindingIndex) && FrameSet.BindingTimeline[Result.BindingIndex] == INDEX_NONE)
	{
		// A plain ability (a stance, a jump) that the gate let through ends the running timeline: its turn is over
		FGSTN_CombatTimelineLogic::CancelForOtherCommand(OutSync.Core, Frame);
	}
	else if (Result.HasRequest() && FrameSet.BindingTimeline.IsValidIndex(Result.BindingIndex) && FrameSet.BindingTimeline[Result.BindingIndex] != INDEX_NONE)
	{
		const int32 Entry = FrameSet.BindingTimeline[Result.BindingIndex];
		FGSTN_CombatTimelineLogic::Start(OutSync.Core, Frame, Entry, FrameCmd.ForcedCommand != 0 ? EGSTN_ActivationSource::Forced : EGSTN_ActivationSource::Command);
		StampMoveOrigin(OutSync.Core, Frame);
		if (const FGSTN_CombatTimelineData* Data = GetRegistryTimeline(FrameSet, Entry))
		{
			FGSTN_CombatTimelineLogic::EvaluateStart(OutSync.Core, Frame, *Data, FrameSet.TimelineDefaultOverride[Entry], TimelineOutput, &HookEnvironment);
		}
	}
	// A timeline that reached its commit is a timeline that started: the event of the one that was replaced inside its startup
	// override window never exists. Written on every machine and every re-simulation, published by the authority only
	if (OutSync.Core.ActionState == EGSTN_ActionState::InTimeline && OutSync.Core.Requests.Find(EGSTN_RequestType::CommitTimeline, Frame, OutSync.Core.ActiveTimeline))
	{
		FGSTN_SimEvent Event;
		Event.EventType = GSTNCombatTags::Event_TimelineStarted;
		Event.Frame = Frame;
		Event.Instigator = GetOwner();
		Event.Tags.AddTag(OutSync.Core.ActiveSource == EGSTN_ActivationSource::Adopted ? GSTNCombatTags::Source_Adopted
			: OutSync.Core.ActiveSource == EGSTN_ActivationSource::Forced ? GSTNCombatTags::Source_Forced : GSTNCombatTags::Source_Command);
		if (const UGSTN_GA_BaseCombat* Default = FrameSet.TimelineRegistry.IsValidIndex(OutSync.Core.ActiveTimeline) ? FrameSet.TimelineRegistry[OutSync.Core.ActiveTimeline]->GetDefaultObject<UGSTN_GA_BaseCombat>() : nullptr)
		{
			Event.Tags.AppendTags(Default->GetAssetTags());
		}
		EventOutbox.Add(MoveTemp(Event));
	}
	// Step 4, the weapon switch track (combat-m3 7.2): the swap frame of the running timeline was crossed by this frame's evaluation.
	// The new index goes into the state, the old one into the request, and the ability system's pre-tick swaps the stats effects. A
	// timeline that ended or was replaced this frame left no valid evaluation behind
	if (OutSync.Core.ActionState == EGSTN_ActionState::InTimeline)
	{
		for (const FGSTN_TimelineTrack_WeaponSwitch* Switch : TimelineOutput.WeaponSwitches)
		{
			const int32 Old = OutSync.Core.ActiveWeaponIndex;
			const int32 New = FGSTN_WeaponSwitchRules::Resolve(Switch->Target, Switch->Index, Old, FrameSet.WeaponStats.Num());
			if (New == Old)
			{
				continue;
			}
			OutSync.Core.ActiveWeaponIndex = static_cast<uint8>(New);
			if (!OutSync.Core.Requests.Add(EGSTN_RequestType::SwapWeaponStats, Frame, static_cast<int16>(Old)))
			{
				UE_LOG(LogGSTNCombatSim, Error, TEXT("%s frame %d: no room for the weapon swap request"), *GetOwner()->GetName(), Frame);
			}
			FGSTN_SimEvent Event;
			Event.EventType = GSTNCombatTags::Event_WeaponSwitched;
			Event.Frame = Frame;
			Event.Instigator = GetOwner();
			EventOutbox.Add(MoveTemp(Event));
		}
	}
	Extensions.Run(EGSTN_CombatHookPoint::AfterExecute, HookContext);

	// Debug hitstop, until hits replace it: a press of the test button opens a window (not while one is already open)
	if (HitstopTestButton >= 0 && !InSync.Core.IsInHitstop(Frame) && (Result.Pressed & (1u << HitstopTestButton)))
	{
		OutSync.Core.HitstopStartFrame = Frame;
		OutSync.Core.HitstopEndFrame = Frame + HitstopTestFrames;
		// The Mover half of the window: the freeze runs for as many frames as the window, from this frame (the ability system's pre-tick executes it)
		OutSync.Core.Requests.Add(EGSTN_RequestType::Freeze, Frame, static_cast<int16>(HitstopTestFrames));
		if (HitstopTestMontage && AbilitySystem)
		{
			AbilitySystem->PlaySyncedMontage(HitstopTestMontage, 0.f, 1.f, NAME_None, 0.f);
		}
	}

	Extensions.Run(EGSTN_CombatHookPoint::BeforeTagProjection, HookContext);
	ServiceSync = nullptr;
	ServiceFrame = INDEX_NONE;
	ServiceHookEnvironment = nullptr;
}

void UGSTN_CombatSimComponent::FillHookContext(FGSTN_CombatTickContext& Context, const FGSTN_InputCmd& Cmd, const FVector& Up)
{
	Context.Owner = GetOwner();
	Context.AbilitySystem = AbilitySystem;
	Context.Outbox = &EventOutbox;
	Context.Up = Up;
	Context.Stick = FVector2D(FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.StickX), FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.StickY));
	Context.StickOnPlane = FGSTN_CombatVictim::StickToPlane(Context.Stick, FGSTN_DirectionClassifier::DequantizeYaw(Cmd.CameraYaw), Up);
	Context.Buttons = &ButtonMap;
}

bool UGSTN_CombatSimComponent::StartForcedTimeline(const UClass* AbilityClass)
{
	if (!ServiceSync || !FrameTables || !AbilityClass)
	{
		return false;
	}
	const UGSTN_CombatTableSet& Set = *FrameTables;
	const int32 Entry = Set.TimelineRegistry.IndexOfByPredicate([AbilityClass](const TSubclassOf<UGSTN_GA_BaseCombat>& Class) { return Class.Get() == AbilityClass; });
	if (Entry == INDEX_NONE)
	{
		return false;
	}
	// What step 8 does for a command, source Forced; the ability system's pre-tick of the frame activates the ability (ActivateEntry), which finds its
	// timeline running from this frame and begins it
	FGSTN_CombatSyncCore& Core = ServiceSync->Core;
	FGSTN_CombatTimelineLogic::Start(Core, ServiceFrame, Entry, EGSTN_ActivationSource::Forced);
	StampMoveOrigin(Core, ServiceFrame);
	if (const FGSTN_CombatTimelineData* Data = GetRegistryTimeline(Set, Entry))
	{
		FGSTN_CombatTimelineLogic::EvaluateStart(Core, ServiceFrame, *Data, Set.TimelineDefaultOverride.IsValidIndex(Entry) ? Set.TimelineDefaultOverride[Entry] : 0, TimelineOutput, ServiceHookEnvironment);
	}
	Core.Requests.Add(EGSTN_RequestType::ActivateEntry, ServiceFrame, static_cast<int16>(Entry));
	return true;
}

bool UGSTN_CombatSimComponent::IsBindingAdmitted(const FGSTN_CommandTable& Table, int32 BindingIndex) const
{
	if (!FrameAdmitTag.IsValid())
	{
		return true;
	}
	return Table.Bindings.IsValidIndex(BindingIndex) && Table.Bindings[BindingIndex].Requirements.RequireTags.HasTagExact(FrameAdmitTag);
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

	if (bQueueStatsModifier && !bStatsModifierQueued)
	{
		TryQueueStatsModifier();
	}
	PollLoadoutComponent();
	TryRegisterWithWorld();

#if ENABLE_DRAW_DEBUG
	if (CVarCombatDebugInput.GetValueOnGameThread() != 0)
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		if (Pawn && Pawn->IsLocallyControlled())
		{
			DrawDebugString(GetWorld(), Pawn->GetActorLocation() + FVector(0.0, 0.0, 140.0), GetDebugString(), nullptr, FColor::White, 0.f, true);
		}
	}
	if (GSTNCombatDebug::IsHitboxDrawOn())
	{
		GSTNCombatDebug::DrawHitboxes(GetWorld(), AttackerDebugShapes);
	}
	if (GSTNCombatDebug::IsHurtboxDrawOn() && RegisteredEntityId != INDEX_NONE)
	{
		if (const UGSTN_CombatWorldSubsystem* Subsystem = WorldSubsystem.Get())
		{
			if (const FGSTN_SnapshotEntry* Entry = Subsystem->GetSnapshot().Find(RegisteredEntityId))
			{
				GSTNCombatDebug::DrawHurtboxes(GetWorld(), *Entry);
			}
		}
	}
#endif
}

void UGSTN_CombatSimComponent::TryQueueStatsModifier()
{
	const AActor* Owner = GetOwner();
	UMoverComponent* Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
	if (!Mover)
	{
		bStatsModifierQueued = true;
		return;
	}

	// Mover's sync state carries the modifier to the machines that simulate it: the authority and the owning client queue it, the
	// others get it with the state. A simulated proxy only interpolates, so it has nothing to write
	if (Owner->GetLocalRole() != ROLE_Authority && Owner->GetLocalRole() != ROLE_AutonomousProxy)
	{
		bStatsModifierQueued = true;
		return;
	}
	if (Mover->FindMovementModifierByType<FGSTN_StatsMovementModifier>())
	{
		bStatsModifierQueued = true;
		return;
	}

	// An invalid handle means Mover's simulation does not exist yet: try again next tick
	if (Mover->QueueMovementModifier(MakeShared<FGSTN_StatsMovementModifier>()).IsValid())
	{
		bStatsModifierQueued = true;
	}
}

void UGSTN_CombatSimComponent::RestoreFrame(const FGSTN_CombatSyncState* Sync, const FGSTN_CombatAuxState* Aux)
{
	// A rollback restores every combat instance, and each one invalidates: a snapshot built from the state before it is never read again
	if (UGSTN_CombatWorldSubsystem* Subsystem = WorldSubsystem.Get())
	{
		Subsystem->InvalidateSnapshot();
	}
	if (!Sync)
	{
		Inbox.Reset();
		return;
	}
	// The restored state is the end of its latest frame; the re-simulation starts on the next one
	Inbox.TrimFrom(Sync->Core.Input.History.GetLatestFrame() + 1);
}

bool UGSTN_CombatSimComponent::TicksOnThisMachine() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->GetLocalRole() != ROLE_SimulatedProxy;
}

bool UGSTN_CombatSimComponent::PostToInbox(const FGSTN_InboxEntry& Entry)
{
	if (!TicksOnThisMachine())
	{
		return false;
	}
	const uint8 Kind = Entry.Kind;
	const int32 SenderId = Entry.SenderId;
	const int32 Frame = Entry.Frame;
	if (!Inbox.Add(Entry))
	{
		UE_LOG(LogGSTNCombatSim, Warning, TEXT("%s: combat inbox full (%d entries), an entry of kind %d from %d at frame %d is dropped"), *GetPathName(), FGSTN_CombatInbox::MaxEntries, Kind, SenderId, Frame);
		return false;
	}
	return true;
}

bool UGSTN_CombatSimComponent::PostToInbox(int32 TargetId, const FGSTN_InboxEntry& Entry)
{
	UGSTN_CombatWorldSubsystem* World = WorldSubsystem.Get();
	return World && World->PostToInbox(TargetId, Entry);
}

bool UGSTN_CombatSimComponent::PostHit(const FGSTN_HitInboxEntry& Entry)
{
	return PostToInbox(Entry.ToInboxEntry());
}

void UGSTN_CombatSimComponent::FinalizeFrame(const FGSTN_CombatSyncState* Sync, const FGSTN_CombatAuxState* Aux)
{
	// Outside the simulation: the authority (a dedicated server, or the listen-server host's own pawn) publishes each event
	// once; a predicting client drops what its predictions and re-simulations wrote
	FGSTN_InputHostUtils::PublishOutbox(EventOutbox, GetOwner());

	// The surfaces a victim broke (combat-m4-hits 7b): the authority breaks each once, here, outside the simulation. A predicting client drops
	// the requests its predictions wrote; the surface replicates its own state
	if (!BreakRequests.IsEmpty())
	{
		ExecuteSurfaceBreaks();
	}

	// The attacker's own presentation (M4 H10): the locally controlled pawn shows its predicted hits at once and cancels the ones a correction removes
	if (Sync && TicksOnThisMachine())
	{
		if (!bHitPresentationSearched)
		{
			bHitPresentationSearched = true;
			HitPresentation = GetOwner()->FindComponentByClass<UGSTN_HitPresentationComponent>();
		}
		const APawn* Pawn = Cast<APawn>(GetOwner());
		if (HitPresentation.IsValid() && Pawn && Pawn->IsLocallyControlled())
		{
			HitPresentation->OnAttackerFrame(Sync->Core, Sync->Core.Input.History.GetLatestFrame(), RegisteredEntityId, PredictedHits, LastStepMs);
		}
	}
}

FString UGSTN_CombatSimComponent::GetHitstopMontageTrace() const
{
	TArray<int32> Frames;
	HitstopMontageTrace.GetKeys(Frames);
	Frames.Sort();
	FString Out;
	for (const int32 Frame : Frames)
	{
		Out += FString::Printf(TEXT("%d:%.4f;"), Frame, HitstopMontageTrace[Frame]);
	}
	return Out;
}

FString UGSTN_CombatSimComponent::GetHitstopMontageInfo() const
{
	const FGSTN_CombatSyncState* Sync = GetSyncState();
	float Time = -1.f;
	const bool bPlaying = AbilitySystem && AbilitySystem->GetPlayingSyncedMontage(Time) != nullptr;
	return FString::Printf(TEXT("%d,%.4f,%d,%d,%d"), bPlaying ? 1 : 0, Time, Sync ? Sync->Core.Input.History.GetLatestFrame() : -1,
		Sync ? Sync->Core.HitstopStartFrame : -1, Sync ? Sync->Core.HitstopEndFrame : -1);
}

void UGSTN_CombatSimComponent::ExecuteFreeze(TSubclassOf<UGameplayEffect> Effect, int32 Frames, float StepMs)
{
	if (!AbilitySystem || !Effect || Frames <= 0)
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(Effect, 1.f, AbilitySystem->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_FreezeSeconds, Frames * StepMs * 0.001f);
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
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

	ProjectStateTags(Sync->Core, Frame);

	// The tables of this frame (the combat tick of the same frame set them). A CancelTimeline names a slot of the registry the timeline
	// was started in, which on a swap frame is the previous set
	const UGSTN_CombatTableSet& FrameSet = Tables();
	const UGSTN_CombatTableSet& PrevSet = FramePrevTables ? *FramePrevTables : FrameSet;

	// A timeline replaced or cancelled this frame ends first, so its ability is gone before the replacing one starts
	for (const FGSTN_CombatRequest& Slot : Sync->Core.Requests.Slots)
	{
		if (Slot.Type == EGSTN_RequestType::CancelTimeline && Slot.Frame == Frame && PrevSet.TimelineRegistry.IsValidIndex(Slot.Arg))
		{
			const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(PrevSet.TimelineRegistry[Slot.Arg]);
			if (Spec && Spec->IsActive())
			{
				AbilitySystem->CancelAbilityHandle(Spec->Handle);
			}
		}
	}

	// An adoption that another ability won: this one waited in vain and is cancelled before the winner starts
	for (const FGSTN_CombatRequest& Slot : Sync->Core.Requests.Slots)
	{
		if (Slot.Type == EGSTN_RequestType::CancelAbility && Slot.Frame == Frame && FrameSet.TimelineRegistry.IsValidIndex(Slot.Arg))
		{
			const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(FrameSet.TimelineRegistry[Slot.Arg]);
			if (Spec && Spec->IsActive())
			{
				AbilitySystem->CancelAbilityHandle(Spec->Handle);
			}
		}
	}

	// A weapon switch of this frame: the old weapon's stats effect goes, the new one's comes (found by class, so a re-simulated
	// frame finds the same result). Before the activations so an ability that starts now sees the new stats
	for (const FGSTN_CombatRequest& Slot : Sync->Core.Requests.Slots)
	{
		if (Slot.Type == EGSTN_RequestType::SwapWeaponStats && Slot.Frame == Frame)
		{
			FGSTN_WeaponStatsSwap::Execute(*AbilitySystem, FrameSet.WeaponStats, Slot.Arg, Sync->Core.ActiveWeaponIndex);
		}
	}

	// A freeze of this frame (a hit's hitstop, or the debug window): queued on Mover before it ticks, for as many frames as the window
	for (const FGSTN_CombatRequest& Slot : Sync->Core.Requests.Slots)
	{
		if (Slot.Frame != Frame)
		{
			continue;
		}
		switch (Slot.Type)
		{
		case EGSTN_RequestType::Freeze:
			ExecuteFreeze(FreezeEffect, Slot.Arg, TimeStep.StepMs);
			break;
		case EGSTN_RequestType::VictimFreeze:
			// A victim that stays in place leaves the freeze at rest; any other keeps the velocity it came with until its move replaces it
			ExecuteFreeze(VictimResult.bFreezeZeroVelocity && VictimFrame == Frame ? FreezeAtRestEffect : FreezeEffect, Slot.Arg, TimeStep.StepMs);
			break;
		case EGSTN_RequestType::EndReaction:
			// The Mover half of a window that ended early: the freezes of the hitstop go with it (the state half was rewritten by the combat tick)
			if (AbilitySystem)
			{
				for (const TSubclassOf<UGameplayEffect>& Freeze : {FreezeEffect, FreezeAtRestEffect})
				{
					if (Freeze)
					{
						AbilitySystem->RemoveActiveGameplayEffectBySourceEffect(Freeze, AbilitySystem);
					}
				}
			}
			break;
		case EGSTN_RequestType::MeterGain:
			ExecuteMeterGain(Slot.Arg);
			break;
		case EGSTN_RequestType::ApplyDamage:
			ExecuteVictimHits(Frame);
			break;
		case EGSTN_RequestType::ApplyEffect:
			ExecuteVictimEffects(Frame);
			break;
		case EGSTN_RequestType::Cue:
			ExecuteVictimCues(Frame);
			break;
		case EGSTN_RequestType::ImpactDamage:
			if (VictimFrame == Frame)
			{
				ExecuteImpactDamage(ImpactResult.Damage);
			}
			break;
		case EGSTN_RequestType::Knockback:
		case EGSTN_RequestType::PushBack:
			if (VictimFrame == Frame)
			{
				ExecuteReactionMove(TimeStep.StepMs);
			}
			break;
		default:
			break;
		}
	}

	// A surface that launches, met by a combatant that is not tumbling: queued on Mover before it ticks, before a jump of this frame starts
	StepSurfaceLaunch(TimeStep, Sync->Core);

	// A timeline the model started by itself this frame (StartForcedTimeline: an extension's landing, tech, get-up): its ability is activated here and
	// finds its timeline running from this frame, like a command's
	for (const FGSTN_CombatRequest& Slot : Sync->Core.Requests.Slots)
	{
		if (Slot.Type == EGSTN_RequestType::ActivateEntry && Slot.Frame == Frame && FrameSet.TimelineRegistry.IsValidIndex(Slot.Arg) && AbilitySystem)
		{
			const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(FrameSet.TimelineRegistry[Slot.Arg]);
			if (Spec && !Spec->IsActive())
			{
				// Activated as the model's own request, like a command's: the timeline it belongs to is already the running one, which an
				// activation from outside the model (a trigger, an effect) would be refused for (UGSTN_GA_Base's interrupt policy)
				const FGameplayAbilitySpecHandle Handle = Spec->Handle;
				const FGSTN_CommandActivationScope CommandScope(Handle);
				if (!AbilitySystem->TryActivateAbility(Handle))
				{
					UE_LOG(LogGSTNCombatSim, Log, TEXT("%s frame %d: forced timeline %d (%s) did not activate"), *GetNameSafe(GetOwner()), Frame, static_cast<int32>(Slot.Arg),
						*GetNameSafe(FrameSet.TimelineRegistry[Slot.Arg].Get()));
				}
			}
		}
	}

	// A request made on this frame starts its ability here, inside the ability system's tick
	FGSTN_InputHostUtils::ExecuteRequest(AbilitySystem, bFrameGateClosed ? EmptyTableSet->Table : FrameSet.Table, Sync->Core.Input, Frame);

	// The cost of a timeline commits when its startup override window ends: the ability is committed from here, so a timeline
	// that was replaced inside the window never paid. Only the commit of the timeline the state runs counts: a frame can hold the
	// commit of one that was replaced in it, and the timeline logic removes that one, this is the second guard
	if (Sync->Core.ActionState == EGSTN_ActionState::InTimeline && Sync->Core.Requests.Find(EGSTN_RequestType::CommitTimeline, Frame, Sync->Core.ActiveTimeline))
	{
		if (FrameSet.TimelineRegistry.IsValidIndex(Sync->Core.ActiveTimeline))
		{
			const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(FrameSet.TimelineRegistry[Sync->Core.ActiveTimeline]);
			if (Spec)
			{
				// The meter can have dropped since the activation: a refused commit ends the ability (the clock then returns to
				// Actionable on the next frame) instead of letting the timeline run for free
				UGSTN_GA_BaseCombat::CommitOrCancel(*AbilitySystem, *Spec);
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

	if (HitstopTestMontage)
	{
		float Time = -1.f;
		AbilitySystem->GetPlayingSyncedMontage(Time);
		HitstopMontageTrace.Add(Frame, Time);
		HitstopMontageTrace.Remove(Frame - 256);
	}
}
