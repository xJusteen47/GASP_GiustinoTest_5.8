// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_CombatSimComponent.h"
#include "Loadout/GSTN_LoadoutInputFeel.h"

#include "Hits/GSTN_HurtboxPose.h"
#include "Movement/GSTN_GravityFrame.h"
#include "Loadout/GSTN_LoadoutModeRules.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "GameRules/GSTN_GameRules.h"

#include "Loadout/GSTN_LoadoutComponent.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GSTN_CombatWaitTasks.h"
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
#include "Movement/GSTN_LayeredMove_LedgeGuard.h"
#include "Movement/GSTN_LayeredMove_MomentumSlide.h"
#include "Movement/GSTN_LayeredMove_RunSkid.h"
#include "Presentation/GSTN_PrecisionAimComponent.h"
#include "Presentation/GSTN_RunPresentationComponent.h"
#include "Presentation/GSTN_CombatPresentationEvents.h"
#include "Timeline/GSTN_StartupSteer.h"
#include "Presentation/GSTN_SteerIndicatorComponent.h"
#include "Movement/GSTN_SetFacingEffect.h"
#include "Movement/GSTN_BodyCapsuleModifier.h"
#include "Input/GSTN_InputTags.h"
#include "Context/GSTN_InputContextProvider.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Kits/GSTN_LocomotionProfile.h"
#include "Movement/GSTN_AnalogWalkTypes.h"
#include "GSTN_CombatTags.h"
#include "GSTN_CombatRulesComponent.h"
#include "Input/GSTN_CombatSettings.h"
#include "Input/GSTN_DirectionClassifier.h"
#include "Input/GSTN_InputConfig.h"
#include "Input/GSTN_InputMatcher.h"
#include "Input/GSTN_InputQueries.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputSlot.h"
#include "Input/GSTN_InputScript.h"
#include "Input/GSTN_InputSettings.h"
#include "Movement/GSTN_StatsMovementModifier.h"
#include "Movement/GSTN_RunStageSyncState.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
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
#include "Components/CapsuleComponent.h"
#include "Debug/GSTN_CombatDebugDraw.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "Simulation/GSTN_CombatTraceBuild.h"
#include "Trace/GSTN_CombatTrace.h"
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
	SetFacingEffect = UGSTN_GE_SetFacing::StaticClass();
	MomentumSlideEffect = UGSTN_GE_MomentumSlide::StaticClass();
	DodgeLandingSlideEffect = UGSTN_GE_DodgeLandingSlide::StaticClass();
	LedgeGuardEffect = UGSTN_GE_LedgeGuard::StaticClass();
	RunSkidEffect = UGSTN_GE_RunSkid::StaticClass();

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

	// The precision reticle and aim arrow (presentation): added here so no pawn Blueprint has to place it
	if (bAddPrecisionAim && GetNetMode() != NM_DedicatedServer && GetOwner() && !GetOwner()->FindComponentByClass<UGSTN_PrecisionAimComponent>())
	{
		UGSTN_PrecisionAimComponent* Aim = NewObject<UGSTN_PrecisionAimComponent>(GetOwner(), TEXT("GSTN_PrecisionAim"));
		Aim->RegisterComponent();
	}
	else
	{
		UE_LOG(LogGSTNCombatSim, Warning, TEXT("%s: no context provider, the character is taken to face yaw 0 with no movement context"), *GetPathName());
	}
	// The steering arrows (presentation, owner 2026-11-14), the same way
	if (bAddSteerIndicator && GetNetMode() != NM_DedicatedServer && GetOwner() && !GetOwner()->FindComponentByClass<UGSTN_SteerIndicatorComponent>())
	{
		UGSTN_SteerIndicatorComponent* Arrows = NewObject<UGSTN_SteerIndicatorComponent>(GetOwner(), TEXT("GSTN_SteerIndicator"));
		Arrows->RegisterComponent();
	}
	// The run skid's placeholder pose and the foot slide hook (presentation, Mario-style run P2), the same way
	if (GetNetMode() != NM_DedicatedServer)
	{
		UGSTN_RunPresentationComponent::FindOrAdd(GetOwner());
	}
	// The presentation events (dust, sparks, shakes per kit table, Blueprint events): on every machine, the server included (events, no effects)
	UGSTN_CombatPresentationEvents::FindOrAdd(GetOwner());
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
	for (int32 Index = 0; Index < Set->TimelineRegistry.Num(); ++Index)
	{
		if (const FGSTN_CombatTimelineData* Data = Set->GetTimelineData(Index))
		{
			TimelineOutput.Reserve(*Data);
			AbilityStartOutput.Reserve(*Data);
		}
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
		Old->SetServerLoadoutApplier(nullptr);
	}
	BoundLoadout = Loadout;
	Loadout->OnLoadoutApplied.AddUniqueDynamic(this, &UGSTN_CombatSimComponent::HandleLoadoutApplied);
	// A client's request (the loadout screen) goes through the same server path as every other change: the table revision moves with it
	Loadout->SetServerLoadoutApplier([WeakThis = TWeakObjectPtr<UGSTN_CombatSimComponent>(this), WeakLoadout = TWeakObjectPtr<UGSTN_LoadoutComponent>(Loadout)]
		(const FGSTN_LoadoutData& Requested, TArray<FString>& OutErrors)
	{
		UGSTN_CombatSimComponent* This = WeakThis.Get();
		UGSTN_LoadoutComponent* Target = WeakLoadout.Get();
		if (!This || !Target)
		{
			return Target ? Target->AcceptLoadout(Requested, OutErrors) : false;
		}
		return This->ServerApplyLoadoutChange(Target, Requested, OutErrors);
	});

	// A loadout that was applied before this component looked (the client's OnRep ran when the component replicated in)
	if (Loadout->HasLoadout())
	{
		HandleLoadoutApplied(Loadout, Loadout->GetRevision());
	}
}

TArray<FGSTN_WeaponStatsEntry> UGSTN_CombatSimComponent::BuildWeaponStats(const FGSTN_ResolvedLoadout& Resolved)
{
	return BuildWeaponStats(Resolved, UGSTN_LoadoutModeRules::Get().SupportWeaponStats);
}

TArray<FGSTN_WeaponStatsEntry> UGSTN_CombatSimComponent::BuildWeaponStats(const FGSTN_ResolvedLoadout& Resolved, EGSTN_SupportWeaponStats SupportStats)
{
	TArray<FGSTN_WeaponStatsEntry> Entries;
	for (int32 Index = 0; Index < Resolved.Weapons.Num(); ++Index)
	{
		const UGSTN_WeaponDefinition* Weapon = Resolved.Weapons[Index];
		FGSTN_WeaponStatsEntry& Entry = Entries.AddDefaulted_GetRef();
		// KitOnly: every weapon after the primary is an empty entry, so switching to it leaves the kit's stats (the swap only removes)
		const bool bKitStatsOnly = Index > 0 && SupportStats == EGSTN_SupportWeaponStats::KitOnly;
		if (Weapon && !bKitStatsOnly)
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
	KitTumbleRecovery.Reset();
	if (const UGSTN_PrimaryKitDefinition* Kit = Component->GetResolvedLoadout().Kit)
	{
		if (const FGSTN_ImpactResponseExtension* Extension = Kit->Extensions.Find<FGSTN_ImpactResponseExtension>())
		{
			KitImpactResponse = Extension->Response;
		}
		// And its tumble recovery commands, likewise
		if (const FGSTN_TumbleRecoveryExtension* Recovery = Kit->Extensions.Find<FGSTN_TumbleRecoveryExtension>())
		{
			KitTumbleRecovery = *Recovery;
		}
	}

	const FGSTN_CompiledBindingTable& Compiled = Component->GetCompiledTable();
	if (Compiled.Entries.IsEmpty())
	{
		// The loadout did not compile on this machine: no set, so a demand for its revision keeps the gate closed instead of running a wrong table
		UE_LOG(LogGSTNCombatSim, Warning, TEXT("%s: loadout revision %d has no usable table on this machine, the combat simulation stays gated"), *GetOwner()->GetName(), Revision);
		return;
	}
	const uint32 RulesSerial = FGSTN_ActiveGameRules::GetSerial();
	if (const UGSTN_CombatTableSet* Existing = FindTableSet(Compiled.Revision); Existing && Existing->RulesSerial == RulesSerial)
	{
		// A revision's loadout never changes; a repeated apply (a pawn that was set again) has nothing new. Under other GameRules (IO-2b:
		// the server's pick arrived after this table was built) the same revision compiles to other buttons and replaces the set
		DemandAppliedRevision(Revision);
		return;
	}

	FGSTN_CombatTableConvertInput Input([](const FSoftObjectPath& Path) -> UClass* { return TSoftClassPtr<UGameplayAbility>(Path).LoadSynchronous(); });
	Input.InputSettings = &UGSTN_InputSettings::Get();
	Input.CombatSettings = &UGSTN_CombatSettings::Get();
	Input.GlobalResetRules = ResetRules;
	Input.Adoptable = AdoptableTimelines;
	Input.WeaponStats = BuildWeaponStats(Component->GetResolvedLoadout());
	BuildRicochetFallbacks(Component->GetResolvedLoadout(), Input.WeaponRicochet, Input.KitRicochet);
	Component->GetApplied().GetExtensionBlockBytes(Input.ReservedExtension);

	FGSTN_CompileReport Report;
	UGSTN_CombatTableSet* Set = FGSTN_CombatTableConverter::Convert(this, Compiled, Input, &Report);
	for (const FText& Error : Report.Errors)
	{
		UE_LOG(LogGSTNCombatSim, Warning, TEXT("%s: loadout revision %d: %s"), *GetOwner()->GetName(), Revision, *Error.ToString());
	}
	if (Set)
	{
		Set->RulesSerial = RulesSerial;
		// The kit's movement hurtbox travels with the revision's table (owner 2026-11-16: the idle/movement hurtbox may change per kit)
		const UGSTN_PrimaryKitDefinition* Kit = Component->GetResolvedLoadout().Kit;
		const FGSTN_MovementHurtboxExtension* Movement = Kit ? Kit->Extensions.Find<FGSTN_MovementHurtboxExtension>() : nullptr;
		Set->bHasKitMovementHurtbox = Movement != nullptr;
		Set->KitMovementHurtbox = Movement ? *Movement : FGSTN_MovementHurtboxExtension();
		// The kit's air crouch, the same way (owner 2026-11-16)
		const FGSTN_AirCrouchKitExtension* AirCrouch = Kit ? Kit->Extensions.Find<FGSTN_AirCrouchKitExtension>() : nullptr;
		Set->bHasKitAirCrouch = AirCrouch != nullptr;
		Set->KitAirCrouch = AirCrouch ? *AirCrouch : FGSTN_AirCrouchKitExtension();
		const FGSTN_GroundCrouchKitExtension* GroundCrouch = Kit ? Kit->Extensions.Find<FGSTN_GroundCrouchKitExtension>() : nullptr;
		Set->KitGroundCrouch = GroundCrouch ? *GroundCrouch : FGSTN_GroundCrouchKitExtension();
		const FGSTN_DodgeLandingKitExtension* DodgeLanding = Kit ? Kit->Extensions.Find<FGSTN_DodgeLandingKitExtension>() : nullptr;
		Set->KitDodgeLanding = DodgeLanding ? *DodgeLanding : FGSTN_DodgeLandingKitExtension();
		Set->StickThresholds = FGSTN_LoadoutInputFeelExt::Resolve(Component->GetApplied().Extensions);
	}
	RegisterTableSet(Set);
	DemandAppliedRevision(Revision);
}

void UGSTN_CombatSimComponent::DemandAppliedRevision(int32 Revision)
{
	// Every accept path ends here on the server (ServerApplyLoadoutChange, the loadout screen's request, a kit taken from the ground, a
	// grant from the PlayerState): the simulation must be told to run the new table, or it keeps the old revision's table while the old
	// abilities are gone and the kit does nothing (owner playtest 2026-11-13: no attack, no pickup after a kit change). The table set is
	// registered above, so the demand never names a table this machine lacks. A write of the same value is skipped
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (Sync && Sync->Core.LoadoutRevision != static_cast<uint16>(Revision))
	{
		ServerWriteLoadoutRevision(static_cast<uint16>(Revision));
	}
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
	return Set.GetTimelineData(Index);
}

int32 UGSTN_CombatSimComponent::GetSimulationFrame() const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	return Sync ? Sync->Core.Input.History.GetLatestFrame() : INDEX_NONE;
}

const UGSTN_CombatTableSet* UGSTN_CombatSimComponent::GetActiveTableSet() const
{
	return FrameTables || !TableSets.IsEmpty() || EmptyTableSet ? &Tables() : nullptr;
}

bool UGSTN_CombatSimComponent::IsTimelineRunning(const UClass* AbilityClass, int32& OutStartFrame, int32* OutEntry) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	const UGSTN_CombatTableSet& Current = Tables();
	if (!Sync || Sync->Core.ActionState != EGSTN_ActionState::InTimeline || !Current.TimelineRegistry.IsValidIndex(Sync->Core.ActiveTimeline))
	{
		return false;
	}
	OutStartFrame = Sync->Core.TimelineStartFrame;
	if (OutEntry)
	{
		*OutEntry = Sync->Core.ActiveTimeline;
	}
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
	const FGSTN_CombatTimelineData* Data = GetRegistryTimeline(Current, Sync->Core.ActiveTimeline);
	const EGSTN_StrikeKind StrikeKind = Data ? Data->ResolveStrike(Sync->Core.StrikeTrack).Kind : EGSTN_StrikeKind::Unspecified;
	OutResponse = FGSTN_CombatTimelineLogic::ResolveRicochet(Data, Sync->Core.StrikeTrack, Current.GetRicochetFallback(Sync->Core.ActiveWeaponIndex, StrikeKind));
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
	if (GateCore && Current.Table.Commands.IsValidIndex(CommandIndex))
	{
		// The dodge landing (owner 2026-11-17): its ending lag refuses every command but the kit's roll-out, which passes whatever the running dodge
		// says; its re-dodge lock refuses the dodge. A refused press stays in the buffer for as long as the buffer keeps it
		const EGSTN_DodgeLandingGate Landing = FGSTN_DodgeLandingLogic::Gate(GateCore->DodgeLanding, Current.KitDodgeLanding, GateFrame, Current.Table.Commands[CommandIndex].Tag);
		if (Landing != EGSTN_DodgeLandingGate::Free)
		{
			UE_LOG(LogGSTNCombatSim, Verbose, TEXT("%s frame %d: command %s %s by the dodge landing"), *GetNameSafe(GetOwner()), GateFrame, *Current.Table.Commands[CommandIndex].Tag.ToString(),
				Landing == EGSTN_DodgeLandingGate::Recovery ? TEXT("allowed as a roll-out") : TEXT("refused"));
			return Landing == EGSTN_DodgeLandingGate::Recovery;
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
	if (GateCore && FGSTN_CombatVictim::IsTumbleActionable(*GateCore) && Current.Table.Commands.IsValidIndex(CommandIndex))
	{
		// A tumble that recovered in the air admits only the recovery's commands (the kit's list, else the rules'; empty = every one)
		return FGSTN_TumbleRecoveryResolved::IsCommandAllowed(GetTumbleRecoveryCommands(), Current.Table.Commands[CommandIndex].Tag);
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

void UGSTN_CombatSimComponent::GetFrameStick(FVector2D& OutStick, float& OutCameraYawDegrees) const
{
	OutStick = FVector2D(FGSTN_DirectionClassifier::DequantizeStickAxis(FrameAim.StickX), FGSTN_DirectionClassifier::DequantizeStickAxis(FrameAim.StickY));
	OutCameraYawDegrees = FGSTN_DirectionClassifier::DequantizeYaw(FrameAim.CameraYaw);
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

	// Precision aim (slice 3b): the local reticle goes into the predicted command, so the steer and later aimable moves read it in the sim
	if (!CachedPrecisionAim.IsValid() && GetOwner())
	{
		CachedPrecisionAim = GetOwner()->FindComponentByClass<UGSTN_PrecisionAimComponent>();
	}
	Cmd->bPrecisionAim = CachedPrecisionAim.IsValid() && CachedPrecisionAim->IsReticleShown();
	bLastPrecisionAim = Cmd->bPrecisionAim;
	LastAimStickX = Cmd->AimStickX;
	LastAimStickY = Cmd->AimStickY;
	LastMoveStickX = Cmd->StickX;
	LastMoveStickY = Cmd->StickY;
	LastCameraYaw = Cmd->CameraYaw;
}

void UGSTN_CombatSimComponent::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	const FGSTN_CombatSyncState* Sync = GetSyncState();
	if (!Sync || !GetOwner())
	{
		return;
	}
	// The frame this input is for is the one after the latest simulated frame. Per-move steering (owner 2026-11-14): the reticle source holds
	// only with the reticle on (slice 3b, unchanged); the move stick sources hold from the first frame to the end of the last active one, so
	// the movement mode never turns the move further than its cap, nor after the lock. The rate (M11 L9) is the sim's, so they agree
	// A timeline that ignores the move stick (the get-ups): no walk, the facing held. Same frame rule as the steer below: the frame after the latest
	const bool bInTimeline = Sync->Core.ActionState == EGSTN_ActionState::InTimeline;
	const FGSTN_CombatTimelineData* Running = bInTimeline ? GetRegistryTimeline(Tables(), Sync->Core.ActiveTimeline) : nullptr;
	// Rooted outside a move (downed on the floor, a hit reaction): the stick must not turn the body either (react-hook-2 capture: a lying victim
	// turned under the held stick, so the roll back read the stick as forward). Inside a move the steer below decides
	const bool bRooted = !bInTimeline && AbilitySystem
		&& (AbilitySystem->HasMatchingGameplayTag(GSTNCombatTags::Lock_Movement) || AbilitySystem->HasMatchingGameplayTag(GSTNCombatTags::Lock_Reaction));
	// A hit turn the latest frame queued (the victim step's SetFacing; the state holds only that frame's requests): Mover applies it on the
	// frame this input is for, and the walking mode orients to the command's intent on that same tick, so an intent still pointing the old way
	// undid the turn (react-hook-5 diagnostics: SetFacing ran on the hit frame, the yaw never changed). The intent points the new way instead
	// Yaws about the local up (gravity audit 2026-11-16): Mover keeps the capsule upright on the gravity floor, so the actor's up is it
	const FVector OwnerUp = GetOwner()->GetActorUpVector();
	const float OwnerYaw = FGSTN_GravityFrame::YawOfRotator(GetOwner()->GetActorRotation(), OwnerUp);
	float HoldYaw = OwnerYaw;
	bool bHitTurn = false;
	// Read inside a move too: a move's start snap (back aerial, 180 deg) is a SetFacing of the latest frame, and the held facing below must
	// point the new way on the frame Mover applies it
	bool bQueuedTurn = false;
	for (const FGSTN_CombatRequest& Slot : Sync->Core.Requests.Slots)
	{
		if (Slot.Type == EGSTN_RequestType::SetFacing || Slot.Type == EGSTN_RequestType::SetFacingTurnMomentum)
		{
			HoldYaw = FGSTN_StartupSteer::DequantizeYaw(Slot.Arg);
			bQueuedTurn = true;
		}
	}
	bHitTurn = !bInTimeline && bQueuedTurn;
	// Ground crouch without crouch walk (FGSTN_GroundCrouchKitExtension::bCrouchWalk off, Smash-like): crouched outside a move, the stick does not
	// walk and the facing holds. Read from the latest frame's decision, like the steering below; the command carries it, so a resimulation agrees
	bool bCrouchStill = false;
	if (!bInTimeline && FrameCrouch.bCrouch && FrameCrouch.Frame != INDEX_NONE)
	{
		const UGSTN_CombatTableSet* CrouchTables = FindTableSet(Sync->Core.TableRevision);
		bCrouchStill = CrouchTables && !CrouchTables->KitGroundCrouch.bCrouchWalk;
	}
	// A dodge's ending lag (owner 2026-11-17): no walking and no turning, the speed it kept slides out (the kit's landing slide) or brakes with the
	// walking mode's friction. The frame this input is for, like the rest
	const bool bDodgeEndLag = Sync->Core.DodgeLanding.IsEndLag(GetSimulationFrame() + 1);
	if ((Running && Running->bIgnoreMoveInput) || bRooted || bHitTurn || bCrouchStill || bDodgeEndLag)
	{
		if (FCharacterDefaultInputs* Inputs = InputCmdResult.InputCollection.FindMutableDataByType<FCharacterDefaultInputs>())
		{
			Inputs->SetMoveInput(EMoveInputType::DirectionalIntent, FVector::ZeroVector);
		}
		HoldOrientationIntent(InputCmdResult, HoldYaw, OwnerUp);
		bHoldFacingAfterReaction = true;
		return;
	}
	const FVector2D AimStick(FGSTN_DirectionClassifier::DequantizeStickAxis(LastAimStickX), FGSTN_DirectionClassifier::DequantizeStickAxis(LastAimStickY));
	const FVector2D MoveStick(FGSTN_DirectionClassifier::DequantizeStickAxis(LastMoveStickX), FGSTN_DirectionClassifier::DequantizeStickAxis(LastMoveStickY));
	// Analog walk and air turn (kit locomotion profile data): input only, so they are predicted and replayed with the command
	const FGSTN_LocomotionSimTuning* Locomotion = FGSTN_LocomotionAttach::Find(GetOwner());
	const FGSTN_AnalogWalkTuning* Analog = Locomotion && Locomotion->AnalogWalk.bEnabled ? &Locomotion->AnalogWalk : nullptr;
	const float CameraYawDeg = FGSTN_DirectionClassifier::DequantizeYaw(LastCameraYaw);
	if (Analog)
	{
		const APawn* OwnerPawn = Cast<APawn>(GetOwner());
		ApplyAnalogMoveIntent(InputCmdResult, *Analog, MoveStick, CameraYawDeg, OwnerUp, OwnerPawn && OwnerPawn->IsPlayerControlled());
	}
	float SteerHoldYaw = 0.f;
	bool bStickActive = false;
	if (FGSTN_CombatTimelineLogic::ResolveSteerFrame(Sync->Core, GetSimulationFrame() + 1, GetRegistryTimeline(Tables(), Sync->Core.ActiveTimeline), MoveStick,
		AimStick, bLastPrecisionAim, FGSTN_DirectionClassifier::DequantizeYaw(LastCameraYaw), OwnerYaw,
		LastStepMs * 0.001f, SteerHoldYaw, bStickActive) != EGSTN_SteerPhase::None)
	{
		HoldOrientationIntent(InputCmdResult, SteerHoldYaw, OwnerUp);
		bHoldFacingAfterReaction = false;
		return;
	}
	// Free after a reaction: the facing the hit turned to stays until the player pushes the stick
	if (bHoldFacingAfterReaction)
	{
		if (MoveStick.SizeSquared() > FMath::Square(0.2))
		{
			bHoldFacingAfterReaction = false;
		}
		else
		{
			HoldOrientationIntent(InputCmdResult, OwnerYaw, OwnerUp);
			return;
		}
	}
	// Air turn (owner 2026-11-16: "in the air you can turn"): GASP orients to the velocity, which air control barely bends, so the facing
	// follows the stick at the profile's rate instead. A move in progress keeps its facing unless the profile lets it turn
	if (Analog && Analog->AirTurnRateDegPerSec > 0.f && MoveStick.SizeSquared() > FMath::Square(FMath::Max(Analog->InnerDeadzone, 0.05f))
		&& (!bInTimeline || Analog->bAirTurnDuringMoves))
	{
		const UMoverComponent* Mover = GetOwner()->FindComponentByClass<UMoverComponent>();
		if (Mover && !Mover->HasGameplayTag(Mover_IsOnGround, false))
		{
			const FVector StickDir = FGSTN_CombatVictim::StickToPlane(MoveStick, CameraYawDeg, OwnerUp);
			if (!StickDir.IsNearlyZero())
			{
				const float TargetYaw = FGSTN_GravityFrame::YawOfRotator(FRotationMatrix::MakeFromXZ(StickDir, OwnerUp).Rotator(), OwnerUp);
				const float StepSeconds = LastStepMs > 0 ? LastStepMs * 0.001f : 1.f / 62.5f;
				HoldOrientationIntent(InputCmdResult, FGSTN_AnalogWalkLogic::StepAirYaw(*Analog, OwnerYaw, TargetYaw, StepSeconds), OwnerUp);
				return;
			}
		}
	}
	// A move without a steer phase keeps the facing it has (playtest 2026-11-17: the back aerial snapped 180 deg, then GASP's orientation
	// pulled it back to the old facing at ~5 deg/frame for the whole move). A start snap queued on the latest frame is the facing to keep
	if (bInTimeline)
	{
		// A forced move of a downed body (the ground bounce after a knockdown, the dropkick's own landing): the downed lock is on, so the stick
		// neither walks nor turns it (playtest 2026-11-17: the body turned under the stick after the dropkick). Only a roll or get-up turns it
		if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(GSTNCombatTags::Lock_Movement))
		{
			if (FCharacterDefaultInputs* Inputs = InputCmdResult.InputCollection.FindMutableDataByType<FCharacterDefaultInputs>())
			{
				Inputs->SetMoveInput(EMoveInputType::DirectionalIntent, FVector::ZeroVector);
			}
		}
		HoldOrientationIntent(InputCmdResult, HoldYaw, OwnerUp);
	}
}

bool UGSTN_CombatSimComponent::ApplyAnalogMoveIntent(FMoverInputCmdContext& InputCmd, const FGSTN_AnalogWalkTuning& Tuning, const FVector2D& Stick,
	float CameraYawDeg, const FVector& Up, bool bPlayerStick)
{
	FCharacterDefaultInputs* Inputs = InputCmd.InputCollection.FindMutableDataByType<FCharacterDefaultInputs>();
	if (!Inputs || Inputs->GetMoveInputType() != EMoveInputType::DirectionalIntent)
	{
		return false;
	}
	// No stick, nothing to scale: a pawn moved by something else (an AI's path following) keeps its own intent
	const float Intent = FGSTN_AnalogWalkLogic::StickToIntent(Tuning, static_cast<float>(FMath::Min(Stick.Size(), 1.0)));
	if (Intent <= 0.f)
	{
		// A player's stick inside their dead zone (UGSTN_InputOptionsSubsystem) is no move at all: the pawn's own intent comes from the mapping
		// context, which keeps only a hardware floor, so it would creep. A pawn without a player (AI path following) keeps its own
		if (bPlayerStick && !Inputs->GetMoveInput().IsNearlyZero())
		{
			Inputs->SetMoveInput(EMoveInputType::DirectionalIntent, FVector::ZeroVector);
			return true;
		}
		return false;
	}
	FVector Direction = Inputs->GetMoveInput().GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		// The pawn dropped a light tilt: the stick's own direction on the ground plane, camera relative like the pawn's
		Direction = FGSTN_CombatVictim::StickToPlane(Stick, CameraYawDeg, Up).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			return false;
		}
	}
	Inputs->SetMoveInput(EMoveInputType::DirectionalIntent, Direction * Intent);
	return true;
}

bool UGSTN_CombatSimComponent::GetSteerView(FSteerView& Out) const
{
	Out = FSteerView();
	const FGSTN_CombatSyncState* Sync = GetSyncState();
	const FGSTN_CombatTimelineData* Timeline = Sync ? GetRegistryTimeline(Tables(), Sync->Core.ActiveTimeline) : nullptr;
	if (!Timeline || !GetOwner())
	{
		return false;
	}
	const FVector2D AimStick(FGSTN_DirectionClassifier::DequantizeStickAxis(LastAimStickX), FGSTN_DirectionClassifier::DequantizeStickAxis(LastAimStickY));
	const FVector2D MoveStick(FGSTN_DirectionClassifier::DequantizeStickAxis(LastMoveStickX), FGSTN_DirectionClassifier::DequantizeStickAxis(LastMoveStickY));
	const float CameraYaw = FGSTN_DirectionClassifier::DequantizeYaw(LastCameraYaw);
	float Yaw = 0.f;
	bool bStickActive = false;
	// The latest simulated frame, step 0: where the steering stands, not a new step of it
	Out.Phase = FGSTN_CombatTimelineLogic::ResolveSteerFrame(Sync->Core, GetSimulationFrame(), Timeline, MoveStick, AimStick, bLastPrecisionAim, CameraYaw,
		FGSTN_GravityFrame::YawOfRotator(GetOwner()->GetActorRotation(), GetOwner()->GetActorUpVector()), 0.f, Yaw, bStickActive);
	if (Out.Phase == EGSTN_SteerPhase::None)
	{
		return false;
	}
	Out.StartYawDeg = FGSTN_StartupSteer::DequantizeYaw(static_cast<int16>(Sync->Core.Hit.MoveOrigin.RotationQ[1]));
	Out.MaxSteerDegrees = Timeline->MaxSteerDegrees;
	Out.bShow = Timeline->Steer.bShowIndicator;
	FVector2D Stick;
	if (FGSTN_StartupSteer::PickStick(Timeline->Steer.Source, MoveStick, AimStick, bLastPrecisionAim, Stick, Timeline->Steer.StickDeadZone)
		&& FGSTN_StartupSteer::IsSteering(Stick, Timeline->MaxSteerDegrees, Timeline->Steer.StickDeadZone))
	{
		Out.bStickActive = true;
		Out.StickYawDeg = FRotator::NormalizeAxis(CameraYaw + FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(Stick.X, Stick.Y))));
	}
	return true;
}

bool UGSTN_CombatSimComponent::HoldOrientationIntent(FMoverInputCmdContext& InputCmd, float YawDeg, const FVector& Up)
{
	FCharacterDefaultInputs* Inputs = InputCmd.InputCollection.FindMutableDataByType<FCharacterDefaultInputs>();
	if (!Inputs || Inputs->bUsingMovementBase)
	{
		return false;
	}
	// YawDeg is about the local up: Mover's walking mode reads the intent back in its gravity frame (ApplyGravityToOrientationIntent)
	Inputs->OrientationIntent = FGSTN_GravityFrame::DirectionOfYaw(YawDeg, Up);
	return true;
}

FVector UGSTN_CombatSimComponent::GetBaseGravity() const
{
	if (GravityBase.bValid)
	{
		return GravityBase.Base;
	}
	const UMoverComponent* Mover = GetOwner() ? GetOwner()->FindComponentByClass<UMoverComponent>() : nullptr;
	return Mover ? Mover->GetGravityAcceleration() : FVector(0.0, 0.0, -FGSTN_GravityFrame::NominalGravity);
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
	// Coyote time for the ground moves that may take off (owner 2026-11-16): the jump step of this frame ran before the input step
	if (bFrameInCoyote)
	{
		OutTags.AddTag(GSTNInputTags::Context_Coyote);
	}
	// The run skid's tags (owner 2026-11-16 12:46Z), from the tracked Mover state of the end of the previous frame: a kit binding can require
	// Movement.State.Skid or its JumpWindow (the window is already shifted by that frame, FGSTN_RunSkidMath::InJumpWindow)
	if (const UGSTN_MoverSimStateTracker* Tracker = JumpTracker.Get())
	{
		if (const FGSTN_LayeredMove_RunSkid* Skid = FGSTN_LayeredMove_RunSkid::FindActive(Tracker->GetSyncState()))
		{
			Skid->GetGameplayTags(OutTags);
		}
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
	// A Wait Command of a running ability takes this command first (Priority Before Bindings): the binding does not start, so the slot keeps its
	// press for the wait, as a locked command does. The tasks are read as the previous frame left them, like the lock tags
	if (AbilitySystem)
	{
		const FGSTN_CommandTable& Bound = GetCommandTable();
		for (int32 CommandIndex = 0; CommandIndex < Bound.Commands.Num(); ++CommandIndex)
		{
			const FGSTN_CommandTable::FCommand& Command = Bound.Commands[CommandIndex];
			if (BindingIndex >= Command.FirstBinding && BindingIndex < Command.FirstBinding + Command.NumBindings)
			{
				if (UGSTN_WaitCommandTask::IsCommandClaimed(*AbilitySystem, CommandIndex))
				{
					UE_LOG(LogGSTNCombatSim, Verbose, TEXT("%s: binding %d held back, a Wait Command claims %s"), *GetNameSafe(GetOwner()), BindingIndex, *Command.Tag.ToString());
					return false;
				}
				break;
			}
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

namespace
{
	/** The age context of the input questions: the frame being simulated and the hitstop window of its state, the buffer's rule */
	FGSTN_InputQueries::FAgeContext MakeQueryAgeContext(const FGSTN_CombatSyncState& Sync)
	{
		FGSTN_InputQueries::FAgeContext Context;
		Context.Now = Sync.Core.Input.History.GetLatestFrame();
		Context.PausedStart = Sync.Core.HitstopStartFrame;
		Context.PausedEnd = Sync.Core.HitstopEndFrame;
		return Context;
	}
}

int32 UGSTN_CombatSimComponent::FindQueryButton(const FGameplayTag& Button) const
{
	return ActiveInputConfig && Button.IsValid() ? ActiveInputConfig->FindButtonIndex(Button) : INDEX_NONE;
}

bool UGSTN_CombatSimComponent::WasButtonPressedWithin(const FGameplayTag& Button, int32 Frames, bool bIncludeUsed) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	const int32 Index = FindQueryButton(Button);
	return Sync && Index != INDEX_NONE && FGSTN_InputQueries::WasPressedWithin(Sync->Core.Input.History, Index, MakeQueryAgeContext(*Sync), Frames, bIncludeUsed);
}

int32 UGSTN_CombatSimComponent::GetFramesSinceButtonPressed(const FGameplayTag& Button, bool bIncludeUsed) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	const int32 Index = FindQueryButton(Button);
	return Sync && Index != INDEX_NONE ? FGSTN_InputQueries::GetNewestPressAge(Sync->Core.Input.History, Index, MakeQueryAgeContext(*Sync), bIncludeUsed) : INDEX_NONE;
}

int32 UGSTN_CombatSimComponent::CountButtonPressesWithin(const FGameplayTag& Button, int32 Frames, bool bIncludeUsed) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	const int32 Index = FindQueryButton(Button);
	return Sync && Index != INDEX_NONE ? FGSTN_InputQueries::CountPressesWithin(Sync->Core.Input.History, Index, MakeQueryAgeContext(*Sync), Frames, bIncludeUsed) : 0;
}

int32 UGSTN_CombatSimComponent::GetButtonHeldFrames(const FGameplayTag& Button) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	const int32 Index = FindQueryButton(Button);
	return Sync && Index != INDEX_NONE ? FGSTN_InputQueries::GetHeldFrames(Sync->Core.Input.History, Index, Sync->Core.Input.History.GetLatestFrame()) : INDEX_NONE;
}

namespace
{
	/**
	 * Matches command Index of Table on Core's history with the slot's own rules (simultaneity, direction policy), BufferFrames as the time
	 * buffer (-1 = the slot's own) and no hold buffer: "entered in the last N frames", not "still held". The matcher skips used presses, so the
	 * press that started the move never answers
	 */
	bool MatchCommandInBuffer(const FGSTN_CombatSyncCore& Core, const FGSTN_CommandTable& Table, const FGSTN_ButtonMap& Buttons, int32 Index, int32 BufferFrames, FGSTN_PatternMatch& OutMatch)
	{
		const FGSTN_InputSlotSpec* Slot = Table.Commands.IsValidIndex(Index) ? Table.Commands[Index].Slot : nullptr;
		if (!Slot)
		{
			return false;
		}
		FGSTN_MatchParams Params = Slot->MakeMatchParams(UGSTN_InputSettings::Get(), Buttons, Core.Input.History.GetLatestFrame());
		if (BufferFrames >= 0)
		{
			Params.BufferFrames = BufferFrames;
		}
		Params.bHoldBuffer = false;
		Params.PausedStart = Core.HitstopStartFrame;
		Params.PausedEnd = Core.HitstopEndFrame;
		return FGSTN_InputMatcher::Match(Slot->Pattern, Core.Input.History, Params, OutMatch);
	}
}

bool UGSTN_CombatSimComponent::WasCommandEnteredWithin(const FGameplayTag& Command, int32 Frames) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (!Sync || Frames < 0)
	{
		return false;
	}
	const FGSTN_CommandTable& Table = GetCommandTable();
	FGSTN_PatternMatch Match;
	return MatchCommandInBuffer(Sync->Core, Table, ButtonMap, Table.FindCommand(Command), Frames, Match);
}

int32 UGSTN_CombatSimComponent::FindCommandIndex(const FGameplayTag& Command) const
{
	return GetCommandTable().FindCommand(Command);
}

bool UGSTN_CombatSimComponent::CanTakeCommandForWait(int32 CommandIndex, EGSTN_CancelCondition Condition) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (!Sync || !AbilitySystem || !AbilitySystem->IsInSimulationTick() || AbilitySystem->GetIsRestoringFrame() || bFrameGateClosed)
	{
		return false;
	}
	const FGSTN_CommandTable& Table = GetCommandTable();
	if (!Table.Commands.IsValidIndex(CommandIndex) || Table.IsCommandLocked(CommandIndex, AbilitySystem->GetOwnedGameplayTags()))
	{
		return false;
	}
	const int32 Frame = Sync->Core.Input.History.GetLatestFrame();
	return !Sync->Core.IsInHitstop(Frame) && !FGSTN_CombatVictim::IsActionLocked(Sync->Core)
		&& FGSTN_CombatTimelineLogic::IsConditionMet(Condition, Sync->Core, AbilityStepCancelConditions);
}

bool UGSTN_CombatSimComponent::IsCommandEnteredForWait(int32 CommandIndex, int32 BufferFrames, int32* OutTerminalFrame) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	FGSTN_PatternMatch Match;
	const bool bEntered = Sync && MatchCommandInBuffer(Sync->Core, GetCommandTable(), ButtonMap, CommandIndex, BufferFrames, Match);
	if (OutTerminalFrame)
	{
		*OutTerminalFrame = bEntered && Match.StepAnchors.Num() > 0 ? Match.StepAnchors.Last() : INDEX_NONE;
	}
	return bEntered;
}

bool UGSTN_CombatSimComponent::ConsumeCommandForWait(int32 CommandIndex, int32 BufferFrames)
{
	const FGSTN_CombatSyncState* Pending = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (!Pending || !AbilitySystem || !AbilitySystem->IsInSimulationTick() || AbilitySystem->GetIsRestoringFrame())
	{
		return false;
	}
	FGSTN_PatternMatch Match;
	if (!MatchCommandInBuffer(Pending->Core, GetCommandTable(), ButtonMap, CommandIndex, BufferFrames, Match))
	{
		return false;
	}
	// Into this frame's combat state, from the ability system's step (as StartTimelineFromAbility): the next frame's tick starts from it, so no
	// binding fires on these presses later. The event slots of the match index the same history, unchanged since the read above
	const FGSTN_CombatSyncState* Written = NetworkPredictionProxy.WriteSyncState<FGSTN_CombatSyncState>([&Match](FGSTN_CombatSyncState& Sync)
	{
		for (const uint8 EventSlot : Match.UsedButtonEvents)
		{
			Sync.Core.Input.History.Consume(EventSlot);
		}
		if (Match.TerminalStickAnchor != INDEX_NONE)
		{
			Sync.Core.Input.History.ConsumeSticksUpTo(Match.TerminalStickAnchor);
		}
	}, "GSTN WaitCommandConsume");
	return Written != nullptr;
}

FVector2D UGSTN_CombatSimComponent::GetMoveStick() const
{
	FVector2D Stick = FVector2D::ZeroVector;
	float CameraYaw = 0.f;
	GetFrameStick(Stick, CameraYaw);
	return Stick;
}

EGSTN_StickDirection UGSTN_CombatSimComponent::GetStickDirection(EGSTN_DirectionSpace Space) const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	return Sync ? Sync->Core.Input.History.GetCurrentStick().GetDirection(Space) : EGSTN_StickDirection::Neutral;
}

int32 UGSTN_CombatSimComponent::GetMoveHitConfirmGroups() const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	return Sync && Sync->Core.ActionState == EGSTN_ActionState::InTimeline ? static_cast<int32>(Sync->Core.Hit.HitConfirmFlags) : 0;
}

int32 UGSTN_CombatSimComponent::GetEvaluatedTimelineFrame() const
{
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	if (!Sync || Sync->Core.ActionState != EGSTN_ActionState::InTimeline || Sync->Core.TimelineStartFrame == INDEX_NONE || Sync->Core.RecoilStartFrame != INDEX_NONE)
	{
		return INDEX_NONE;
	}
	// FGSTN_CombatTimelineLogic's evaluated frame (EvaluateInto): the paused frames already count this frame's own pause
	return Sync->Core.Input.History.GetLatestFrame() - Sync->Core.TimelineStartFrame - Sync->Core.TimelinePausedFrames;
}

const FGSTN_CombatTimelineData* UGSTN_CombatSimComponent::ReadEvaluatedTimeline(int32& OutFrame) const
{
	OutFrame = GetEvaluatedTimelineFrame();
	if (OutFrame == INDEX_NONE)
	{
		return nullptr;
	}
	const FGSTN_CombatSyncState* Sync = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	const FGSTN_CombatTimelineData* Data = Sync ? GetRegistryTimeline(Tables(), Sync->Core.ActiveTimeline) : nullptr;
	if (!Data)
	{
		OutFrame = INDEX_NONE;
	}
	return Data;
}

#if !UE_BUILD_SHIPPING
namespace
{
	/** E17: one combat event to the trace sink, if one is installed. Reads nothing of the simulation; the arguments are values the tick already holds */
	FORCEINLINE void TraceCombatEvent(EGSTN_CombatTraceEventKind Kind, int32 EntityId, const FNetSimTimeStep& TimeStep, int32 A, int32 B)
	{
		if (IGSTN_CombatTraceSink* Sink = GSTNCombatTrace::GetSink())
		{
			FGSTN_CombatTraceEvent Event;
			Event.EntityId = EntityId;
			Event.Frame = TimeStep.Frame;
			Event.Kind = Kind;
			Event.bResimulated = TimeStep.IsResimulating;
			Event.A = A;
			Event.B = B;
			Sink->OnCombatEvent(Event);
		}
	}
}
#endif

void UGSTN_CombatSimComponent::SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<FGSTN_CombatStateTypes>& Input,
	const TNetSimOutput<FGSTN_CombatStateTypes>& Output)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GSTN_CombatSimComponent_SimulationTick);
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

	// The pushbox move of this frame, from the same end-of-previous-frame snapshot every fighter reads: a pair's shares are equal and opposite.
	// Mover's tick of this frame applies it (FGSTN_BodyCapsuleModifier)
	ComputeFramePush(Frame, StepSnapshot);

	// The aim of this frame's command, kept for the ability system's tick that follows (GetAimRotation)
	FrameAim.bHasAim = Input.Cmd->bHasAim;
	FrameAim.Yaw = Input.Cmd->AimYaw;
	FrameAim.Pitch = Input.Cmd->AimPitch;
	FrameAim.StickX = Input.Cmd->StickX;
	FrameAim.StickY = Input.Cmd->StickY;
	FrameAim.CameraYaw = Input.Cmd->CameraYaw;
	FrameAim.bPrecisionAim = Input.Cmd->bPrecisionAim;

	// Requests older than this frame are dead (the ability system executed them in their own frame's pre-tick)
	OutSync.Core.Requests.PruneBefore(Frame);

	// The victim side (combat-m4-hits 7): the hits the attackers wrote on the previous frame are resolved before anything else of this frame, so
	// the hitstop window they open already stands this frame (its first frame is the frame after the hit, like the attacker's), and a cancelled
	// move or a hitstun state is what the rest of the tick sees
	RunVictimStep(TimeStep, InSync, OutSync, StepSnapshot, *Input.Cmd);

	// The dodge landing (owner 2026-11-17): a hit reaction or a KO of this frame ends its lag and lock, so does the end of both windows
	FGSTN_DodgeLandingLogic::Normalize(OutSync.Core.DodgeLanding, Frame, FGSTN_CombatVictim::IsInReaction(OutSync.Core) || OutSync.Core.ActionState == EGSTN_ActionState::KO);

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
	FrameClockStill = bHitstopFrame ? Frame : INDEX_NONE;

	// What the track hooks of the plugins may read besides the input: the button map and the fragments as this frame writes them (M6 F7)
	FGSTN_TrackHookEnvironment HookEnvironment;
	HookEnvironment.Buttons = &ButtonMap;
	HookEnvironment.Fragments = &OutSync.Fragments;
	// Mover's ground as of the end of the previous frame (the tracker is rolled back with Mover): the Hold Until Landed rule of the clock reads it
	if (const UGSTN_MoverSimStateTracker* GroundTracker = GetJumpTracker())
	{
		HookEnvironment.Ground = GroundTracker->HasGameplayTag(Mover_IsOnGround, true) ? EGSTN_TrackHookGround::OnGround : EGSTN_TrackHookGround::Airborne;
	}
	// The Ledge Check rule's floor probe (FGSTN_LedgeRules): called by the clock only inside a ledge window, on the ground
	HookEnvironment.HasFloorAhead = [this](float AheadCm, float DepthCm) { return ProbeFloorAhead(AheadCm, DepthCm); };
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
		// The recoil fallback follows the strike kind of the group that ricocheted (plan precisione-weakpoint); the move's kind without one
		const FGSTN_CombatTimelineData* RunningData = GetRegistryTimeline(FrameSet, Running);
		const EGSTN_StrikeKind StrikeKind = RunningData ? RunningData->ResolveStrike(OutSync.Core.StrikeTrack).Kind : EGSTN_StrikeKind::Unspecified;
		FGSTN_CombatTimelineLogic::AdvanceClock(OutSync.Core, Frame, RunningData,
			FrameSet.TimelineDefaultOverride.IsValidIndex(Running) ? FrameSet.TimelineDefaultOverride[Running] : 0, bAbilityActive, TimelineOutput, bHitstopFrame,
			FrameSet.GetRicochetFallback(OutSync.Core.ActiveWeaponIndex, StrikeKind), &HookEnvironment);
		if (bWasRunning && OutSync.Core.ActionState != EGSTN_ActionState::InTimeline)
		{
			UE_LOG(LogGSTNCombatSim, Verbose, TEXT("%s frame %d: timeline %d (start %d) ended: timeline frame %d of %d, finished %d, ability active %d"), *GetOwner()->GetName(), Frame, Running, StartBefore,
				TimelineOutput.Frame, TimelineOutput.TotalFrames, TimelineOutput.bFinished, bAbilityActive);

			// A move that ended BY ITSELF links into its follow-up (FGSTN_CombatTimelineData::FollowUpTimeline: the dropkick's land into its get-up),
			// on this same frame, before the adoption and the commands of the frame: a buffered command can still replace it by the normal rules
			int32 FollowUp = !bFrameGateClosed && TimelineOutput.bFinished && !TimelineOutput.bRecoil ? FrameSet.FindFollowUpEntry(Running) : INDEX_NONE;
			// A landing inside a ContinueWithMove window (FGSTN_TimelineTrack_Landing) hands over to its continuation the same way
			if (!bFrameGateClosed && TimelineOutput.LandingContinueTrack != INDEX_NONE)
			{
				FollowUp = FrameSet.FindLandingContinueEntry(Running, TimelineOutput.LandingContinueTrack);
			}
			if (FollowUp != INDEX_NONE)
			{
				StartTimelineEntry(OutSync.Core, Frame, FrameSet, FollowUp, &HookEnvironment);
				UE_LOG(LogGSTNCombatSim, Verbose, TEXT("%s frame %d: timeline %d links into its follow-up %d"), *GetOwner()->GetName(), Frame, Running, FollowUp);
			}
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
#if !UE_BUILD_SHIPPING
		if (Adopted != INDEX_NONE)
		{
			TraceCombatEvent(EGSTN_CombatTraceEventKind::Adoption, RegisteredEntityId, TimeStep, Adopted, 0);
		}
#endif
		if (const FGSTN_CombatTimelineData* Data = GetRegistryTimeline(FrameSet, Adopted))
		{
			FGSTN_CombatTimelineLogic::EvaluateStart(OutSync.Core, Frame, *Data, FrameSet.TimelineDefaultOverride[Adopted], TimelineOutput, &HookEnvironment);
			StampMoveOrigin(OutSync.Core, Frame);
		}
	}

	// The gate the input simulation asks (IsCommandAllowed) reads the state this frame is writing
	GateCore = &OutSync.Core;
	GateFrame = Frame;

	// Extension hooks at the fixed points of the tick. With nothing registered every call is an empty loop
	FGSTN_CombatTickContext HookContext(Frame, InSync, OutSync);
	{
		FTransform Pose;
		FVector Up = FVector::UpVector;
		ReadOwnPose(Pose, Up);
		FillHookContext(HookContext, FrameCmd, Up);
	}
	HookContext.Timeline = &TimelineOutput;
	HookContext.RunningTimeline = OutSync.Core.ActionState == EGSTN_ActionState::InTimeline ? GetRegistryTimeline(FrameSet, OutSync.Core.ActiveTimeline) : nullptr;
	HookContext.Snapshot = StepSnapshot;
	// What an extension may ask the model to do in this tick (StartForcedTimeline): this frame's state, tables and hook environment
	ServiceSync = &OutSync;
	ServiceFrame = Frame;
	ServiceHookEnvironment = &HookEnvironment;
	ServiceTimeStep = &TimeStep;
	ServiceStepSnapshot = StepSnapshot;
	ServiceView = nullptr;
	bServiceViewResolved = false;
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
#if !UE_BUILD_SHIPPING
		for (const FGSTN_AttackerHit& Landed : AttackerHits)
		{
			TraceCombatEvent(EGSTN_CombatTraceEventKind::Hit, RegisteredEntityId, TimeStep, Landed.VictimId, Landed.HitGroup);
		}
#endif
		// The hits of this frame, for the extensions' AfterExecute point (the scratch array lives on the component until the next attacker step)
		HookContext.AttackerHits = AttackerHits;
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
	Args.StickThresholds = &FrameSet.StickThresholds;
	Args.PausedStart = InSync.Core.HitstopStartFrame;
	Args.PausedEnd = InSync.Core.HitstopEndFrame;
	Args.Outbox = &EventOutbox;
	const FGSTN_InputTickResult Result = FGSTN_InputSimulator::Tick(Args, *this);
	GateCore = nullptr;
	bFrameActionsBlocked = false;
	AbilityStepCancelConditions = FrameCancelConditions;
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

	// A command from a tumble that recovered in the air ends the tumble (owner 2026-11-16): a jump or an air dodge lands as a normal fall,
	// a move that starts a timeline overwrites the state anyway. The knockback move keeps flying until its lifetime, its probe unread outside Tumble
	if (Result.HasRequest())
	{
		FGSTN_CombatVictim::LeaveTumbleByAction(OutSync.Core);
	}

	// A command the gate let through inside a dodge's ending lag is its roll-out (only the kit's recovery commands pass there): the lag ends on this
	// frame, the re-dodge lock keeps running
	if (Result.HasRequest())
	{
		OutSync.Core.DodgeLanding.CutEndLag(Frame);
	}

	// Step 5b: a jump or fast fall command that the gate let through changes the jump state in the frame of the request
	if (Result.HasRequest())
	{
		ApplyJumpRequest(Frame, Result.BindingIndex, OutSync.Core);
	}

#if !UE_BUILD_SHIPPING
	if (Result.HasRequest())
	{
		TraceCombatEvent(EGSTN_CombatTraceEventKind::CommandExecuted, RegisteredEntityId, TimeStep, Result.BindingIndex,
			FrameSet.BindingTimeline.IsValidIndex(Result.BindingIndex) ? FrameSet.BindingTimeline[Result.BindingIndex] : INDEX_NONE);
	}
#endif

	// Step 8: a command that started a timeline ability replaces the running timeline (if any) and begins its own. The ability
	// itself is activated by the input request of this frame, from the ability system's pre-tick
	if (Result.HasRequest() && FrameSet.BindingTimeline.IsValidIndex(Result.BindingIndex) && FrameSet.BindingTimeline[Result.BindingIndex] == INDEX_NONE)
	{
		// A plain ability (a stance, a jump) that the gate let through ends the running timeline: its turn is over
#if !UE_BUILD_SHIPPING
		if (OutSync.Core.ActionState == EGSTN_ActionState::InTimeline)
		{
			TraceCombatEvent(EGSTN_CombatTraceEventKind::Cancel, RegisteredEntityId, TimeStep, OutSync.Core.ActiveTimeline, 0);
		}
#endif
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
		// Seam D-1: the event keys of the running timeline that carry an outbox event, crossed by this frame's evaluation. Written on every machine
		// and every re-simulation, published by the authority only (the cue and the gameplay event of the key stay the ability's)
		FGSTN_CombatTimelineLogic::AppendOutboxEvents(TimelineOutput, Frame, GetOwner(), EventOutbox);
	}
	// Startup steering (plan precisione-weakpoint 5): after the starts of this frame, so a move can steer from its first frame. A hitstop frame
	// stands still like the timeline does
	if (!bHitstopFrame && !bFrameGateClosed)
	{
		StepStartupSteer(Frame, FrameCmd, OutSync.Core, static_cast<float>(TimeStep.StepMS) * 0.001f);
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
	ProjectTimelineTags(InSync.Core, OutSync.Core, Frame);
#if !UE_BUILD_SHIPPING
	// E17: the one trace call of the frame, at the end of the tick; const reads of the state just written
	if (IGSTN_CombatTraceSink* Sink = GSTNCombatTrace::GetSink())
	{
		Sink->OnCombatFrame(GSTNCombatTraceNP::MakeFrame(RegisteredEntityId, Frame, TimeStep.IsResimulating, OutSync.Core));
		TraceLastFrame = Frame;
	}
#endif
	ServiceSync = nullptr;
	ServiceFrame = INDEX_NONE;
	ServiceHookEnvironment = nullptr;
	ServiceTimeStep = nullptr;
	ServiceStepSnapshot = nullptr;
	ServiceView = nullptr;
	bServiceViewResolved = false;
}

void UGSTN_CombatSimComponent::CollectTimelineTags(const FGSTN_CombatSyncCore& Core, int32 Frame, FGameplayTagContainer& OutTags) const
{
	// The same timeline frame the hurt states read (GSTN_CombatSimSnapshot): a hitstop frame has not advanced the timeline, a recoil runs none
	const UGSTN_CombatTableSet* Set = Core.ActionState == EGSTN_ActionState::InTimeline && Core.RecoilStartFrame == INDEX_NONE ? FindTableSet(Core.TableRevision) : nullptr;
	const FGSTN_CombatTimelineData* Timeline = Set ? GetRegistryTimeline(*Set, Core.ActiveTimeline) : nullptr;
	if (!Timeline)
	{
		return;
	}
	const int32 EvaluatedFrame = Core.IsInHitstop(Frame) ? Frame - 1 : Frame;
	const int32 TimelineFrame = FMath::Max(EvaluatedFrame - Core.TimelineStartFrame - Core.TimelinePausedFrames, 0);
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instance : Timeline->Tracks)
	{
		const FGSTN_TimelineTrack* Track = Instance.GetPtr<FGSTN_TimelineTrack>();
		if (Track && Track->GetKind() == EGSTN_TimelineTrackKind::Tags && Track->Contains(TimelineFrame))
		{
			OutTags.AppendTags(static_cast<const FGSTN_TimelineTrack_Tags*>(Track)->Tags);
		}
	}
}

void UGSTN_CombatSimComponent::ProjectTimelineTags(const FGSTN_CombatSyncCore& InCore, const FGSTN_CombatSyncCore& OutCore, int32 Frame)
{
	// Applied as a difference, not set: Combat.Lock.Movement and the command locks are also granted by effects, and a set count would erase
	// them. The difference is a function of the two states only, and the ability system's tag counts are part of its own rolled back sync
	// state (it ticks after this model in the same frame): a restored frame holds the tags of the restored input state, so a re-simulation
	// or a correction applies the right difference with no member remembering what was granted
	if (!AbilitySystem)
	{
		return;
	}
	FGameplayTagContainer Before;
	FGameplayTagContainer After;
	CollectTimelineTags(InCore, Frame - 1, Before);
	CollectTimelineTags(OutCore, Frame, After);
	if (Before.IsEmpty() && After.IsEmpty())
	{
		return;
	}
	for (const FGameplayTag& Tag : Before)
	{
		if (!After.HasTagExact(Tag))
		{
			AbilitySystem->RemoveLooseGameplayTag(Tag);
		}
	}
	for (const FGameplayTag& Tag : After)
	{
		if (!Before.HasTagExact(Tag))
		{
			AbilitySystem->AddLooseGameplayTag(Tag);
		}
	}
}

const FGSTN_StepSnapshot* UGSTN_CombatSimComponent::GetViewSnapshot()
{
	return ServiceTimeStep ? ResolveViewOnce(*ServiceTimeStep, ServiceStepSnapshot) : nullptr;
}

const FGSTN_StepSnapshot* UGSTN_CombatSimComponent::ResolveViewOnce(const FNetSimTimeStep& TimeStep, const FGSTN_StepSnapshot* Shared)
{
	// Outside the hook window (no frame under way) nothing is cached: resolve as before
	if (!ServiceTimeStep || Shared != ServiceStepSnapshot)
	{
		return ResolveAttackerSnapshot(TimeStep, Shared);
	}
	if (!bServiceViewResolved)
	{
		ServiceView = ResolveAttackerSnapshot(TimeStep, Shared);
		bServiceViewResolved = true;
	}
	return ServiceView;
}

void UGSTN_CombatSimComponent::FillHookContext(FGSTN_CombatTickContext& Context, const FGSTN_InputCmd& Cmd, const FVector& Up)
{
	Context.Owner = GetOwner();
	Context.AbilitySystem = AbilitySystem;
	Context.EntityId = RegisteredEntityId;
	Context.Outbox = &EventOutbox;
	Context.Up = Up;
	Context.Stick = FVector2D(FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.StickX), FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.StickY));
	Context.StickOnPlane = FGSTN_CombatVictim::StickToPlane(Context.Stick, FGSTN_DirectionClassifier::DequantizeYaw(Cmd.CameraYaw), Up);
	Context.Buttons = &ButtonMap;
	if (ContextProvider)
	{
		FGameplayTagContainer ContextTags;
		ContextProvider->AppendContextTags(ContextTags);
		Context.bGrounded = ContextTags.HasTagExact(GSTNInputTags::Context_Grounded);
	}
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
	StartTimelineEntry(ServiceSync->Core, ServiceFrame, Set, Entry, ServiceHookEnvironment);
	return true;
}

void UGSTN_CombatSimComponent::StartTimelineEntry(FGSTN_CombatSyncCore& Core, int32 Frame, const UGSTN_CombatTableSet& Set, int32 Entry, const FGSTN_TrackHookEnvironment* Environment)
{
	// What step 8 does for a command, source Forced; the ability system's pre-tick of the frame activates the ability (ActivateEntry), which finds its
	// timeline running from this frame and begins it
	FGSTN_CombatTimelineLogic::Start(Core, Frame, Entry, EGSTN_ActivationSource::Forced);
	StampMoveOrigin(Core, Frame);
	if (const FGSTN_CombatTimelineData* Data = GetRegistryTimeline(Set, Entry))
	{
		FGSTN_CombatTimelineLogic::EvaluateStart(Core, Frame, *Data, Set.TimelineDefaultOverride.IsValidIndex(Entry) ? Set.TimelineDefaultOverride[Entry] : 0, TimelineOutput, Environment);
	}
	Core.Requests.Add(EGSTN_RequestType::ActivateEntry, Frame, static_cast<int16>(Entry));
}

EGSTN_TimelineStartRefusal UGSTN_CombatSimComponent::StartTimelineFromAbility(int32 Entry, bool& bOutCommitDue)
{
	bOutCommitDue = false;
	const UGSTN_CombatTableSet& Set = Tables();
	const FGSTN_CombatTimelineData* Data = GetRegistryTimeline(Set, Entry);
	const FGSTN_CombatSyncState* Pending = NetworkPredictionProxy.ReadSyncState<FGSTN_CombatSyncState>(ENetworkPredictionStateRead::Simulation);
	const EGSTN_TimelineStartRefusal Refusal = FGSTN_TimelinePlayRules::CheckStart(AbilitySystem && Pending && AbilitySystem->IsInSimulationTick(),
		AbilitySystem && AbilitySystem->GetIsRestoringFrame(), bFrameGateClosed, Data != nullptr);
	if (Refusal != EGSTN_TimelineStartRefusal::None)
	{
		UE_LOG(LogGSTNCombatSim, Log, TEXT("%s: timeline entry %d not started from the ability graph (refusal %d)"), *GetNameSafe(GetOwner()), Entry, static_cast<int32>(Refusal));
		return Refusal;
	}

	// The combat tick of this frame ran: its pending state is this frame's output, the one the abilities read in this step
	const int32 Frame = Pending->Core.Input.History.GetLatestFrame();
	const UClass* Starting = Set.TimelineRegistry[Entry].Get();
	const UClass* Replaced = Pending->Core.ActionState == EGSTN_ActionState::InTimeline && Set.TimelineRegistry.IsValidIndex(Pending->Core.ActiveTimeline)
		? Set.TimelineRegistry[Pending->Core.ActiveTimeline].Get() : nullptr;
	FGameplayTagContainer TagsBefore;
	CollectTimelineTags(Pending->Core, Frame, TagsBefore);

	FGSTN_TrackHookEnvironment Environment;
	Environment.Buttons = &ButtonMap;
	if (const UGSTN_MoverSimStateTracker* GroundTracker = GetJumpTracker())
	{
		Environment.Ground = GroundTracker->HasGameplayTag(Mover_IsOnGround, true) ? EGSTN_TrackHookGround::OnGround : EGSTN_TrackHookGround::Airborne;
	}
	const int32 DefaultOverride = Set.TimelineDefaultOverride.IsValidIndex(Entry) ? Set.TimelineDefaultOverride[Entry] : 0;
	const FGSTN_CombatSyncState* Written = NetworkPredictionProxy.WriteSyncState<FGSTN_CombatSyncState>([&](FGSTN_CombatSyncState& Sync)
	{
		Environment.Fragments = &Sync.Fragments;
		// Step 8 of the tick for a command, source Forced. Its CancelTimeline request names a frame whose pre-tick ran: the replaced ability is
		// cancelled below instead, and the request is pruned with the frame
		FGSTN_CombatTimelineLogic::Start(Sync.Core, Frame, Entry, EGSTN_ActivationSource::Forced);
		StampMoveOrigin(Sync.Core, Frame);
		FGSTN_CombatTimelineLogic::EvaluateStart(Sync.Core, Frame, *Data, DefaultOverride, AbilityStartOutput, &Environment);
		bOutCommitDue = Sync.Core.Requests.Find(EGSTN_RequestType::CommitTimeline, Frame, static_cast<int16>(Entry)) != nullptr;
	}, "GSTN AbilityStartTimeline");
	if (!Written)
	{
		return EGSTN_TimelineStartRefusal::OutsideSimulation;
	}

	// The Tags tracks: the tick projected the replaced timeline's (or none) for this frame; the new one's replace them now, so the projection of
	// the next frame, a difference with this state, starts from the counts this state holds
	FGameplayTagContainer TagsAfter;
	CollectTimelineTags(Written->Core, Frame, TagsAfter);
	FGameplayTagContainer Remove;
	FGameplayTagContainer Add;
	FGSTN_TimelinePlayRules::DiffTags(TagsBefore, TagsAfter, Remove, Add);
	for (const FGameplayTag& Tag : Remove)
	{
		AbilitySystem->RemoveLooseGameplayTag(Tag);
	}
	for (const FGameplayTag& Tag : Add)
	{
		AbilitySystem->AddLooseGameplayTag(Tag);
	}
	ProjectStateTags(Written->Core, Frame);

	// A move of another class that ran is over (its ability may be the caller: Start Combat Move from inside a move). The same class is the
	// caller's own sequence (a host playing its next timeline) or a restart into itself, which the activation handles
	if (Replaced && Replaced != Starting)
	{
		const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(const_cast<UClass*>(Replaced));
		if (Spec && Spec->IsActive())
		{
			AbilitySystem->CancelAbilityHandle(Spec->Handle);
		}
	}
	UE_LOG(LogGSTNCombatSim, Verbose, TEXT("%s frame %d: timeline entry %d started from the ability graph (replaced %s)"), *GetNameSafe(GetOwner()), Frame, Entry, *GetNameSafe(Replaced));
	return EGSTN_TimelineStartRefusal::None;
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

	// Per-move movement capsule: size and mesh offset of the presented Mover state, on every machine (simulated proxies run no modifier)
	if (UMoverComponent* Mover = GetOwner() ? GetOwner()->FindComponentByClass<UMoverComponent>() : nullptr)
	{
		FGSTN_BodyCapsuleModifier::ApplyPresentation(*Mover);
	}

#if ENABLE_DRAW_DEBUG
	if (CVarCombatDebugInput.GetValueOnGameThread() != 0)
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		if (Pawn && Pawn->IsLocallyControlled())
		{
			DrawDebugString(GetWorld(), Pawn->GetActorLocation() + FVector(0.0, 0.0, 140.0), GetDebugString(), nullptr, FColor::White, 0.f, true);
		}
	}
	// Debug shapes ride the presented (smoothed) mesh unless gstn.Combat.Debug.FollowMesh 0
	const FVector DebugOffset = (GSTNCombatDebug::IsHitboxDrawOn() || GSTNCombatDebug::IsBodyDrawOn() || GSTNCombatDebug::IsHurtboxDrawOn())
		? GSTNCombatDebug::GetPresentationOffset(GetOwner()) : FVector::ZeroVector;
	if (GSTNCombatDebug::IsHitboxDrawOn())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (AttackerDebugShapes.Num() > 0)
		{
			LingerDebugShapes = AttackerDebugShapes;
			LingerDebugShapesTime = Now;
			GSTNCombatDebug::DrawHitboxes(GetWorld(), AttackerDebugShapes, /*bFaded*/ false, DebugOffset);
		}
		else if (LingerDebugShapesTime >= 0.0 && Now - LingerDebugShapesTime <= GSTNCombatDebug::GetHitboxLingerSeconds())
		{
			GSTNCombatDebug::DrawHitboxes(GetWorld(), LingerDebugShapes, /*bFaded*/ true, DebugOffset);
		}
	}
	if (GSTNCombatDebug::IsBodyDrawOn())
	{
		const UGSTN_CombatWorldSubsystem* Subsystem = WorldSubsystem.Get();
		const FGSTN_SnapshotEntry* Entry = (Subsystem && RegisteredEntityId != INDEX_NONE) ? Subsystem->GetSnapshot().Find(RegisteredEntityId) : nullptr;
		GSTNCombatDebug::DrawBody(GetWorld(), Entry, GetOwner() ? Cast<UCapsuleComponent>(GetOwner()->GetRootComponent()) : nullptr, DebugOffset);
	}
	if (GSTNCombatDebug::IsHurtboxDrawOn() && RegisteredEntityId != INDEX_NONE)
	{
		if (const UGSTN_CombatWorldSubsystem* Subsystem = WorldSubsystem.Get())
		{
			if (const FGSTN_SnapshotEntry* Entry = Subsystem->GetSnapshot().Find(RegisteredEntityId))
			{
				GSTNCombatDebug::DrawHurtboxes(GetWorld(), *Entry, DebugOffset);
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

	// Ground crouch (owner 2026-11-16): this component owns it (FGSTN_GroundCrouchKitExtension through the body capsule modifier). Mover's own
	// stance handling (GASP's crouch input -> FStanceModifier) would resize the same capsule: off, on every machine
	if (bNativeGroundCrouch)
	{
		if (UCharacterMoverComponent* CharacterMover = Cast<UCharacterMoverComponent>(Mover))
		{
			CharacterMover->SetHandleStanceChanges(false);
		}
	}

	// Mover's sync state carries the modifier to the machines that simulate it: the authority and the owning client queue it, the
	// others get it with the state. A simulated proxy only interpolates, so it has nothing to write
	if (Owner->GetLocalRole() != ROLE_Authority && Owner->GetLocalRole() != ROLE_AutonomousProxy)
	{
		bStatsModifierQueued = true;
		return;
	}
	// The run stages' state rides in Mover's sync state; the modes never write it, so it must be persistent to survive them
	FGSTN_RunStageMover::RegisterPersistentState(*Mover);
	// The body capsule modifier (per-move movement capsule, FGSTN_TimelineTrack_BodyCapsule) rides along: same machines, same lifetime.
	// An invalid handle means Mover's simulation does not exist yet (both fail together): try again next tick
	bool bQueued = true;
	if (!Mover->FindMovementModifierByType<FGSTN_StatsMovementModifier>())
	{
		bQueued &= Mover->QueueMovementModifier(MakeShared<FGSTN_StatsMovementModifier>()).IsValid();
	}
	if (!Mover->FindMovementModifierByType<FGSTN_BodyCapsuleModifier>())
	{
		bQueued &= Mover->QueueMovementModifier(MakeShared<FGSTN_BodyCapsuleModifier>()).IsValid();
	}
	bStatsModifierQueued = bQueued;
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
#if !UE_BUILD_SHIPPING
	if (IGSTN_CombatTraceSink* Sink = GSTNCombatTrace::GetSink())
	{
		FGSTN_CombatTraceEvent Event;
		Event.EntityId = RegisteredEntityId;
		Event.Frame = Sync->Core.Input.History.GetLatestFrame();
		Event.Kind = EGSTN_CombatTraceEventKind::Rollback;
		Event.bResimulated = true;
		Event.A = Sync->Core.Input.History.GetLatestFrame() + 1;
		Event.B = TraceLastFrame;
		Sink->OnCombatEvent(Event);
	}
#endif
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
	// The Blueprint events (bp-exposure P1): the confirmed ones from the authority's outbox, before it is published and emptied; the state
	// ones from the presented state, on every machine
	SendConfirmedEvents();
	if (Sync)
	{
		UpdateStateEvents(*Sync);
	}

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

void UGSTN_CombatSimComponent::StepStartupSteer(int32 Frame, const FGSTN_InputCmd& Cmd, FGSTN_CombatSyncCore& Core, float StepSeconds)
{
	if (Core.ActionState != EGSTN_ActionState::InTimeline)
	{
		return;
	}
	const FVector2D AimStick(FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.AimStickX), FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.AimStickY));
	const FVector2D MoveStick(FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.StickX), FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.StickY));
	// Start aim and reverse (owner 2026-11-16: the stick has priority over the facing): before the steering, so the cone starts from the new direction.
	// The move origin is sync state and the stick the frame's command (read at execution, not at a buffered press): every machine and every
	// resimulation turns the same way. The turn itself is a SetFacing request, like the steering's; nothing else steers in that frame
	if (StepOriginAim(Frame, Cmd, MoveStick, AimStick, Core))
	{
		return;
	}
	if (Cmd.AimStickX == 0 && Cmd.AimStickY == 0 && Cmd.StickX == 0 && Cmd.StickY == 0)
	{
		return;
	}
	FTransform Pose;
	FVector Up;
	if (!ReadOwnPose(Pose, Up))
	{
		return;
	}
	// Yaw about the local up, the frame the facing effect writes in (gravity audit 2026-11-16)
	const float CurrentYaw = FGSTN_GravityFrame::YawOfRotator(Pose.Rotator(), Up);
	float SteerYaw = 0.f;
	bool bStickActive = false;
	// Per-move steering (owner 2026-11-14): a turn only while the window is open and the move's stick is pushed; the holds are the input producer's
	const EGSTN_SteerPhase Phase = FGSTN_CombatTimelineLogic::ResolveSteerFrame(Core, Frame, GetRegistryTimeline(Tables(), Core.ActiveTimeline), MoveStick,
		AimStick, Cmd.bPrecisionAim, FGSTN_DirectionClassifier::DequantizeYaw(Cmd.CameraYaw), CurrentYaw, StepSeconds, SteerYaw, bStickActive);
	const int16 YawArg = FGSTN_StartupSteer::QuantizeYaw(SteerYaw);
	if (Phase == EGSTN_SteerPhase::Steering && bStickActive && YawArg != FGSTN_StartupSteer::QuantizeYaw(CurrentYaw))
	{
		if (!Core.Requests.Add(EGSTN_RequestType::SetFacing, Frame, YawArg))
		{
			UE_LOG(LogGSTNCombatSim, Error, TEXT("%s frame %d: no room for the steering request"), *GetOwner()->GetName(), Frame);
		}
	}
}

bool UGSTN_CombatSimComponent::StepOriginAim(int32 Frame, const FGSTN_InputCmd& Cmd, const FVector2D& MoveStick, const FVector2D& AimStick, FGSTN_CombatSyncCore& Core)
{
	const FGSTN_CombatTimelineData* Data = GetRegistryTimeline(Tables(), Core.ActiveTimeline);
	if (!Data || Core.RecoilStartFrame != INDEX_NONE || !Core.Hit.MoveOrigin.bSet)
	{
		return false;
	}
	FGSTN_AimFrameInput In;
	In.TimelineFrame = Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames;
	// The move origin keeps the yaw at 65536 steps per turn in a uint16: the same bits as the request's int16
	In.OriginYawDeg = FGSTN_StartupSteer::DequantizeYaw(static_cast<int16>(Core.Hit.MoveOrigin.RotationQ[1]));
	In.MoveStick = MoveStick;
	In.AimStick = AimStick;
	In.bPrecisionAim = Cmd.bPrecisionAim;
	In.CameraYawDeg = FGSTN_DirectionClassifier::DequantizeYaw(Cmd.CameraYaw);
	In.bStartedByCommand = Core.ActiveSource != EGSTN_ActivationSource::Forced;
	float Yaw = 0.f;
	const EGSTN_AimTurn Turn = FGSTN_StartupSteer::ResolveOriginAim(*Data, In, Yaw);
	if (Turn == EGSTN_AimTurn::None)
	{
		return false;
	}
	const int16 YawArg = FGSTN_StartupSteer::QuantizeYaw(Yaw);
	Core.Hit.MoveOrigin.RotationQ[1] = static_cast<uint16>(YawArg);
	const bool bTurnMomentum = Turn == EGSTN_AimTurn::Reverse && Data->Reverse.Momentum == EGSTN_ReverseMomentum::TurnWithFacing;
	if (!Core.Requests.Add(bTurnMomentum ? EGSTN_RequestType::SetFacingTurnMomentum : EGSTN_RequestType::SetFacing, Frame, YawArg))
	{
		UE_LOG(LogGSTNCombatSim, Error, TEXT("%s frame %d: no room for the start aim request"), *GetOwner()->GetName(), Frame);
	}
	UE_LOG(LogGSTNCombatSim, Verbose, TEXT("%s frame %d: %s to %.0f (timeline frame %d)"), *GetOwner()->GetName(), Frame,
		Turn == EGSTN_AimTurn::Reverse ? TEXT("reverse") : TEXT("start aim"), Yaw, In.TimelineFrame);
	return true;
}

void UGSTN_CombatSimComponent::ExecuteSetFacing(int16 YawArg, bool bTurnMomentum)
{
	if (!AbilitySystem || !SetFacingEffect)
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(SetFacingEffect, 1.f, AbilitySystem->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_FacingYaw, FGSTN_StartupSteer::DequantizeYaw(YawArg));
		Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_FacingTurnVelocity, bTurnMomentum ? 1.f : 0.f);
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

	// A finished move's released montage tail (owner rule 2026-11-15: it plays to its own end) is stopped by the next action. Walking and a hit
	// reaction are actions too (owner PIE report 2026-11-16: after the example kit's 42-frame strong attack, the 4 s takedown montage's root
	// motion overrode the walk for seconds, 0.2 cm a frame with the stick held). From the stick of this frame, inside the ASC tick: deterministic
	float PlayingTime = 0.f;
	if (AbilitySystem && AbilitySystem->GetPlayingSyncedMontage(PlayingTime) && Sync->Core.ActionState != EGSTN_ActionState::InTimeline
		&& (Sync->Core.ActionState != EGSTN_ActionState::Actionable || Sync->Core.Input.History.GetCurrentStick().Zone != EGSTN_StickZone::Neutral))
	{
		UGSTN_GA_BaseCombat::StopReleasedMontages(*AbilitySystem);
	}

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
		case EGSTN_RequestType::SetFacing:
			if (VictimFrame == Frame && bFrameInterrupted)
			{
				// Diagnostics of the react-hook-5 capture: the hit turn reached the pre-tick
				UE_LOG(LogGSTNCombatSim, Verbose, TEXT("[HitTurn] %s pre-tick frame %d SetFacing %.0f actorYaw=%.0f effect=%d"), *GetOwner()->GetName(), Frame,
					FGSTN_StartupSteer::DequantizeYaw(Slot.Arg), GetOwner()->GetActorRotation().Yaw, SetFacingEffect ? 1 : 0);
			}
			ExecuteSetFacing(Slot.Arg);
			break;
		case EGSTN_RequestType::SetFacingTurnMomentum:
			// A reverse that flips the drift (FGSTN_ReverseWindow::Momentum)
			ExecuteSetFacing(Slot.Arg, true);
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

	// The run skid (Mario-style run P2): the stick of this frame reversed against the run of the end of the previous frame, queued before Mover ticks
	StepRunSkid(Sync->Core);

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
			// A host pays for itself (Commit Ability in its graph, as any GAS ability): the timelines it plays never commit it, or a sequence of
			// three would pay three times
			if (Spec && !UGSTN_CombatTableSet::IsHostClass(FrameSet.TimelineRegistry[Sync->Core.ActiveTimeline]))
			{
				// The meter can have dropped since the activation: a refused commit ends the ability (the clock then returns to
				// Actionable on the next frame) instead of letting the timeline run for free
				UGSTN_GA_BaseCombat::CommitOrCancel(*AbilitySystem, *Spec);
			}
		}
	}

	// Paused on every frame the move clock stands still with a montage (also one started during the hitstop), resumed on the first frame
	// after. The frame of the hit runs on like the timeline does, so the montage holds the pose of the timeline's frame through the hitstop.
	// The fork's legacy start tick (the montage one step ahead) is only aligned again after a hit when the pause starts on the hit frame:
	// that pairing is kept as it was. The pause flag only exists with a montage: it is not replicated otherwise
	const bool bPauseFromHitFrame = AbilitySystem->bAdvanceSyncedMontageOnStartTick;
	const bool bMontageStill = bPauseFromHitFrame ? Sync->Core.IsInHitstop(Frame) : FrameClockStill == Frame;
	float MontageTime = 0.f;
	if (bMontageStill && AbilitySystem->GetPlayingSyncedMontage(MontageTime))
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
