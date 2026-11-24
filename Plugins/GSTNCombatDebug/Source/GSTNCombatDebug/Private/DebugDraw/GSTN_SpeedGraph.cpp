#include "DebugDraw/GSTN_SpeedGraph.h"

#include "CanvasItem.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace GSTNSpeedGraph
{
	static TAutoConsoleVariable<int32> CVarSpeedGraph(TEXT("gstn.Debug.SpeedGraph"), 0,
		TEXT("Speed graph of the local pawn, bottom left: horizontal speed over the last seconds, peak, and t50 / t90 of the last start from rest. 0 = off, 1 = on."),
		ECVF_Default);
	static TAutoConsoleVariable<float> CVarWindow(TEXT("gstn.Debug.SpeedGraph.Seconds"), 3.f,
		TEXT("Seconds of history shown by gstn.Debug.SpeedGraph."), ECVF_Default);

	constexpr float RestSpeed = 10.f;
	/** Speed is measured over at least this many seconds: the simulation ticks at 62.5 Hz, so a 60 fps frame now and then shows two
	 *  ticks of movement and a per-frame delta reads a double-speed spike (capture 2026-11-24: peak 1152 at a 576 run, t50 = t90 = 0.75 s). */
	constexpr double SpeedWindow = 0.1;

	struct FState
	{
		TWeakObjectPtr<APawn> Pawn;
		FVector LastLocation = FVector::ZeroVector;
		double LastTime = -1.0;
		TArray<TPair<double, FVector>> Trail; // (world time, location) of the last SpeedWindow seconds
		TArray<FVector2D> History;      // (world time, speed)
		TArray<FVector2D> StartSamples; // (seconds since the start, speed) of the start in progress
		double StartTime = -1.0;
		float T50 = -1.f;
		float T90 = -1.f;
		float Peak = 0.f;
	};
	static FState State;
	static FDelegateHandle DrawHandle;

	static void Draw(UCanvas* Canvas, APlayerController* Controller)
	{
		if (CVarSpeedGraph.GetValueOnGameThread() == 0 || !Canvas || !Controller || !Controller->GetPawn() || !Controller->GetWorld())
		{
			return;
		}
		APawn* Pawn = Controller->GetPawn();
		const double Now = Controller->GetWorld()->GetTimeSeconds();
		const FVector Location = Pawn->GetActorLocation();
		if (State.Pawn.Get() != Pawn || State.LastTime < 0.0 || Now < State.LastTime)
		{
			State = FState();
			State.Pawn = Pawn;
			State.LastLocation = Location;
			State.LastTime = Now;
			return;
		}
		const double Dt = Now - State.LastTime;
		if (Dt <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		State.Trail.Emplace(Now, Location);
		while (State.Trail.Num() > 2 && State.Trail[1].Key <= Now - SpeedWindow)
		{
			State.Trail.RemoveAt(0, EAllowShrinking::No);
		}
		const double Span = Now - State.Trail[0].Key;
		const float Speed = Span > KINDA_SMALL_NUMBER ? static_cast<float>(FVector::Dist2D(Location, State.Trail[0].Value) / Span) : 0.f;
		State.LastLocation = Location;
		State.LastTime = Now;

		const float Window = FMath::Max(CVarWindow.GetValueOnGameThread(), 0.5f);
		State.History.Emplace(Now, Speed);
		while (State.History.Num() > 1 && State.History[0].X < Now - Window)
		{
			State.History.RemoveAt(0, EAllowShrinking::No);
		}
		State.Peak = 0.f;
		for (const FVector2D& Sample : State.History)
		{
			State.Peak = FMath::Max(State.Peak, static_cast<float>(Sample.Y));
		}

		// A start from rest: collect until the speed stops rising for a while, then measure it once
		if (Speed <= RestSpeed)
		{
			State.StartTime = -1.0;
			State.StartSamples.Reset();
		}
		else
		{
			if (State.StartTime < 0.0)
			{
				State.StartTime = State.LastTime - Dt;
				State.StartSamples.Emplace(0.0, 0.0);
			}
			State.StartSamples.Emplace(Now - State.StartTime, Speed);
			float T50 = 0.f;
			float T90 = 0.f;
			if (Now - State.StartTime < 4.0 && FGSTN_SpeedGraph::MeasureStart(State.StartSamples, T50, T90))
			{
				State.T50 = T50;
				State.T90 = T90;
			}
		}

		// Graph box
		const float Width = 420.f;
		const float Height = 130.f;
		const float Left = 40.f;
		const float Top = Canvas->ClipY - Height - 60.f;
		const float Scale = FMath::Max(State.Peak, 100.f) * 1.1f;
		FCanvasTileItem Back(FVector2D(Left, Top), FVector2D(Width, Height), FLinearColor(0.f, 0.f, 0.f, 0.45f));
		Back.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Back);
		for (int32 Index = 1; Index < State.History.Num(); ++Index)
		{
			const FVector2D& A = State.History[Index - 1];
			const FVector2D& B = State.History[Index];
			const auto ToScreen = [&](const FVector2D& S)
			{
				return FVector2D(Left + Width * (1.0 - (Now - S.X) / Window), Top + Height * (1.0 - S.Y / Scale));
			};
			FCanvasLineItem Line(ToScreen(A), ToScreen(B));
			Line.SetColor(FLinearColor(0.3f, 0.9f, 1.f));
			Line.LineThickness = 2.f;
			Canvas->DrawItem(Line);
		}
		UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
		const FString Text = FString::Printf(TEXT("speed %4.0f cm/s   peak %4.0f   last start t50 %s  t90 %s"), Speed, State.Peak,
			State.T50 >= 0.f ? *FString::Printf(TEXT("%.2f s"), State.T50) : TEXT("-"),
			State.T90 >= 0.f ? *FString::Printf(TEXT("%.2f s"), State.T90) : TEXT("-"));
		FCanvasTextItem Label(FVector2D(Left + 6.f, Top + 4.f), FText::FromString(Text), Font, FLinearColor::White);
		Label.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Label);
	}
}

bool FGSTN_SpeedGraph::MeasureStart(const TArray<FVector2D>& Samples, float& OutT50, float& OutT90)
{
	float Top = 0.f;
	for (const FVector2D& Sample : Samples)
	{
		Top = FMath::Max(Top, static_cast<float>(Sample.Y));
	}
	if (Top <= GSTNSpeedGraph::RestSpeed)
	{
		return false;
	}
	OutT50 = -1.f;
	OutT90 = -1.f;
	for (const FVector2D& Sample : Samples)
	{
		if (OutT50 < 0.f && Sample.Y >= 0.5f * Top)
		{
			OutT50 = static_cast<float>(Sample.X);
		}
		if (OutT90 < 0.f && Sample.Y >= 0.9f * Top)
		{
			OutT90 = static_cast<float>(Sample.X);
		}
	}
	return OutT50 >= 0.f && OutT90 >= 0.f;
}

void FGSTN_SpeedGraph::Startup()
{
	GSTNSpeedGraph::DrawHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateStatic(&GSTNSpeedGraph::Draw));
}

void FGSTN_SpeedGraph::Shutdown()
{
	UDebugDrawService::Unregister(GSTNSpeedGraph::DrawHandle);
	GSTNSpeedGraph::DrawHandle.Reset();
	GSTNSpeedGraph::State = GSTNSpeedGraph::FState();
}
