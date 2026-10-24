// GAS + NPP + Mover network test harness.

#include "GASNPPTest/GSTN_GASNPPTestComponent.h"

#include "Abilities/NpAbilitySystemComponent.h"
#include "Attributes/GSTN_CombatAttributeSet.h"
#include "Attributes/GSTN_MeterAttributeSet.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "DefaultMovementSet/Modes/SimpleWalkingMode.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Dom/JsonObject.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GASNPPTest/GSTN_TestAttributeSet.h"
#include "Abilities/GSTN_GA_Dash.h"
#include "Abilities/GSTN_GA_PlatformJump.h"
#include "GASNPPTest/GSTN_TestCombatAbilities.h"
#include "GASNPPTest/GSTN_TestEffects.h"
#include "GASNPPTest/GSTN_TestProbeAbilities.h"
#include "GSTN_CombatTags.h"
#include "Input/GSTN_InputScript.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kits/GSTN_CombatAssetTypes.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "MoverComponent.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "World/GSTN_StepSnapshot.h"

UGSTN_GASNPPTestComponent::UGSTN_GASNPPTestComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
	bWantsInitializeComponent = true;
}

void UGSTN_GASNPPTestComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// The harness pawn has no loadout yet, so it gets the combat attribute set here (the ability system creates the listed sets
	// when its simulation state initialises, later than this). The production path is the loadout wiring (C7/C15), which needs the
	// owner's ok on the game mode / PlayerState Blueprints
	if (UNpAbilitySystemComponent* Asc = GetOwner()->FindComponentByClass<UNpAbilitySystemComponent>())
	{
		Asc->DefaultAttributes.AddUnique(UGSTN_CombatAttributeSet::StaticClass());
		// The meters of the C18 scenarios (ex_meter, super_code): the same early creation
		Asc->DefaultAttributes.AddUnique(UGSTN_MeterAttributeSet::StaticClass());
		Asc->GetOrCreateAttributeSubobject_Mutable(UGSTN_MeterAttributeSet::StaticClass());
		// The ability system may have initialised its simulation already (components initialise in their own order): create it
		// now as well, the lookup by class keeps it from being added twice
		Asc->GetOrCreateAttributeSubobject_Mutable(UGSTN_CombatAttributeSet::StaticClass());
	}

	// Before any BeginPlay: the combat component compiles its timeline registry there, and the list must be the same on every machine
	if (UGSTN_CombatSimComponent* Combat = GetOwner()->FindComponentByClass<UGSTN_CombatSimComponent>())
	{
		Combat->AdoptableTimelines.AddUnique(UGSTN_GA_TestTriggeredTimeline::StaticClass());
		Combat->AdoptableTimelines.AddUnique(UGSTN_GA_TestWeaponSwitch::StaticClass());
		// Weapon 0 is the primary, 1 the support: mechanism test effects (real support weapons ship without a stats effect)
		Combat->WeaponStats.Reset();
		Combat->WeaponStats.AddDefaulted_GetRef().Effect = UGSTN_GE_TestPrimaryWeaponStats::StaticClass();
		Combat->WeaponStats.AddDefaulted_GetRef().Effect = UGSTN_GE_TestSupportWeaponStats::StaticClass();
		// Bit 20 is bound to no button of the test input config: the hitstop scenario taps it as a scripted button (every machine alike)
		Combat->HitstopTestButton = 20;
		Combat->HitstopTestFrames = 8;
	}
}

void UGSTN_GASNPPTestComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!TestMappingContext && !DefaultTestMappingContext.IsNull())
	{
		TestMappingContext = DefaultTestMappingContext.LoadSynchronous();
	}
	UE_CLOG(!TestMappingContext, LogTemp, Error, TEXT("%s: no test mapping context (set TestMappingContext or DefaultTestMappingContext in DefaultGame.ini), test input will not work"), *GetName());

	Mover = GetOwner()->FindComponentByClass<UMoverComponent>();
	AbilitySystem = GetOwner()->FindComponentByClass<UNpAbilitySystemComponent>();

	if (GetOwner()->HasAuthority())
	{
		SpawnLocation = GetOwner()->GetActorLocation();
		bHasSpawnLocation = true;
	}

	if (Mover)
	{
		Mover->OnPreSimulationTick.AddDynamic(this, &UGSTN_GASNPPTestComponent::HandlePreSimulationTick);
		Mover->OnPostSimulationRollback.AddDynamic(this, &UGSTN_GASNPPTestComponent::HandlePostSimulationRollback);
	}

	if (AbilitySystem)
	{
		// Machines initialise the ability system and this component in different orders, so the sets would be listed in a different
		// order on each, and the sync state compares them by position. Put them in a fixed order (by class name)
		TArray<UAttributeSet*> Sets = AbilitySystem->GetSpawnedAttributes();
		Sets.Sort([](const UAttributeSet& A, const UAttributeSet& B) { return A.GetClass()->GetName() < B.GetClass()->GetName(); });
		AbilitySystem->SetSpawnedAttributes(Sets);

		AbilitySystem->OnSimulationPreTick.AddUObject(this, &UGSTN_GASNPPTestComponent::HandleAbilityPreTick);
	}

	// The authority publishes each timeline start once (clients get nothing): count them by source, for scenario_triggered_timeline
	if (UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(this))
	{
		TimelineStartedListener = Events->ListenNative(FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNCombatTags::Event_TimelineStarted)),
			[this](const FGSTN_SimEvent& Event)
			{
				if (Event.Instigator.Get() == GetOwner())
				{
					const FName Source = Event.Tags.HasTagExact(GSTNCombatTags::Source_Adopted) ? FName("adopted") : Event.Tags.HasTagExact(GSTNCombatTags::Source_Forced) ? FName("forced") : FName("command");
					++TimelineStartedCounts.FindOrAdd(Source);
				}
			}).Id;
		WeaponSwitchedListener = Events->ListenNative(FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNCombatTags::Event_WeaponSwitched)),
			[this](const FGSTN_SimEvent& Event)
			{
				if (Event.Instigator.Get() == GetOwner())
				{
					++WeaponSwitchedCount;
				}
			}).Id;
	}
}

void UGSTN_GASNPPTestComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Mover)
	{
		Mover->OnPreSimulationTick.RemoveDynamic(this, &UGSTN_GASNPPTestComponent::HandlePreSimulationTick);
		Mover->OnPostSimulationRollback.RemoveDynamic(this, &UGSTN_GASNPPTestComponent::HandlePostSimulationRollback);
	}
	if (AbilitySystem)
	{
		AbilitySystem->OnSimulationPreTick.RemoveAll(this);
	}
	if (TimelineStartedListener != INDEX_NONE)
	{
		if (UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(this))
		{
			FGSTN_SimEventListenerHandle Handle;
			Handle.Id = TimelineStartedListener;
			Events->StopListening(Handle);
		}
		TimelineStartedListener = INDEX_NONE;
	}
	if (WeaponSwitchedListener != INDEX_NONE)
	{
		if (UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(this))
		{
			FGSTN_SimEventListenerHandle Handle;
			Handle.Id = WeaponSwitchedListener;
			Events->StopListening(Handle);
		}
		WeaponSwitchedListener = INDEX_NONE;
	}
	Super::EndPlay(EndPlayReason);
}

void UGSTN_GASNPPTestComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInputReady)
	{
		TrySetupInput();
	}
}

void UGSTN_GASNPPTestComponent::TrySetupInput()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || !AbilitySystem || !TestMappingContext)
	{
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController());
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!LocalPlayer || !PlayerController->InputComponent)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		InputSubsystem->AddMappingContext(TestMappingContext, 10);
	}

	// Refresh actor info now that the controller exists, then bind the ability input actions.
	AbilitySystem->RefreshAvatarActor(GetOwner());
	AbilitySystem->AddMappingContext(TestMappingContext);
	bInputReady = true;
}

UEnhancedInputLocalPlayerSubsystem* UGSTN_GASNPPTestComponent::GetLocalInputSubsystem() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
}

bool UGSTN_GASNPPTestComponent::StartHoldingInput(UInputAction* Action, FVector Value)
{
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetLocalInputSubsystem();
	if (!InputSubsystem || !Action)
	{
		return false;
	}
	InputSubsystem->StartContinuousInputInjectionForAction(Action, Value, {}, {});
	return true;
}

bool UGSTN_GASNPPTestComponent::StopHoldingInput(UInputAction* Action)
{
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetLocalInputSubsystem();
	if (!InputSubsystem || !Action)
	{
		return false;
	}
	InputSubsystem->StopContinuousInputInjectionForAction(Action);
	return true;
}

bool UGSTN_GASNPPTestComponent::PressInput(UInputAction* Action)
{
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetLocalInputSubsystem();
	if (!InputSubsystem || !Action)
	{
		return false;
	}
	InputSubsystem->InjectInputForAction(Action, FInputActionValue(true), {}, {});
	return true;
}

namespace
{
	/** Temporary diagnosis (startup singles, review of the in-window mismatches): logs the Mover mode and the floor check of every
	    pawn when either changes. Off by default: `gstn.test.LogMoverModeChanges 1` in the world to watch. */
	static TAutoConsoleVariable<int32> CVarLogMoverModeChanges(TEXT("gstn.test.LogMoverModeChanges"), 0,
		TEXT("Log Mover mode and floor-hit changes (GASNPP_MODE) and probe activations (GASNPP_ACT) of the test pawns."), ECVF_Default);
}

void UGSTN_GASNPPTestComponent::HandlePreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
	if (CVarLogMoverModeChanges.GetValueOnGameThread() != 0 && Mover)
	{
		FHitResult Floor;
		const bool bFloor = Mover->TryGetFloorCheckHitResult(Floor);
		const FName Mode = Mover->GetMovementModeName();
		if (Mode != LoggedMode || bFloor != bLoggedFloor)
		{
			UE_LOG(LogTemp, Display, TEXT("GASNPP_MODE %s role=%d frame=%d resim=%d mode=%s->%s floor=%d->%d blocking=%d walkable=%d z=%.2f vel=%s"),
				*GetNameSafe(GetOwner()), static_cast<int32>(GetOwner()->GetLocalRole()), TimeStep.ServerFrame, TimeStep.bIsResimulating ? 1 : 0,
				*LoggedMode.ToString(), *Mode.ToString(), bLoggedFloor ? 1 : 0, bFloor ? 1 : 0, Floor.bBlockingHit ? 1 : 0,
				Floor.bBlockingHit && Floor.IsValidBlockingHit() ? 1 : 0, GetOwner()->GetActorLocation().Z, *GetOwner()->GetVelocity().ToCompactString());
			LoggedMode = Mode;
			bLoggedFloor = bFloor;
		}
	}

	{
		FSimHistoryEntry& Entry = SimHistory.AddDefaulted_GetRef();
		Entry.SimTimeMs = TimeStep.BaseSimTimeMs;
		Entry.Location = GetOwner()->GetActorLocation();
		if (Mover)
		{
			Entry.Velocity = Mover->GetVelocity();
			Entry.Mode = Mover->GetMovementModeName();
		}
		if (AbilitySystem)
		{
			FGameplayTagContainer OwnedTags;
			AbilitySystem->GetOwnedGameplayTags(OwnedTags);
			for (const FGameplayTag& Tag : OwnedTags)
			{
				if (Tag.ToString().StartsWith(TEXT("Test.")))
				{
					Entry.Tags.Add(Tag.ToString());
				}
			}
		}
		if (SimHistory.Num() > 4000)
		{
			SimHistory.RemoveAt(0, 2000, EAllowShrinking::No);
		}
	}

	++SimTickCount;
	if (TimeStep.bIsResimulating)
	{
		++ResimTickCount;
		if (AbilitySystem && AbilitySystem->IsReSimulating())
		{
			++AbilityResimTickCount;
		}
	}

	ApplyStatsProbe();
}

void UGSTN_GASNPPTestComponent::HandleAbilityPreTick(const FAbilitySystemTimeStep& TimeStep)
{
	AbilityFrame = TimeStep.ServerFrame;
}

void UGSTN_GASNPPTestComponent::ApplyStatsProbe()
{
	if (!Mover || !AbilitySystem || (!bStatsProbeEnabled && !bStatsBaseCaptured))
	{
		return;
	}

	UCommonLegacyMovementSettings* Settings = Mover->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();
	if (!Settings)
	{
		return;
	}

	if (!bStatsProbeEnabled)
	{
		// Switched off: put back what was captured, once. Speeds are not the probe's: the stats modifier owns them
		Mover->SetGravityOverride(false);
		bStatsBaseCaptured = false;
		AppliedSpeedScale = 1.f;
		AppliedGravityScale = 1.f;
		return;
	}

	if (!bStatsBaseCaptured)
	{
		// Captured once, before the first rewrite: every later tick assigns base * scale, never a delta on the current value
		BaseMaxSpeed = Settings->MaxSpeed;
		BaseGravity = Mover->GetGravityAcceleration();
		bStatsBaseCaptured = true;
	}

	// The ability system ticks before Mover, so this is the attribute value of the frame about to be simulated, restored
	// by a rollback like the rest of the ability system state
	bool bFound = false;
	const float SpeedScale = AbilitySystem->GetGameplayAttributeValue(UGSTN_TestAttributeSet::GetProbeSpeedScaleAttribute(), bFound);
	AppliedSpeedScale = bFound ? SpeedScale : 1.f;
	const float GravityScale = AbilitySystem->GetGameplayAttributeValue(UGSTN_TestAttributeSet::GetProbeGravityScaleAttribute(), bFound);
	AppliedGravityScale = bFound ? GravityScale : 1.f;

	// Only gravity is the probe's. The speeds follow the WalkSpeed stat through the real FGSTN_StatsMovementModifier (the preset
	// effects change it together with ProbeSpeedScale, which the checks use as the expected factor)
	Mover->SetGravityOverride(true, BaseGravity * AppliedGravityScale);
}

void UGSTN_GASNPPTestComponent::HandlePostSimulationRollback(const FMoverTimeStep& CurrentTimeStep, const FMoverTimeStep& ExpungedTimeStep)
{
	++RollbackCount;
	RolledBackTimeMs += FMath::Max(0.0, ExpungedTimeStep.BaseSimTimeMs - CurrentTimeStep.BaseSimTimeMs);
}

void UGSTN_GASNPPTestComponent::ResetCounters()
{
	RollbackCount = 0;
	RolledBackTimeMs = 0.0;
	SimTickCount = 0;
	ResimTickCount = 0;
	AbilityResimTickCount = 0;
	ProbeCounters.Reset();
}

void UGSTN_GASNPPTestComponent::RecordProbeAttempt(FName Probe, bool bRestoring)
{
	FGSTN_ProbeCounters& Counters = ProbeCounters.FindOrAdd(Probe);
	++Counters.Attempts;
	Counters.AttemptsRestoring += bRestoring ? 1 : 0;
}

void UGSTN_GASNPPTestComponent::RecordProbeActivation(FName Probe, bool bResimulating, bool bRestoring)
{
	// Which path produced each activation: role, ability frame, and the flags the counters below are classified with (same cvar as the mode log)
	if (CVarLogMoverModeChanges.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Display, TEXT("GASNPP_ACT %s probe=%s role=%d frame=%d resim=%d restoring=%d"),
			*GetNameSafe(GetOwner()), *Probe.ToString(), static_cast<int32>(GetOwner()->GetLocalRole()), AbilityFrame, bResimulating ? 1 : 0, bRestoring ? 1 : 0);
	}

	FGSTN_ProbeCounters& Counters = ProbeCounters.FindOrAdd(Probe);
	if (bRestoring)
	{
		++Counters.ActivationsRestoring;
		return;
	}
	// The ability system's IsReSimulating() is false on the path that re-runs an activation after a rollback, so the flag
	// alone counts a re-fire as a forward activation. A forward activation always lands on a frame beyond every earlier one;
	// anything at or below LastFrame (the highest activation frame seen) is the simulation replaying a frame.
	if (bResimulating || AbilityFrame <= Counters.LastFrame)
	{
		++Counters.ActivationsResim;
	}
	else
	{
		++Counters.Activations;
	}
	Counters.LastFrame = FMath::Max(Counters.LastFrame, AbilityFrame);
}

void UGSTN_GASNPPTestComponent::RecordProbeEnd(FName Probe)
{
	ProbeCounters.FindOrAdd(Probe).EndFrame = AbilityFrame;
}

namespace
{
	TSubclassOf<UGameplayAbility> ResolveTriggerProbe(const FString& Name)
	{
		if (Name == TEXT("EventRaiser")) { return UGSTN_GA_ProbeEventRaiser::StaticClass(); }
		if (Name == TEXT("EventTarget")) { return UGSTN_GA_ProbeEventTarget::StaticClass(); }
		if (Name == TEXT("TagRaiser")) { return UGSTN_GA_ProbeTagRaiser::StaticClass(); }
		if (Name == TEXT("TagTarget")) { return UGSTN_GA_ProbeTagTarget::StaticClass(); }
		if (Name == TEXT("TimelineTarget")) { return UGSTN_GA_TestTriggeredTimeline::StaticClass(); }
		if (Name == TEXT("SwitchTarget")) { return UGSTN_GA_TestWeaponSwitch::StaticClass(); }
		if (Name == TEXT("Launcher")) { return UGSTN_GA_TestLaunch::StaticClass(); }
		return nullptr;
	}

	/** Clears every spec of the ability system whose class passes Predicate. Returns how many were cleared. */
	template <typename PredicateType>
	int32 ClearAbilitiesWhere(UNpAbilitySystemComponent& AbilitySystem, PredicateType Predicate)
	{
		TArray<FGameplayAbilitySpecHandle> Handles;
		for (const FGameplayAbilitySpec& Spec : AbilitySystem.GetActivatableAbilities())
		{
			if (Spec.Ability && Predicate(Spec.Ability->GetClass()))
			{
				Handles.Add(Spec.Handle);
			}
		}
		for (const FGameplayAbilitySpecHandle& Handle : Handles)
		{
			AbilitySystem.ClearAbility(Handle);
		}
		return Handles.Num();
	}
}

bool UGSTN_GASNPPTestComponent::ServerGrantProbe(const FString& Name)
{
	const TSubclassOf<UGameplayAbility> AbilityClass = ResolveTriggerProbe(Name);
	if (!AbilitySystem || !GetOwner()->HasAuthority() || !AbilityClass || AbilitySystem->FindAbilitySpecFromClass(AbilityClass))
	{
		return false;
	}
	return AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, nullptr)).IsValid();
}

int32 UGSTN_GASNPPTestComponent::ServerRevokeProbes()
{
	if (!AbilitySystem || !GetOwner()->HasAuthority())
	{
		return 0;
	}
	return ClearAbilitiesWhere(*AbilitySystem, [](const UClass* AbilityClass) { return AbilityClass->IsChildOf(UGSTN_ProbeAbilityBase::StaticClass()) || AbilityClass->IsChildOf(UGSTN_GA_TestTriggeredTimeline::StaticClass()) || AbilityClass->IsChildOf(UGSTN_GA_TestWeaponSwitch::StaticClass()) || AbilityClass->IsChildOf(UGSTN_GA_TestLaunch::StaticClass()); });
}

int32 UGSTN_GASNPPTestComponent::ServerSetJumpKit(bool bGrant)
{
	if (!AbilitySystem || !GetOwner()->HasAuthority())
	{
		return 0;
	}
	if (!bGrant)
	{
		return ClearAbilitiesWhere(*AbilitySystem, [](const UClass* AbilityClass) { return AbilityClass->IsChildOf(UGSTN_GA_PlatformJump::StaticClass()); });
	}

	int32 Granted = 0;
	for (const TSubclassOf<UGameplayAbility> AbilityClass : { TSubclassOf<UGameplayAbility>(UGSTN_GA_PlatformJump::StaticClass()), TSubclassOf<UGameplayAbility>(UGSTN_GA_PlatformFastFall::StaticClass()) })
	{
		if (!AbilitySystem->FindAbilitySpecFromClass(AbilityClass))
		{
			Granted += AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, nullptr)).IsValid() ? 1 : 0;
		}
	}
	return Granted;
}

int32 UGSTN_GASNPPTestComponent::ServerSetMoveKit(bool bDash, bool bDodge, bool bInteract)
{
	if (!AbilitySystem || !GetOwner()->HasAuthority())
	{
		return 0;
	}

	const auto Apply = [this](bool bGrant, std::initializer_list<TSubclassOf<UGameplayAbility>> Classes)
	{
		int32 Changed = 0;
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : Classes)
		{
			if (bGrant)
			{
				if (!AbilitySystem->FindAbilitySpecFromClass(AbilityClass))
				{
					Changed += AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, nullptr)).IsValid() ? 1 : 0;
				}
			}
			else
			{
				Changed += ClearAbilitiesWhere(*AbilitySystem, [&AbilityClass](const UClass* Existing) { return Existing == AbilityClass.Get(); });
			}
		}
		return Changed;
	};
	int32 Changed = Apply(bDash, { UGSTN_GA_Dash::StaticClass() });
	Changed += Apply(bDodge, { UGSTN_GA_GroundDodge::StaticClass(), UGSTN_GA_AirDodge::StaticClass(), UGSTN_GA_TestEscapeDodge::StaticClass() });
	Changed += Apply(bInteract, { UGSTN_GA_TestGrab::StaticClass(), UGSTN_GA_TestPickUp::StaticClass() });
	return Changed;
}

bool UGSTN_GASNPPTestComponent::ServerSetHitstun(bool bApply)
{
	if (!AbilitySystem || !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (HitstunEffectHandle.IsValid())
	{
		AbilitySystem->RemoveActiveGameplayEffect(HitstunEffectHandle);
		HitstunEffectHandle = FActiveGameplayEffectHandle();
	}
	if (bApply)
	{
		HitstunEffectHandle = AbilitySystem->ApplyGameplayEffectToSelf(GetDefault<UGSTN_GE_TestHitstun>(), 1.f, AbilitySystem->MakeEffectContext());
	}
	return !bApply || HitstunEffectHandle.IsValid();
}

bool UGSTN_GASNPPTestComponent::ServerSetEscapeKit(bool bApply)
{
	if (!AbilitySystem || !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (EscapeEffectHandle.IsValid())
	{
		AbilitySystem->RemoveActiveGameplayEffect(EscapeEffectHandle);
		EscapeEffectHandle = FActiveGameplayEffectHandle();
	}
	if (bApply)
	{
		EscapeEffectHandle = AbilitySystem->ApplyGameplayEffectToSelf(GetDefault<UGSTN_GE_TestEscapeKit>(), 1.f, AbilitySystem->MakeEffectContext());
	}
	return !bApply || EscapeEffectHandle.IsValid();
}

bool UGSTN_GASNPPTestComponent::SetTestGravity(FVector Acceleration, bool bEnable)
{
	UMoverComponent* GravityMover = GetOwner()->FindComponentByClass<UMoverComponent>();
	if (!GravityMover)
	{
		return false;
	}
	GravityMover->SetGravityOverride(bEnable, bEnable ? Acceleration : FVector::ZeroVector);
	return true;
}

int32 UGSTN_GASNPPTestComponent::ServerGrantBulk(int32 FirstIndex, int32 Count)
{
	const TArray<TSubclassOf<UGameplayAbility>>& Classes = GSTNProbe::GetGrantClasses();
	if (!AbilitySystem || !GetOwner()->HasAuthority() || Classes.IsEmpty())
	{
		return 0;
	}

	int32 Granted = 0;
	const int32 Total = FMath::Min(Count, Classes.Num());
	for (int32 Offset = 0; Offset < Total; ++Offset)
	{
		const int32 Index = ((FirstIndex + Offset) % Classes.Num() + Classes.Num()) % Classes.Num();
		if (!AbilitySystem->FindAbilitySpecFromClass(Classes[Index]))
		{
			Granted += AbilitySystem->GiveAbility(FGameplayAbilitySpec(Classes[Index], 1, INDEX_NONE, nullptr)).IsValid() ? 1 : 0;
		}
	}
	return Granted;
}

int32 UGSTN_GASNPPTestComponent::ServerRevokeBulk()
{
	if (!AbilitySystem || !GetOwner()->HasAuthority())
	{
		return 0;
	}
	return ClearAbilitiesWhere(*AbilitySystem, [](const UClass* AbilityClass) { return AbilityClass->IsChildOf(UGSTN_GA_ProbeGrantBase::StaticClass()); });
}

bool UGSTN_GASNPPTestComponent::ServerSetBulkEffect(bool bApply)
{
	if (!AbilitySystem || !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (BulkEffectHandle.IsValid())
	{
		AbilitySystem->RemoveActiveGameplayEffect(BulkEffectHandle);
		BulkEffectHandle = FActiveGameplayEffectHandle();
	}
	if (bApply)
	{
		BulkEffectHandle = AbilitySystem->ApplyGameplayEffectToSelf(GetDefault<UGSTN_GE_ProbeBulk>(), 1.f, AbilitySystem->MakeEffectContext());
	}
	return !bApply || BulkEffectHandle.IsValid();
}

bool UGSTN_GASNPPTestComponent::ServerEquipPrimaryWeapon(bool bEquip)
{
	UGSTN_CombatSimComponent* Combat = GetOwner()->FindComponentByClass<UGSTN_CombatSimComponent>();
	if (!AbilitySystem || !Combat || !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (bEquip)
	{
		return FGSTN_WeaponStatsSwap::Execute(*AbilitySystem, Combat->WeaponStats, INDEX_NONE, 0);
	}
	int32 Removed = 0;
	for (const FGSTN_WeaponStatsEntry& Weapon : Combat->WeaponStats)
	{
		Removed += UGSTN_GE_WeaponStats::Remove(*AbilitySystem, Weapon.Effect);
	}
	return true;
}

bool UGSTN_GASNPPTestComponent::ServerResetToSpawn(FVector Offset)
{
	if (!Mover || !bHasSpawnLocation || !GetOwner()->HasAuthority())
	{
		return false;
	}
	TSharedPtr<FTeleportEffect> Teleport = MakeShared<FTeleportEffect>();
	Teleport->TargetLocation = SpawnLocation + Offset;
	// The spawn is a little above the floor and a lane may sit on other ground: stand the capsule on the floor under the lane, or the pawn would be
	// left floating in Walking (the mode does not look for a floor while it stands still) and the first move of the scenario would start with a fall
	FHitResult Floor;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GASNPPTestReset), false, GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(Floor, Teleport->TargetLocation + FVector(0.0, 0.0, 50.0), Teleport->TargetLocation - FVector(0.0, 0.0, 600.0), ECC_Visibility, Params))
	{
		Teleport->TargetLocation.Z = Floor.ImpactPoint.Z + GetOwner()->GetSimpleCollisionHalfHeight() + 1.0;
	}
	Teleport->bUseActorRotation = true;
	Mover->QueueInstantMovementEffect(Teleport);

	return true;
}

bool UGSTN_GASNPPTestComponent::ServerResetTestState(FVector Offset)
{
	if (!ServerResetToSpawn(Offset))
	{
		return false;
	}
	// The teleport keeps the velocity, and a pawn left in the air would land wherever it was going. The mode is not forced: a forced Walking leaves the
	// pawn without the floor result of a mode that arrived by itself and the first ground move of the scenario (a dodge) did nothing; the harness
	// waits until the sim reports Walking at rest instead
	TSharedPtr<FApplyVelocityEffect> Stop = MakeShared<FApplyVelocityEffect>();
	Stop->VelocityToApply = FVector::ZeroVector;
	Stop->bAdditiveVelocity = false;
	Mover->QueueInstantMovementEffect(Stop);

	ServerRevokeProbes();
	ServerRevokeBulk();
	ServerSetBulkEffect(false);
	ServerSetStatsPreset(0);
	ServerSetHitstun(false);
	ServerSetEscapeKit(false);
	ServerEquipPrimaryWeapon(false);
	bStatsProbeEnabled = false;
	if (AbilitySystem)
	{
		// Only a meter that is not empty: a write is an out-of-band change that the owner reconciles
		for (const FGameplayAttribute& Meter : { UGSTN_MeterAttributeSet::GetExMeterAttribute(), UGSTN_MeterAttributeSet::GetSuperMeterAttribute() })
		{
			if (AbilitySystem->HasAttributeSetForAttribute(Meter) && AbilitySystem->GetNumericAttribute(Meter) > 0.f)
			{
				AbilitySystem->SetNumericAttributeBase(Meter, 0.f);
			}
		}
	}
	return true;
}

FString UGSTN_GASNPPTestComponent::ReadSnapshotEntry(int32 Frame)
{
	UGSTN_CombatSimComponent* Combat = GetOwner()->FindComponentByClass<UGSTN_CombatSimComponent>();
	const FGSTN_CombatSyncState* State = Combat ? Combat->GetSyncState() : nullptr;
	if (!State)
	{
		return FString();
	}
	FGSTN_SnapshotEntry Entry;
	Entry.Hurtboxes = Combat->HurtboxSet.LoadSynchronous();
	const int32 ReadFrame = Frame >= 0 ? Frame : State->Core.Input.History.GetLatestFrame();
	if (!Combat->ReadSnapshotEntry(ReadFrame, Entry))
	{
		return FString();
	}
	auto Vec = [](const FVector& V) { return TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z) }; };
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("frame"), ReadFrame);
	Json->SetArrayField(TEXT("loc"), Vec(Entry.Transform.GetLocation()));
	Json->SetArrayField(TEXT("vel"), Vec(Entry.Velocity));
	Json->SetArrayField(TEXT("up"), Vec(Entry.Up));
	Json->SetArrayField(TEXT("facing"), Vec(Entry.Facing));
	Json->SetBoolField(TEXT("inHitstop"), Entry.bInHitstop);
	Json->SetNumberField(TEXT("actionState"), static_cast<int32>(Entry.ActionState));
	TArray<TSharedPtr<FJsonValue>> Hurt;
	for (const FGSTN_HurtboxFrameState& H : Entry.HurtStates)
	{
		Hurt.Add(MakeShared<FJsonValueNumber>(static_cast<int32>(H.State)));
	}
	Json->SetArrayField(TEXT("hurtStates"), Hurt);
	Json->SetNumberField(TEXT("reactionFlags"), Entry.Reaction.Flags);
	Json->SetNumberField(TEXT("damagePercent"), Entry.Reaction.GetDamagePercent());
	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Json, Writer);
	return Out;
}

UGSTN_LoadoutComponent* UGSTN_GASNPPTestComponent::EnsureLoadoutComponent()
{
	// A loadout component wired into the test character Blueprint wins (it grants on the owner's own ability system)
	if (UGSTN_LoadoutComponent* OnPawn = GetOwner()->FindComponentByClass<UGSTN_LoadoutComponent>())
	{
		return OnPawn;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	if (!PlayerState)
	{
		return nullptr;
	}
	if (UGSTN_LoadoutComponent* Found = PlayerState->FindComponentByClass<UGSTN_LoadoutComponent>())
	{
		return Found;
	}
	if (!PlayerState->HasAuthority())
	{
		return nullptr;
	}
	// A replicated component made at runtime: the PlayerState Blueprint is shared and stays as it is
	UGSTN_LoadoutComponent* Created = NewObject<UGSTN_LoadoutComponent>(PlayerState, TEXT("GSTNTestLoadout"));
	Created->SetIsReplicated(true);
	Created->RegisterComponent();
	return Created;
}

FString UGSTN_GASNPPTestComponent::ServerApplyTestLoadout(const FString& KitName, const FString& SupportWeaponName)
{
	return ServerApplyTestLoadoutWithSupers(KitName, SupportWeaponName, FString());
}

bool UGSTN_GASNPPTestComponent::ServerGrantMeter(int32 Which)
{
	if (!AbilitySystem || !GetOwner()->HasAuthority())
	{
		return false;
	}
	const UGameplayEffect* Effect = Which == 0 ? static_cast<const UGameplayEffect*>(GetDefault<UGSTN_GE_TestExMeterGrant>()) : GetDefault<UGSTN_GE_TestSuperMeterGrant>();
	// Instant: the handle of the application is not valid by design
	AbilitySystem->ApplyGameplayEffectToSelf(Effect, 1.f, AbilitySystem->MakeEffectContext());
	return true;
}

FString UGSTN_GASNPPTestComponent::ServerApplyTestLoadoutWithSupers(const FString& KitName, const FString& SupportWeaponName, const FString& SuperNamesCsv)
{
	UGSTN_CombatSimComponent* Combat = GetOwner()->FindComponentByClass<UGSTN_CombatSimComponent>();
	UGSTN_LoadoutComponent* Loadout = EnsureLoadoutComponent();
	if (!Combat || !Loadout || !GetOwner()->HasAuthority())
	{
		return TEXT("no combat component, no loadout component or not the authority");
	}

	FGSTN_LoadoutData Data;
	Data.PrimaryKit = FPrimaryAssetId(GSTNCombatAssetTypes::PrimaryKit(), FName(*KitName));
	if (!SupportWeaponName.IsEmpty())
	{
		Data.AdditionalWeapons.Add(FPrimaryAssetId(GSTNCombatAssetTypes::Weapon(), FName(*SupportWeaponName)));
	}
	TArray<FString> SuperNames;
	SuperNamesCsv.ParseIntoArray(SuperNames, TEXT(","), true);
	for (const FString& SuperName : SuperNames)
	{
		Data.Supers.Add(FPrimaryAssetId(GSTNCombatAssetTypes::Super(), FName(*SuperName.TrimStartAndEnd())));
	}
	TArray<FString> Errors;
	if (!Combat->ServerApplyLoadoutChange(Loadout, Data, Errors))
	{
		return FString::Join(Errors, TEXT(" | "));
	}
	return TEXT("OK");
}

bool UGSTN_GASNPPTestComponent::ServerSetStatsPreset(int32 Preset)
{
	if (!AbilitySystem || !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (StatsEffectHandle.IsValid())
	{
		AbilitySystem->RemoveActiveGameplayEffect(StatsEffectHandle);
		StatsEffectHandle = FActiveGameplayEffectHandle();
	}

	const UGameplayEffect* Effect = nullptr;
	if (Preset == 1)
	{
		Effect = GetDefault<UGSTN_GE_ProbeStatsFast>();
	}
	else if (Preset == 2)
	{
		Effect = GetDefault<UGSTN_GE_ProbeStatsSlow>();
	}
	if (Effect)
	{
		StatsEffectHandle = AbilitySystem->ApplyGameplayEffectToSelf(Effect, 1.f, AbilitySystem->MakeEffectContext());
	}
	return !Effect || StatsEffectHandle.IsValid();
}

int32 UGSTN_GASNPPTestComponent::CountOwnedBulkSpecs() const
{
	if (!AbilitySystem)
	{
		return 0;
	}
	int32 Owned = 0;
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : GSTNProbe::GetGrantClasses())
	{
		Owned += AbilitySystem->FindAbilitySpecFromClass(AbilityClass) ? 1 : 0;
	}
	return Owned;
}

FString UGSTN_GASNPPTestComponent::DrainSimHistory()
{
	auto Vec = [](const FVector& V) -> TArray<TSharedPtr<FJsonValue>>
	{
		return { MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z) };
	};
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FSimHistoryEntry& Entry : SimHistory)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("t"), Entry.SimTimeMs);
		Json->SetArrayField(TEXT("loc"), Vec(Entry.Location));
		Json->SetArrayField(TEXT("vel"), Vec(Entry.Velocity));
		Json->SetStringField(TEXT("mode"), Entry.Mode.ToString());
		TArray<TSharedPtr<FJsonValue>> Tags;
		for (const FString& Tag : Entry.Tags)
		{
			Tags.Add(MakeShared<FJsonValueString>(Tag));
		}
		Json->SetArrayField(TEXT("tags"), Tags);
		Out.Add(MakeShared<FJsonValueObject>(Json));
	}
	SimHistory.Reset();
	FString Result;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
	FJsonSerializer::Serialize(Out, Writer);
	return Result;
}

FString UGSTN_GASNPPTestComponent::GetStateSnapshot() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	const AActor* Owner = GetOwner();

	Json->SetStringField(TEXT("actor"), Owner->GetName());
	Json->SetStringField(TEXT("role"), UEnum::GetValueAsString(Owner->GetLocalRole()));
	Json->SetNumberField(TEXT("rollbacks"), RollbackCount);
	Json->SetNumberField(TEXT("rolledBackMs"), RolledBackTimeMs);
	Json->SetNumberField(TEXT("simTicks"), SimTickCount);
	Json->SetNumberField(TEXT("resimTicks"), ResimTickCount);
	Json->SetNumberField(TEXT("abilityResimTicks"), AbilityResimTickCount);
	Json->SetBoolField(TEXT("inputReady"), bInputReady);

	const FVector Location = Owner->GetActorLocation();
	Json->SetArrayField(TEXT("location"), { MakeShared<FJsonValueNumber>(Location.X), MakeShared<FJsonValueNumber>(Location.Y), MakeShared<FJsonValueNumber>(Location.Z) });
	Json->SetNumberField(TEXT("yaw"), Owner->GetActorRotation().Yaw);

	if (Mover)
	{
		const FVector Velocity = Mover->GetVelocity();
		Json->SetArrayField(TEXT("velocity"), { MakeShared<FJsonValueNumber>(Velocity.X), MakeShared<FJsonValueNumber>(Velocity.Y), MakeShared<FJsonValueNumber>(Velocity.Z) });
		Json->SetStringField(TEXT("mode"), Mover->GetMovementModeName().ToString());
		Json->SetNumberField(TEXT("simTimeMs"), Mover->GetLastTimeStep().BaseSimTimeMs);
		Json->SetNumberField(TEXT("serverFrame"), Mover->GetLastTimeStep().ServerFrame);
	}

	if (AbilitySystem)
	{
		TSharedRef<FJsonObject> Attributes = MakeShared<FJsonObject>();
		for (const FGameplayAttribute& Attribute : { UGSTN_TestAttributeSet::GetHealthAttribute(), UGSTN_TestAttributeSet::GetStaminaAttribute(),
			UGSTN_TestAttributeSet::GetProbeEventCountAttribute(), UGSTN_TestAttributeSet::GetProbeTagCountAttribute(),
			UGSTN_TestAttributeSet::GetProbeSpeedScaleAttribute(), UGSTN_TestAttributeSet::GetProbeGravityScaleAttribute() })
		{
			bool bFound = false;
			const float Value = AbilitySystem->GetGameplayAttributeValue(Attribute, bFound);
			if (bFound)
			{
				Attributes->SetNumberField(Attribute.GetName(), Value);
			}
		}
		Json->SetObjectField(TEXT("attributes"), Attributes);

		FGameplayTagContainer OwnedTags;
		AbilitySystem->GetOwnedGameplayTags(OwnedTags);
		TArray<TSharedPtr<FJsonValue>> Tags;
		for (const FGameplayTag& Tag : OwnedTags)
		{
			if (Tag.ToString().StartsWith(TEXT("Test.")))
			{
				Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
			}
		}
		Json->SetArrayField(TEXT("tags"), Tags);

		TArray<TSharedPtr<FJsonValue>> Effects;
		for (const FActiveGameplayEffectHandle& Handle : AbilitySystem->GetActiveEffects(FGameplayEffectQuery()))
		{
			if (const FActiveGameplayEffect* Effect = AbilitySystem->GetActiveGameplayEffect(Handle))
			{
				Effects.Add(MakeShared<FJsonValueString>(GetNameSafe(Effect->Spec.Def)));
			}
		}
		Json->SetArrayField(TEXT("effects"), Effects);

		TSharedRef<FJsonObject> Cooldowns = MakeShared<FJsonObject>();
		for (const FGameplayTag& CooldownTag : { GSTNTestTags::Cooldown_Dash.GetTag(), GSTNTestTags::Cooldown_Buff.GetTag() })
		{
			const TArray<float> Remaining = AbilitySystem->GetActiveEffectsTimeRemaining(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CooldownTag)));
			Cooldowns->SetNumberField(CooldownTag.ToString(), Remaining.Num() ? Remaining[0] : 0.f);
		}
		Json->SetObjectField(TEXT("cooldowns"), Cooldowns);
		Json->SetBoolField(TEXT("abilityResimulating"), AbilitySystem->IsReSimulating());

		// M3 probes. Counters are per machine and not rolled back (see FGSTN_ProbeCounters); the rest is read from the
		// ability system and Mover as they are now.
		TSharedRef<FJsonObject> ProbeJson = MakeShared<FJsonObject>();
		ProbeJson->SetNumberField(TEXT("ownedBulkSpecs"), CountOwnedBulkSpecs());
		ProbeJson->SetNumberField(TEXT("specs"), AbilitySystem->GetActivatableAbilities().Num());

		TArray<FString> ActiveNames;
		for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.IsActive())
			{
				ActiveNames.Add(Spec.Ability->GetClass()->GetName());
			}
		}
		ActiveNames.Sort();
		TArray<TSharedPtr<FJsonValue>> ActiveJson;
		for (const FString& ActiveName : ActiveNames)
		{
			ActiveJson.Add(MakeShared<FJsonValueString>(ActiveName));
		}
		ProbeJson->SetArrayField(TEXT("activeAbilities"), ActiveJson);

		TSharedRef<FJsonObject> TriggersJson = MakeShared<FJsonObject>();
		for (const TPair<FName, FGSTN_ProbeCounters>& Pair : ProbeCounters)
		{
			TSharedRef<FJsonObject> CountersJson = MakeShared<FJsonObject>();
			CountersJson->SetNumberField(TEXT("attempts"), Pair.Value.Attempts);
			CountersJson->SetNumberField(TEXT("attemptsRestoring"), Pair.Value.AttemptsRestoring);
			CountersJson->SetNumberField(TEXT("activations"), Pair.Value.Activations);
			CountersJson->SetNumberField(TEXT("activationsResim"), Pair.Value.ActivationsResim);
			CountersJson->SetNumberField(TEXT("activationsRestoring"), Pair.Value.ActivationsRestoring);
			CountersJson->SetNumberField(TEXT("lastFrame"), Pair.Value.LastFrame);
			CountersJson->SetNumberField(TEXT("endFrame"), Pair.Value.EndFrame);
			TriggersJson->SetObjectField(Pair.Key.ToString(), CountersJson);
		}
		ProbeJson->SetObjectField(TEXT("triggers"), TriggersJson);

		TSharedRef<FJsonObject> StatsJson = MakeShared<FJsonObject>();
		StatsJson->SetBoolField(TEXT("enabled"), bStatsProbeEnabled);
		StatsJson->SetNumberField(TEXT("speedScale"), AppliedSpeedScale);
		StatsJson->SetNumberField(TEXT("gravityScale"), AppliedGravityScale);
		if (Mover)
		{
			const UCommonLegacyMovementSettings* Settings = Mover->FindSharedSettings<UCommonLegacyMovementSettings>();
			StatsJson->SetNumberField(TEXT("maxSpeed"), Settings ? Settings->MaxSpeed : -1.f);
			StatsJson->SetNumberField(TEXT("baseMaxSpeed"), BaseMaxSpeed);
			StatsJson->SetNumberField(TEXT("gravityZ"), Mover->GetGravityAcceleration().Z);
			float RunSpeed = -1.f;
			if (const UBaseMovementMode* Walking = Mover->FindMode_Mutable(UBaseMovementMode::StaticClass(), FName(TEXT("Walking"))))
			{
				if (const FFloatProperty* Property = CastField<FFloatProperty>(Walking->GetClass()->FindPropertyByName(TEXT("RunSpeed"))))
				{
					RunSpeed = Property->GetPropertyValue_InContainer(Walking);
				}
			}
			StatsJson->SetNumberField(TEXT("gaitRunSpeed"), RunSpeed);
			bool bWalkFound = false;
			StatsJson->SetNumberField(TEXT("walkSpeedStat"), AbilitySystem->GetGameplayAttributeValue(UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), bWalkFound));
			StatsJson->SetBoolField(TEXT("hasCombatSet"), bWalkFound);
			bool bMeterFound = false;
			StatsJson->SetNumberField(TEXT("exMeter"), AbilitySystem->GetGameplayAttributeValue(UGSTN_MeterAttributeSet::GetExMeterAttribute(), bMeterFound));
			StatsJson->SetNumberField(TEXT("superMeter"), AbilitySystem->GetGameplayAttributeValue(UGSTN_MeterAttributeSet::GetSuperMeterAttribute(), bMeterFound));
			StatsJson->SetBoolField(TEXT("hasMeterSet"), bMeterFound);
			const USimpleWalkingMode* SimpleWalking = Cast<USimpleWalkingMode>(Mover->FindMode_Mutable(UBaseMovementMode::StaticClass(), FName(TEXT("Walking"))));
			StatsJson->SetNumberField(TEXT("maxSpeedOverride"), SimpleWalking ? SimpleWalking->MaxSpeedOverride : -2.f);
		}
		ProbeJson->SetObjectField(TEXT("stats"), StatsJson);
		Json->SetObjectField(TEXT("probe"), ProbeJson);
	}

	if (const UGSTN_CombatSimComponent* Combat = Owner->FindComponentByClass<UGSTN_CombatSimComponent>())
	{
		if (const FGSTN_CombatSyncState* CombatState = Combat->GetSyncState())
		{
			// The input history in notation, oldest first: server and client must show the same newest events
			TSharedRef<FJsonObject> CombatJson = MakeShared<FJsonObject>();
			CombatJson->SetStringField(TEXT("history"), FGSTN_InputScript::Describe(CombatState->Core.Input.History, 8));
			CombatJson->SetNumberField(TEXT("heldButtons"), CombatState->Core.Input.History.GetHeldButtons());
			CombatJson->SetNumberField(TEXT("historyFrame"), CombatState->Core.Input.History.GetLatestFrame());
			TSharedRef<FJsonObject> StartedJson = MakeShared<FJsonObject>();
			for (const TPair<FName, int32>& Pair : TimelineStartedCounts)
			{
				StartedJson->SetNumberField(Pair.Key.ToString(), Pair.Value);
			}
			CombatJson->SetObjectField(TEXT("timelineStarted"), StartedJson);
			CombatJson->SetNumberField(TEXT("weaponSwitched"), WeaponSwitchedCount);
			CombatJson->SetNumberField(TEXT("activeWeapon"), CombatState->Core.ActiveWeaponIndex);
			{
				const FGSTN_JumpState& Jump = CombatState->Core.Jump;
				TSharedRef<FJsonObject> JumpJson = MakeShared<FJsonObject>();
				JumpJson->SetNumberField(TEXT("phase"), static_cast<int32>(Jump.Phase));
				JumpJson->SetNumberField(TEXT("startFrame"), Jump.StartFrame);
				JumpJson->SetBoolField(TEXT("cut"), Jump.bJumpCut);
				JumpJson->SetBoolField(TEXT("fastFalling"), Jump.bFastFalling);
				JumpJson->SetNumberField(TEXT("airJumpsUsed"), Jump.AirJumpsUsed);
				JumpJson->SetNumberField(TEXT("coyote"), Jump.CoyoteFramesLeft);
				if (const UMoverComponent* GravityMover = Owner->FindComponentByClass<UMoverComponent>())
				{
					const FVector Gravity = GravityMover->GetGravityAcceleration();
					JumpJson->SetArrayField(TEXT("gravity"), { MakeShared<FJsonValueNumber>(Gravity.X), MakeShared<FJsonValueNumber>(Gravity.Y), MakeShared<FJsonValueNumber>(Gravity.Z) });
				}
				CombatJson->SetObjectField(TEXT("jump"), JumpJson);
			}
			CombatJson->SetArrayField(TEXT("request"), { MakeShared<FJsonValueNumber>(CombatState->Core.Input.RequestedBinding), MakeShared<FJsonValueNumber>(CombatState->Core.Input.RequestFrame) });
			CombatJson->SetNumberField(TEXT("pausedTotal"), CombatState->Core.PausedFramesTotal);
			CombatJson->SetNumberField(TEXT("timelinePaused"), CombatState->Core.TimelinePausedFrames);
			CombatJson->SetNumberField(TEXT("loadoutRevision"), CombatState->Core.LoadoutRevision);
			// The table set the simulation is on: the revision it swapped to, its checksum and size, and the frames the gate ran closed
			CombatJson->SetNumberField(TEXT("tableRevision"), CombatState->Core.TableRevision);
			if (const UGSTN_CombatTableSet* TableSet = Combat->FindTableSet(CombatState->Core.TableRevision))
			{
				CombatJson->SetStringField(TEXT("tableCrc"), FString::Printf(TEXT("%08x"), TableSet->Checksum));
				CombatJson->SetNumberField(TEXT("tableBindings"), TableSet->Table.Bindings.Num());
			}
			CombatJson->SetNumberField(TEXT("tableGateClosed"), Combat->GetGateClosedFrames());
			CombatJson->SetBoolField(TEXT("inTimeline"), CombatState->Core.ActionState == EGSTN_ActionState::InTimeline);
			CombatJson->SetNumberField(TEXT("timeline"), CombatState->Core.ActiveTimeline);
			Json->SetObjectField(TEXT("combat"), CombatJson);
		}
	}

	// The loadout of the PlayerState, on every machine that has the component (the server made it, the clients replicate it)
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		{
			const APlayerState* PlayerState = Pawn->GetPlayerState();
			const UGSTN_LoadoutComponent* Loadout = GetOwner()->FindComponentByClass<UGSTN_LoadoutComponent>();
			if (!Loadout && PlayerState)
			{
				Loadout = PlayerState->FindComponentByClass<UGSTN_LoadoutComponent>();
			}
			if (Loadout)
			{
				const TSharedRef<FJsonObject> LoadoutJson = MakeShared<FJsonObject>();
				LoadoutJson->SetNumberField(TEXT("revision"), Loadout->GetRevision());
				LoadoutJson->SetStringField(TEXT("kit"), Loadout->GetApplied().PrimaryKit.PrimaryAssetName.ToString());
				TArray<TSharedPtr<FJsonValue>> Weapons;
				for (const FPrimaryAssetId& Id : Loadout->GetApplied().AdditionalWeapons)
				{
					Weapons.Add(MakeShared<FJsonValueString>(Id.PrimaryAssetName.ToString()));
				}
				LoadoutJson->SetArrayField(TEXT("additionalWeapons"), Weapons);
				LoadoutJson->SetBoolField(TEXT("kitResolved"), Loadout->GetResolvedKit() != nullptr);
				LoadoutJson->SetNumberField(TEXT("tableEntries"), Loadout->GetCompiledTable().Entries.Num());
				LoadoutJson->SetNumberField(TEXT("grantedAbilities"), Loadout->GetCompiledTable().GrantedAbilities.Num());
				// The table bytes are the same on every machine for the same loadout: compare the checksums
				TArray<uint8> TableBytes;
				Loadout->GetCompiledTable().WriteBytes(TableBytes);
				LoadoutJson->SetStringField(TEXT("tableCrc"), FString::Printf(TEXT("%08x"), FCrc::MemCrc32(TableBytes.GetData(), TableBytes.Num())));
				Json->SetObjectField(TEXT("loadout"), LoadoutJson);
			}
		}
	}

	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Json, Writer);
	return Out;
}
