#include "Camera/GSTN_LaunchReadability.h"

#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GSTN_PresentationComponent.h"
#include "HAL/IConsoleManager.h"
#include "MoverComponent.h"
#include "Presentation/GSTN_CombatStateLibrary.h"
#include "Rendering/DrawElements.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_LaunchReadability)

namespace GSTNLaunchReadability
{
	static TAutoConsoleVariable<int32> CVarZoom(TEXT("gstn.Launch.Debug.Zoom"), 1,
		TEXT("1 = the camera backs off for a while to keep a strong launch you caused in frame (players and important enemies when they tumble, small enemies only when flying out); 0 = never"));
	static TAutoConsoleVariable<int32> CVarArrow(TEXT("gstn.Launch.Debug.Arrow"), 1,
		TEXT("1 = an arrow on the screen edge points at every other player out of frame (never at enemies); 0 = no arrows"));

	static FVector VelocityOf(const AActor* Actor)
	{
		if (const UMoverComponent* Mover = Actor ? Actor->FindComponentByClass<UMoverComponent>() : nullptr)
		{
			return Mover->GetVelocity();
		}
		return Actor ? Actor->GetVelocity() : FVector::ZeroVector;
	}

	static EGSTN_LaunchEffectClass ClassOf(const AActor* Actor)
	{
		const UGSTN_PresentationComponent* Presentation = Actor ? Actor->FindComponentByClass<UGSTN_PresentationComponent>() : nullptr;
		return UGSTN_PresentationComponent::ResolveLaunchEffectClass(Actor, Presentation ? Presentation->LaunchEffectClass : EGSTN_LaunchEffectClass::Auto);
	}

	static double TanHalfFOV(float FOVDegrees)
	{
		return FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(static_cast<double>(FOVDegrees), 1.0, 170.0)) * 0.5);
	}
}

// ---- View math ---------------------------------------------------------------------------------------------------------------------------

float FGSTN_LaunchViewMath::PushBackToFit(const FMinimalViewInfo& View, const FVector& Point, float Margin)
{
	const FVector Local = View.Rotation.UnrotateVector(Point - View.Location);
	const double TanH = GSTNLaunchReadability::TanHalfFOV(View.FOV) * FMath::Clamp(static_cast<double>(Margin), 0.05, 1.0);
	const double Aspect = View.AspectRatio > 0.f ? View.AspectRatio : 16.0 / 9.0;
	const double TanV = TanH / Aspect;
	// The forward distance at which the point's sideways and vertical offsets fit the (narrowed) frustum
	const double Needed = FMath::Max(FMath::Abs(Local.Y) / TanH, FMath::Abs(Local.Z) / TanV);
	return static_cast<float>(FMath::Max(Needed - Local.X, 0.0));
}

bool FGSTN_LaunchViewMath::OffscreenArrow(const FMinimalViewInfo& View, const FVector& Point, const FVector2D& ScreenSize, float EdgeInset, FVector2D& OutPosition, FVector2D& OutDirection)
{
	if (ScreenSize.X <= 1.0 || ScreenSize.Y <= 1.0)
	{
		return false;
	}
	const FVector Local = View.Rotation.UnrotateVector(Point - View.Location);
	const double TanH = GSTNLaunchReadability::TanHalfFOV(View.FOV);
	const double TanV = TanH * ScreenSize.Y / ScreenSize.X;
	if (Local.X > 1.0)
	{
		const double NdcX = Local.Y / (Local.X * TanH);
		const double NdcY = Local.Z / (Local.X * TanV);
		if (FMath::Abs(NdcX) <= 1.0 && FMath::Abs(NdcY) <= 1.0)
		{
			return false;
		}
	}
	// Screen space, y down. Behind the camera the sideways and vertical offsets still say which way to turn
	FVector2D Dir(Local.Y, -Local.Z);
	if (Dir.IsNearlyZero())
	{
		Dir = FVector2D(0.0, 1.0);
	}
	Dir.Normalize();
	const FVector2D Half = ScreenSize * 0.5;
	const double Inset = FMath::Clamp(static_cast<double>(EdgeInset), 0.0, FMath::Min(Half.X, Half.Y) - 1.0);
	const double ScaleX = FMath::Abs(Dir.X) > UE_SMALL_NUMBER ? (Half.X - Inset) / FMath::Abs(Dir.X) : TNumericLimits<double>::Max();
	const double ScaleY = FMath::Abs(Dir.Y) > UE_SMALL_NUMBER ? (Half.Y - Inset) / FMath::Abs(Dir.Y) : TNumericLimits<double>::Max();
	OutPosition = Half + Dir * FMath::Min(ScaleX, ScaleY);
	OutDirection = Dir;
	return true;
}

FLinearColor FGSTN_LaunchViewMath::PlayerColor(int32 PlayerNumber)
{
	switch (PlayerNumber)
	{
	case 1: return FLinearColor(0.95f, 0.15f, 0.12f);
	case 2: return FLinearColor(0.15f, 0.40f, 1.00f);
	case 3: return FLinearColor(1.00f, 0.85f, 0.10f);
	case 4: return FLinearColor(0.20f, 0.85f, 0.25f);
	default: return FLinearColor(0.7f, 0.7f, 0.7f);
	}
}

// ---- Arrow widget ------------------------------------------------------------------------------------------------------------------------

int32 UGSTN_OffscreenArrowWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const APlayerController* Controller = GetOwningPlayer();
	const UWorld* World = GetWorld();
	if (GSTNLaunchReadability::CVarArrow.GetValueOnGameThread() == 0 || !Controller || !Controller->PlayerCameraManager || !World)
	{
		return Layer;
	}
	const FMinimalViewInfo View = Controller->PlayerCameraManager->GetCameraCacheView();
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FGSTN_OffscreenArrowTuning& Tuning = UGSTN_LaunchReadabilitySettings::Get().Arrow;
	const AActor* Mine = Controller->GetViewTarget();

	// Player numbers by the alphabetical order of the names (game-design-direction: the same friends keep the same colours)
	TArray<const APlayerState*> Players;
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		for (const APlayerState* State : GameState->PlayerArray)
		{
			if (State)
			{
				Players.Add(State);
			}
		}
	}
	Players.Sort([](const APlayerState& A, const APlayerState& B) { return A.GetPlayerName() < B.GetPlayerName(); });

	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		const APawn* Pawn = Players[Index]->GetPawn();
		if (!Pawn || Pawn == Mine || Pawn == Controller->GetPawn())
		{
			continue;
		}
		FVector2D Position;
		FVector2D Direction;
		if (!FGSTN_LaunchViewMath::OffscreenArrow(View, Pawn->GetActorLocation() + Pawn->GetActorUpVector() * Tuning.AimHeight, Size, Tuning.EdgeInset + Tuning.Size, Position, Direction))
		{
			continue;
		}
		// A filled-looking triangle: its outline plus a line down the middle, tip towards the player
		const FVector2D Side(-Direction.Y, Direction.X);
		const FVector2D Tip = Position + Direction * Tuning.Size;
		const FVector2D Left = Position - Direction * (Tuning.Size * 0.5f) + Side * (Tuning.Size * 0.7f);
		const FVector2D Right = Position - Direction * (Tuning.Size * 0.5f) - Side * (Tuning.Size * 0.7f);
		const TArray<FVector2D> Outline = { Tip, Left, Right, Tip };
		const TArray<FVector2D> Spine = { Tip, Position - Direction * (Tuning.Size * 0.4f) };
		const FLinearColor Color = FGSTN_LaunchViewMath::PlayerColor(Index + 1);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(), Outline, ESlateDrawEffect::None, Color, true, Tuning.Thickness);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(), Spine, ESlateDrawEffect::None, Color, true, Tuning.Thickness * 1.5f);
	}
	return Layer + 1;
}

// ---- Subsystem ---------------------------------------------------------------------------------------------------------------------------

bool UGSTN_LaunchReadabilitySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UGSTN_LaunchReadabilitySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_LaunchReadabilitySubsystem, STATGROUP_Tickables);
}

void UGSTN_LaunchReadabilitySubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || ArrowWidget)
	{
		return;
	}
	// The arrow layer goes on the first local player's screen once it has a controller (split screen would need one per player: not yet)
	APlayerController* Controller = World->GetFirstPlayerController();
	if (Controller && Controller->IsLocalController() && Controller->GetLocalPlayer())
	{
		ArrowWidget = CreateWidget<UGSTN_OffscreenArrowWidget>(Controller, UGSTN_OffscreenArrowWidget::StaticClass());
		if (ArrowWidget)
		{
			ArrowWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			ArrowWidget->AddToPlayerScreen(50);
		}
	}
}

float UGSTN_LaunchReadabilitySubsystem::UpdateZoom(float DeltaTime, const FMinimalViewInfo& View, const AActor* ViewTarget)
{
	using namespace GSTNLaunchReadability;
	const FGSTN_LaunchZoomTuning& Tuning = UGSTN_LaunchReadabilitySettings::Get().Zoom;
	UWorld* World = GetWorld();
	float Target = 0.f;
	if (World && ViewTarget && CVarZoom.GetValueOnGameThread() != 0)
	{
		const double Now = World->GetTimeSeconds();
		// The launch to follow: one this view's character caused, flying the way the class allows, about to leave the frame; the fastest wins
		const AActor* Best = nullptr;
		double BestSpeed = 0.0;
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			const APawn* Pawn = *It;
			if (!Pawn || Pawn == ViewTarget || !UGSTN_CombatStateLibrary::GetCombatComponent(Pawn))
			{
				continue;
			}
			FGSTN_CombatHitEventData Hit;
			if (!UGSTN_CombatStateLibrary::GetLastHitReceived(Pawn, Hit) || Hit.Attacker != ViewTarget)
			{
				continue;
			}
			const FVector Velocity = VelocityOf(Pawn);
			const double Speed = Velocity.Size();
			const EGSTN_LaunchEffectClass Class = ClassOf(Pawn);
			const bool bFlying = Class == EGSTN_LaunchEffectClass::Minor
				? (UGSTN_CombatStateLibrary::IsInTumble(Pawn) || UGSTN_CombatStateLibrary::IsInHitstun(Pawn)) && Speed >= Tuning.MinorEnemySpeed
				: UGSTN_CombatStateLibrary::IsInTumble(Pawn);
			if (!bFlying || Speed <= BestSpeed)
			{
				continue;
			}
			const FVector Ahead = Pawn->GetActorLocation() + Velocity * Tuning.LookaheadSeconds;
			if (FGSTN_LaunchViewMath::PushBackToFit(View, Ahead, Tuning.ScreenMargin) > 0.f)
			{
				Best = Pawn;
				BestSpeed = Speed;
			}
		}
		if (Best)
		{
			Focus = const_cast<AActor*>(Best);
			HoldUntil = Now + Tuning.HoldSeconds;
		}
		const AActor* Followed = Focus.Get();
		if (Followed && Now < HoldUntil)
		{
			// Keep both where it is and where it is heading in frame
			const FVector Here = Followed->GetActorLocation();
			const FVector Ahead = Here + VelocityOf(Followed) * Tuning.LookaheadSeconds;
			Target = FMath::Min(FMath::Max(FGSTN_LaunchViewMath::PushBackToFit(View, Here, Tuning.ScreenMargin), FGSTN_LaunchViewMath::PushBackToFit(View, Ahead, Tuning.ScreenMargin)),
				Tuning.MaxExtraDistance);
		}
		else
		{
			Focus.Reset();
		}
	}
	CurrentExtra = FMath::FInterpTo(CurrentExtra, Target, DeltaTime, Target > CurrentExtra ? Tuning.ZoomOutSpeed : Tuning.ZoomInSpeed);
	if (CurrentExtra < 1.f && Target <= 0.f)
	{
		CurrentExtra = 0.f;
	}
	return CurrentExtra;
}
