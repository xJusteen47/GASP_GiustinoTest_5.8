// GAS + NPP + Mover network test harness.

#include "GASNPPTest/GSTN_GASNPPTestComponent.h"
#include "GASNPPTest/GSTN_TestAttributeSet.h"
#include "GASNPPTest/GSTN_TestEffects.h"
#include "GASNPPTest/GSTN_TestProbeAbilities.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Input/GSTN_InputScript.h"
#include "MoverComponent.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

UGSTN_GASNPPTestComponent::UGSTN_GASNPPTestComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;

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

	if (Mover)
	{
		Mover->OnPreSimulationTick.AddDynamic(this, &UGSTN_GASNPPTestComponent::HandlePreSimulationTick);
		Mover->OnPostSimulationRollback.AddDynamic(this, &UGSTN_GASNPPTestComponent::HandlePostSimulationRollback);
	}

	if (AbilitySystem)
	{
		AbilitySystem->OnSimulationPreTick.AddUObject(this, &UGSTN_GASNPPTestComponent::HandleAbilityPreTick);
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
		// Switched off: put back what was captured, once
		Settings->MaxSpeed = BaseMaxSpeed;
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

	Settings->MaxSpeed = BaseMaxSpeed * AppliedSpeedScale;
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
	return ClearAbilitiesWhere(*AbilitySystem, [](const UClass* AbilityClass) { return AbilityClass->IsChildOf(UGSTN_ProbeAbilityBase::StaticClass()); });
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
			CombatJson->SetArrayField(TEXT("request"), { MakeShared<FJsonValueNumber>(CombatState->Core.Input.RequestedBinding), MakeShared<FJsonValueNumber>(CombatState->Core.Input.RequestFrame) });
			Json->SetObjectField(TEXT("combat"), CombatJson);
		}
	}

	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Json, Writer);
	return Out;
}
