// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Animation/GSTN_WallAnimUtils.h"

#include "MoverComponent.h"
#include "WallMovement/GSTN_WallMovementTypes.h"
#include "WallMovement/Modes/GSTN_WallClimbV2MovementMode.h"
#include "WallMovement/Modes/GSTN_WallTraverseModeBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallAnimUtils)

namespace GSTNWallAnim
{
	/** Below this |cross(Up, Normal)| the wall is a floor or a ceiling and has no horizontal axis */
	constexpr float MinAxisLength = 1.e-3f;
}

FGSTN_WallAnimSettings::FGSTN_WallAnimSettings()
	: WallModeClass(UGSTN_WallTraverseModeBase::StaticClass())
{
}

void UGSTN_WallAnimUtils::SplitWallVelocity(const FVector& Velocity, const FVector& WallNormal, const FVector& GravityUp, float& OutLateral, float& OutVertical, float& OutPlaneSpeed)
{
	const FVector Normal = WallNormal.GetSafeNormal();
	const FVector OnPlane = Velocity - Normal * FVector::DotProduct(Velocity, Normal);
	OutPlaneSpeed = OnPlane.Size();

	// Facing the wall, Forward = -Normal and Right = Up ^ Forward (Unreal's Y = Z ^ X) = Normal ^ Up
	const FVector Right = FVector::CrossProduct(Normal, GravityUp);
	if (Right.Size() < GSTNWallAnim::MinAxisLength)
	{
		OutLateral = 0.f;
		OutVertical = 0.f;
		return;
	}

	const FVector RightDir = Right.GetUnsafeNormal();
	const FVector UpOnPlane = FVector::CrossProduct(RightDir, Normal); // Right ^ Normal is the gravity tangent, up positive
	OutLateral = FVector::DotProduct(OnPlane, RightDir);
	OutVertical = FVector::DotProduct(OnPlane, UpOnPlane);
}

EGSTN_WallSlideAnimState UGSTN_WallAnimUtils::ClassifySlide(EGSTN_WallSlideAnimState Previous, float Lateral, float PlaneSpeed, bool bIsClimbing, const FGSTN_WallAnimSettings& Settings)
{
	if (!bIsClimbing || Settings.bSkidWhileClimbing)
	{
		const float SkidThreshold = Previous == EGSTN_WallSlideAnimState::Skid ? Settings.SkidExitSpeed : Settings.SkidEnterSpeed;
		if (PlaneSpeed >= SkidThreshold)
		{
			return EGSTN_WallSlideAnimState::Skid;
		}
	}

	const EGSTN_WallSlideAnimState Side = Lateral >= 0.f ? EGSTN_WallSlideAnimState::SlideRight : EGSTN_WallSlideAnimState::SlideLeft;
	const float LateralThreshold = Previous == Side ? Settings.LateralExitSpeed : Settings.LateralEnterSpeed;
	return FMath::Abs(Lateral) >= LateralThreshold ? Side : EGSTN_WallSlideAnimState::Idle;
}

FGSTN_WallAnimState UGSTN_WallAnimUtils::StepWallAnimState(const FGSTN_WallAnimState& Previous, const FGSTN_WallAnimInputs& Inputs, const FGSTN_WallAnimSettings& Settings, float DeltaSeconds)
{
	FGSTN_WallAnimState State = Previous;
	const float Dt = FMath::Max(DeltaSeconds, 0.f);

	// Phase
	const EGSTN_WallAnimPhase PreviousPhase = Previous.Phase;
	const bool bWasOnWall = PreviousPhase == EGSTN_WallAnimPhase::Enter || PreviousPhase == EGSTN_WallAnimPhase::Active;
	EGSTN_WallAnimPhase NewPhase = PreviousPhase;
	if (Inputs.bOnWall)
	{
		if (!bWasOnWall)
		{
			NewPhase = Settings.EnterDuration > 0.f ? EGSTN_WallAnimPhase::Enter : EGSTN_WallAnimPhase::Active;
		}
		else if (PreviousPhase == EGSTN_WallAnimPhase::Enter && Previous.PhaseTime + Dt >= Settings.EnterDuration)
		{
			NewPhase = EGSTN_WallAnimPhase::Active;
		}
	}
	else if (bWasOnWall)
	{
		NewPhase = Settings.ExitDuration > 0.f ? EGSTN_WallAnimPhase::Exit : EGSTN_WallAnimPhase::None;
		State.ExitModeName = Inputs.ModeName;
	}
	else if (PreviousPhase == EGSTN_WallAnimPhase::Exit && Previous.PhaseTime + Dt >= Settings.ExitDuration)
	{
		NewPhase = EGSTN_WallAnimPhase::None;
	}

	State.PhaseTime = NewPhase == PreviousPhase ? Previous.PhaseTime + Dt : 0.f;
	State.Phase = NewPhase;
	State.bIsActive = NewPhase != EGSTN_WallAnimPhase::None;

	// Loop state and speeds: only measured on the wall, kept as they were after leaving it
	if (Inputs.bOnWall)
	{
		const FVector Normal = Inputs.WallNormal.IsNearlyZero() ? -Inputs.FallbackFacing : Inputs.WallNormal;
		float Lateral = 0.f;
		float Vertical = 0.f;
		float PlaneSpeed = 0.f;
		SplitWallVelocity(Inputs.Velocity, Normal, Inputs.GravityUp, Lateral, Vertical, PlaneSpeed);

		// A fresh grab classifies without the hysteresis of the last attachment
		const EGSTN_WallSlideAnimState PreviousSlide = bWasOnWall ? Previous.SlideState : EGSTN_WallSlideAnimState::Idle;
		const EGSTN_WallSlideAnimState NewSlide = ClassifySlide(PreviousSlide, Lateral, PlaneSpeed, Inputs.bIsClimbing, Settings);

		State.SlideStateTime = (bWasOnWall && NewSlide == Previous.SlideState) ? Previous.SlideStateTime + Dt : 0.f;
		State.SlideState = NewSlide;
		State.bIsClimbing = Inputs.bIsClimbing;
		State.LateralSpeed = Lateral;
		State.VerticalSpeed = Vertical;
		State.PlaneSpeed = PlaneSpeed;
		State.LateralAlpha = FMath::Clamp(Lateral / FMath::Max(Settings.LateralReferenceSpeed, 1.f), -1.f, 1.f);
		State.VerticalAlpha = FMath::Clamp(Vertical / FMath::Max(Settings.VerticalReferenceSpeed, 1.f), -1.f, 1.f);
	}
	else
	{
		State.SlideStateTime = Previous.SlideStateTime + Dt;
	}

	return State;
}

FGSTN_WallAnimInputs UGSTN_WallAnimUtils::GatherInputs(const UMoverComponent* MoverComponent, const FGSTN_WallAnimSettings& Settings)
{
	FGSTN_WallAnimInputs Inputs;
	if (!MoverComponent)
	{
		return Inputs;
	}

	const UBaseMovementMode* Mode = MoverComponent->GetMovementMode();
	const UClass* WallClass = Settings.WallModeClass.Get();
	Inputs.ModeName = MoverComponent->GetMovementModeName();
	Inputs.bOnWall = Mode && WallClass && Mode->IsA(WallClass);
	Inputs.bIsClimbing = Inputs.bOnWall && Mode->IsA<UGSTN_WallClimbV2MovementMode>();
	Inputs.Velocity = MoverComponent->GetVelocity();
	Inputs.GravityUp = MoverComponent->GetUpDirection();
	if (const AActor* Owner = MoverComponent->GetOwner())
	{
		Inputs.FallbackFacing = Owner->GetActorForwardVector();
	}

	if (const FGSTN_WallMovementSyncState* WallState = MoverComponent->GetSyncState().SyncStateCollection.FindDataByType<FGSTN_WallMovementSyncState>())
	{
		if (WallState->bHasWall)
		{
			Inputs.WallNormal = WallState->WallNormal;
		}
	}

	return Inputs;
}
