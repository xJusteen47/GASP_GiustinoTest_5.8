#include "Presentation/GSTN_PrecisionAimComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/GSTN_CombatSettings.h"
#include "Input/GSTN_InputConfig.h"
#include "InputAction.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Timeline/GSTN_StartupSteer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PrecisionAimComponent)

UGSTN_PrecisionAimComponent::UGSTN_PrecisionAimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);
}

bool UGSTN_PrecisionAimComponent::IsReticleAllowedIn(const FGameplayTag& Mode)
{
	return Mode.IsValid() && UGSTN_CombatSettings::Get().PrecisionReticleModes.HasTagExact(Mode);
}

float UGSTN_PrecisionAimComponent::AimStickWorldYaw(const FVector2D& AimStick, float CameraYawDeg)
{
	// The same convention as FGSTN_StartupSteer: stick right with the camera at yaw 0 points along +Y (yaw 90)
	return FRotator::NormalizeAxis(CameraYawDeg + FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(AimStick.X, AimStick.Y))));
}

FGameplayTag UGSTN_PrecisionAimComponent::ResolveMatchMode() const
{
	const UWorld* World = GetWorld();
	AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (GameState && GameState->Implements<UGSTN_MatchModeSource>())
	{
		const FGameplayTag Mode = IGSTN_MatchModeSource::Execute_GetMatchMode(GameState);
		if (Mode.IsValid())
		{
			return Mode;
		}
	}
	return UGSTN_CombatSettings::Get().FallbackMatchMode;
}

void UGSTN_PrecisionAimComponent::SetReticleOn(bool bOn)
{
	bReticleOn = bOn && bModeAllowsReticle;
}

bool UGSTN_PrecisionAimComponent::IsLocallyControlledPawn() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled() && Cast<APlayerController>(Pawn->GetController()) != nullptr;
}

void UGSTN_PrecisionAimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsLocallyControlledPawn())
	{
		bReticleOn = false;
		return;
	}
	const APawn* Pawn = CastChecked<APawn>(GetOwner());
	APlayerController* Controller = CastChecked<APlayerController>(Pawn->GetController());
	if (!Combat)
	{
		Combat = GetOwner()->FindComponentByClass<UGSTN_CombatSimComponent>();
	}
	const UGSTN_InputConfig* Config = Combat ? Combat->GetActiveInputConfig() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());
	const UEnhancedPlayerInput* PlayerInput = Subsystem ? Subsystem->GetPlayerInput() : nullptr;

	bModeAllowsReticle = IsReticleAllowedIn(ResolveMatchMode());
	if (!bModeAllowsReticle)
	{
		bReticleOn = false;
	}

	// The reticle button: a press (not a hold) flips it
	if (const UInputAction* Toggle = Config && PlayerInput ? Config->GetReticleToggleAction() : nullptr)
	{
		const bool bHeld = PlayerInput->GetActionValue(Toggle).Get<bool>();
		if (bHeld && !bToggleWasHeld)
		{
			SetReticleOn(!bReticleOn);
		}
		bToggleWasHeld = bHeld;
	}

#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	const FRotator ControlRotation = Controller->GetControlRotation();

	// The aim arrow: where the aim stick points, at the feet, while it is out of the dead zone (the zone where it does not steer either)
	if (const UInputAction* AimAction = Config && PlayerInput ? Config->GetAimStickAction() : nullptr)
	{
		const FVector2D Stick = PlayerInput->GetActionValue(AimAction).Get<FVector2D>();
		if (Stick.SizeSquared() >= static_cast<double>(FGSTN_StartupSteer::DeadZone * FGSTN_StartupSteer::DeadZone))
		{
			const float Yaw = AimStickWorldYaw(Stick, static_cast<float>(ControlRotation.Yaw));
			const FVector Feet = Pawn->GetActorLocation() - FVector(0.0, 0.0, Pawn->GetSimpleCollisionHalfHeight() - 5.0);
			const FVector Tip = Feet + FRotator(0.0, Yaw, 0.0).Vector() * ArrowLength;
			DrawDebugDirectionalArrow(World, Feet, Tip, 30.f, FColor(255, 200, 40), false, 0.f, SDPG_World, 3.f);
		}
	}

	// The reticle: the point the camera looks at, marked where the view ray first meets something (or at the trace's end)
	if (IsReticleShown())
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const FVector End = ViewLocation + ViewRotation.Vector() * ReticleTraceDistance;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNPrecisionReticle), false, GetOwner());
		FHitResult Hit;
		const FVector Point = World->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params) ? Hit.ImpactPoint : End;
		const FVector Normal = -ViewRotation.Vector();
		DrawDebugCircle(World, Point, ReticleSize, 24, FColor::White, false, 0.f, SDPG_Foreground, 1.5f, FRotationMatrix(ViewRotation).GetScaledAxis(EAxis::Y),
			FRotationMatrix(ViewRotation).GetScaledAxis(EAxis::Z), false);
		DrawDebugPoint(World, Point + Normal, 4.f, FColor::Red, false, 0.f, SDPG_Foreground);
	}
#endif
}
