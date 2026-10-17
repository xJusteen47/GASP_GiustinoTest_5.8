// GAS + NPP + Mover network test harness.

#include "GASNPPTest/GSTN_GASNPPTestComponent.h"
#include "GASNPPTest/GSTN_TestAttributeSet.h"
#include "GASNPPTest/GSTN_TestEffects.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "MoverComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"

UGSTN_GASNPPTestComponent::UGSTN_GASNPPTestComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MappingFinder(TEXT("/Game/GASNPPTest/Input/IMC_GASNPP_Test.IMC_GASNPP_Test"));
	if (MappingFinder.Succeeded())
	{
		TestMappingContext = MappingFinder.Object;
	}
}

void UGSTN_GASNPPTestComponent::BeginPlay()
{
	Super::BeginPlay();

	Mover = GetOwner()->FindComponentByClass<UMoverComponent>();
	AbilitySystem = GetOwner()->FindComponentByClass<UNpAbilitySystemComponent>();

	if (Mover)
	{
		Mover->OnPreSimulationTick.AddDynamic(this, &UGSTN_GASNPPTestComponent::HandlePreSimulationTick);
		Mover->OnPostSimulationRollback.AddDynamic(this, &UGSTN_GASNPPTestComponent::HandlePostSimulationRollback);
	}
}

void UGSTN_GASNPPTestComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Mover)
	{
		Mover->OnPreSimulationTick.RemoveDynamic(this, &UGSTN_GASNPPTestComponent::HandlePreSimulationTick);
		Mover->OnPostSimulationRollback.RemoveDynamic(this, &UGSTN_GASNPPTestComponent::HandlePostSimulationRollback);
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

void UGSTN_GASNPPTestComponent::HandlePreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
	++SimTickCount;
	if (TimeStep.bIsResimulating)
	{
		++ResimTickCount;
		if (AbilitySystem && AbilitySystem->IsReSimulating())
		{
			++AbilityResimTickCount;
		}
	}
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
		for (const FGameplayAttribute& Attribute : { UGSTN_TestAttributeSet::GetHealthAttribute(), UGSTN_TestAttributeSet::GetStaminaAttribute() })
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
	}

	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Json, Writer);
	return Out;
}
