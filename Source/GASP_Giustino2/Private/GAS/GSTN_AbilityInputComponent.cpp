// Gameplay Ability System setup (GAS-NPP fork).

#include "GAS/GSTN_AbilityInputComponent.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "UObject/ConstructorHelpers.h"

UGSTN_AbilityInputComponent::UGSTN_AbilityInputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickInterval = 0.1f;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MappingFinder(TEXT("/Game/GAS/Input/IMC_GAS_Abilities.IMC_GAS_Abilities"));
	if (MappingFinder.Succeeded())
	{
		AbilityMappingContexts.Add(MappingFinder.Object);
	}
}

void UGSTN_AbilityInputComponent::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystem = GetOwner()->FindComponentByClass<UNpAbilitySystemComponent>();
	if (!AbilitySystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: no UNpAbilitySystemComponent on %s, ability input disabled"), *GetName(), *GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
	}
}

void UGSTN_AbilityInputComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Only the locally controlled pawn produces ability input: pawns controlled elsewhere get it through Network
	// Prediction and stop here. Unpossessed pawns keep waiting for their controller.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || Pawn->GetLocalRole() == ROLE_SimulatedProxy || (Pawn->GetController() && !Pawn->IsLocallyControlled()))
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (TryBind())
	{
		SetComponentTickEnabled(false);
	}
}

bool UGSTN_AbilityInputComponent::TryBind()
{
	if (bBound)
	{
		return true;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || !AbilitySystem)
	{
		return false;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController());
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (!LocalPlayer || !PlayerController->InputComponent)
	{
		return false;
	}

	// The controller exists only now: refresh the actor info so the ability system finds it when binding.
	AbilitySystem->RefreshAvatarActor(GetOwner());

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	for (const UInputMappingContext* MappingContext : AbilityMappingContexts)
	{
		if (!MappingContext)
		{
			continue;
		}
		if (bAddToInputSubsystem && InputSubsystem)
		{
			InputSubsystem->AddMappingContext(MappingContext, InputSubsystemPriority);
		}
		AbilitySystem->AddMappingContext(MappingContext);
	}

	bBound = true;
	return true;
}
