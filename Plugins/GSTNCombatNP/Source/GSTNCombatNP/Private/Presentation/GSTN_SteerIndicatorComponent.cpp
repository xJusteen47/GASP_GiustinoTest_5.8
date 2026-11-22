#include "Presentation/GSTN_SteerIndicatorComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Simulation/GSTN_CombatSimComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_SteerIndicatorComponent)

namespace GSTNSteerIndicator
{
	static int32 CVarValue = -1;
	static FAutoConsoleVariableRef CVar(TEXT("GSTN.Combat.SteerIndicator"), CVarValue,
		TEXT("Steering arrows of a move's startup (local presentation): -1 = as each move's data says (default), 0 = off, 1 = every steerable move"));

	/** A flat arrow on the ground from Origin along Yaw */
	void DrawArrow(const UWorld* World, const FVector& Origin, float YawDeg, float Length, const FColor& Color, float Thickness)
	{
#if ENABLE_DRAW_DEBUG
		const FVector Tip = Origin + FRotator(0.0, YawDeg, 0.0).Vector() * Length;
		DrawDebugDirectionalArrow(World, Origin, Tip, Length * 0.25f, Color, false, 0.f, SDPG_World, Thickness);
#endif
	}
}

UGSTN_SteerIndicatorComponent::UGSTN_SteerIndicatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);
}

bool UGSTN_SteerIndicatorComponent::ShouldShow(int32 CVarValue, bool bComponentEnabled, bool bMoveShows)
{
	if (CVarValue == 0 || CVarValue == 1)
	{
		return CVarValue == 1;
	}
	return bComponentEnabled && bMoveShows;
}

void UGSTN_SteerIndicatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
#if ENABLE_DRAW_DEBUG
	// Only the attacker sees its own arrows (PvP: the opponent reads the body, not a tuning aid)
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const UGSTN_CombatSimComponent* Sim = Pawn ? Pawn->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	UWorld* World = GetWorld();
	if (!Sim || !World || !Pawn->IsLocallyControlled())
	{
		return;
	}

	UGSTN_CombatSimComponent::FSteerView View;
	const bool bHasView = Sim->GetSteerView(View);
	const int32 Frame = Sim->GetSimulationFrame();
	const bool bSteering = bHasView && (View.Phase == EGSTN_SteerPhase::Steering || View.Phase == EGSTN_SteerPhase::Before);
	const float FacingYaw = static_cast<float>(Pawn->GetActorRotation().Yaw);

	// The lock: the window was open on an earlier frame and is not any more
	if (bWasSteering && !bSteering && Frame != LastSeenFrame)
	{
		FlashUntilFrame = Frame + LockFlashFrames;
		FlashYawDeg = FacingYaw;
	}
	if (Frame != LastSeenFrame)
	{
		bWasSteering = bSteering && ShouldShow(GSTNSteerIndicator::CVarValue, bEnabled, View.bShow);
		LastSeenFrame = Frame;
	}

	float Radius = 34.f;
	float HalfHeight = 88.f;
	Pawn->GetSimpleCollisionCylinder(Radius, HalfHeight);
	const FVector Feet = Pawn->GetActorLocation() - FVector(0.0, 0.0, HalfHeight - 3.0);

	if (FlashUntilFrame != INDEX_NONE)
	{
		if (Frame < FlashUntilFrame)
		{
			GSTNSteerIndicator::DrawArrow(World, Feet, FlashYawDeg, ArrowLength, FColor::White, 6.f);
			return;
		}
		FlashUntilFrame = INDEX_NONE;
	}
	if (!bSteering || !ShouldShow(GSTNSteerIndicator::CVarValue, bEnabled, View.bShow))
	{
		return;
	}

	// The allowed arc around the move origin, then the two arrows over it
	const float Cap = FMath::Clamp(View.MaxSteerDegrees, 0.f, 180.f);
	const int32 Segments = FMath::Max(2, FMath::CeilToInt(Cap * 2.f / 8.f));
	FVector Previous = Feet + FRotator(0.0, View.StartYawDeg - Cap, 0.0).Vector() * ArcRadius;
	for (int32 Index = 1; Index <= Segments; ++Index)
	{
		const float Yaw = View.StartYawDeg - Cap + 2.f * Cap * static_cast<float>(Index) / static_cast<float>(Segments);
		const FVector Point = Feet + FRotator(0.0, Yaw, 0.0).Vector() * ArcRadius;
		DrawDebugLine(World, Previous, Point, ArcColor, false, 0.f, SDPG_World, 1.5f);
		Previous = Point;
	}
	if (Cap < 180.f)
	{
		DrawDebugLine(World, Feet, Feet + FRotator(0.0, View.StartYawDeg - Cap, 0.0).Vector() * ArcRadius, ArcColor, false, 0.f, SDPG_World, 1.f);
		DrawDebugLine(World, Feet, Feet + FRotator(0.0, View.StartYawDeg + Cap, 0.0).Vector() * ArcRadius, ArcColor, false, 0.f, SDPG_World, 1.f);
	}
	if (View.bStickActive)
	{
		GSTNSteerIndicator::DrawArrow(World, Feet + FVector(0.0, 0.0, 1.0), View.StickYawDeg, ArrowLength * 1.1f, StickColor, 1.5f);
	}
	GSTNSteerIndicator::DrawArrow(World, Feet + FVector(0.0, 0.0, 2.0), FacingYaw, ArrowLength, FacingColor, 5.f);
#endif
}
