// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/GSTN_WallMovementDebug.h"

#if GSTN_WALL_DEBUG

#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "WallMovement/MoveLibrary/GSTN_WallMovementLibrary.h"
#include "HAL/IConsoleManager.h"
#include "MoverComponent.h"

#include <atomic>

DEFINE_LOG_CATEGORY(LogGSTNWall);

namespace GSTNWallDebug
{
	static std::atomic<bool> bJumpRequested = false;
	static std::atomic<bool> bDetachHeld = false;

	/** Calls Fn on the Mover component of every player pawn in PIE/game worlds (PlayerId >= 0: only the pawn of the player with that PlayerState id) */
	static void ForEachPlayerMover(TFunctionRef<void(UMoverComponent&)> Fn, int32 PlayerId = -1)
	{
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			UWorld* World = Ctx.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				const APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
				if (PlayerId >= 0 && !(Pawn && Pawn->GetPlayerState() && Pawn->GetPlayerState()->GetPlayerId() == PlayerId))
				{
					continue;
				}
				if (UMoverComponent* Mover = Pawn ? Pawn->FindComponentByClass<UMoverComponent>() : nullptr)
				{
					Fn(*Mover);
				}
			}
		}
	}

	static FAutoConsoleCommand CmdLaunch(TEXT("gstn.Wall.Launch"),
		TEXT("gstn.Wall.Launch X Y Z [Additive=0] [PlayerId=-1]: sets (or adds) the player pawn's velocity through Mover's ApplyVelocity effect (PlayerId: only that player's pawn)"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 3)
			{
				return;
			}
			const FVector Velocity(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2]));
			const bool bAdditive = Args.Num() > 3 && FCString::Atoi(*Args[3]) != 0;
			const int32 PlayerId = Args.Num() > 4 ? FCString::Atoi(*Args[4]) : -1;
			ForEachPlayerMover([&](UMoverComponent& Mover)
			{
				TSharedPtr<FApplyVelocityEffect> Effect = MakeShared<FApplyVelocityEffect>();
				Effect->VelocityToApply = Velocity;
				Effect->bAdditiveVelocity = bAdditive;
				Mover.QueueInstantMovementEffect(Effect);
				UE_LOG(LogGSTNWall, Log, TEXT("Launch %s additive=%d"), *Velocity.ToCompactString(), bAdditive ? 1 : 0);
			}, PlayerId);
		}));

	static FAutoConsoleCommand CmdTeleport(TEXT("gstn.Wall.Teleport"),
		TEXT("gstn.Wall.Teleport X Y Z [Yaw=0] [PlayerId=-1]: teleports the player pawn through Mover's Teleport effect and stops it (A/B harness: lane starts). Run it on every world, as Launch"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 3)
			{
				return;
			}
			const FVector Location(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2]));
			const float Yaw = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.f;
			const int32 PlayerId = Args.Num() > 4 ? FCString::Atoi(*Args[4]) : -1;
			ForEachPlayerMover([&](UMoverComponent& Mover)
			{
				TSharedPtr<FTeleportEffect> Teleport = MakeShared<FTeleportEffect>();
				Teleport->TargetLocation = Location;
				Teleport->bUseActorRotation = false;
				Teleport->TargetRotation = FRotator(0.f, Yaw, 0.f);
				Mover.QueueInstantMovementEffect(Teleport);
				TSharedPtr<FApplyVelocityEffect> Stop = MakeShared<FApplyVelocityEffect>();
				Stop->VelocityToApply = FVector::ZeroVector;
				Stop->bAdditiveVelocity = false;
				Mover.QueueInstantMovementEffect(Stop);
				UE_LOG(LogGSTNWall, Log, TEXT("Teleport %s yaw=%.1f"), *Location.ToCompactString(), Yaw);
			}, PlayerId);
		}));

	static FAutoConsoleCommand CmdJump(TEXT("gstn.Wall.Jump"),
		TEXT("Simulates one jump press for wall modes (wall jump)"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			bJumpRequested = true;
			UE_LOG(LogGSTNWall, Log, TEXT("Jump requested"));
		}));

	static FAutoConsoleCommand CmdDetach(TEXT("gstn.Wall.Detach"),
		TEXT("gstn.Wall.Detach 1|0: holds/releases the wall detach button. Needed because the GASP pawn re-applies its own crouch input every sim tick, overriding Crouch() calls"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			bDetachHeld = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0;
			UE_LOG(LogGSTNWall, Log, TEXT("Detach button %d"), bDetachHeld ? 1 : 0);
		}));

	static FAutoConsoleCommand CmdResetMemory(TEXT("gstn.Wall.ResetMemory"),
		TEXT("Calls UGSTN_WallMovementLibrary::ResetWallMemory on the player pawn"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			ForEachPlayerMover([](UMoverComponent& Mover) { UGSTN_WallMovementLibrary::ResetWallMemory(&Mover); });
		}));

	bool ConsumeJumpRequest()
	{
		return bJumpRequested.exchange(false);
	}

	bool IsDetachHeld()
	{
		return bDetachHeld;
	}

	static TAutoConsoleVariable<int32> CVarCountQueries(TEXT("gstn.Wall.CountQueries"), 0,
		TEXT("1 = count the wall queries of each tick (scoped counter); see gstn.Wall.QueryStats"), ECVF_Cheat);

	static TAutoConsoleVariable<int32> CVarMetrics(TEXT("gstn.Wall.Metrics"), 0,
		TEXT("1 = one log line per wall tick: time, mode, speed, filtered-normal angle change, queries"), ECVF_Cheat);

	namespace
	{
		struct FModeQueryStats
		{
			int32 Ticks = 0;
			int64 Total = 0;
			int32 Max = 0;
		};
		std::atomic<int32> TickQueries = 0;
		FCriticalSection StatsLock;
		TMap<FName, FModeQueryStats> Stats;
	}

	void CountQuery()
	{
		if (CVarCountQueries.GetValueOnAnyThread() != 0)
		{
			++TickQueries;
		}
	}

	int32 GetTickQueryCount()
	{
		return TickQueries.load();
	}

	void FinishTick(FName ModeName, double TimeSeconds, float Speed, float NormalChangeDeg)
	{
		const int32 Queries = TickQueries.exchange(0);
		if (CVarCountQueries.GetValueOnAnyThread() != 0)
		{
			FScopeLock Lock(&StatsLock);
			FModeQueryStats& S = Stats.FindOrAdd(ModeName);
			++S.Ticks;
			S.Total += Queries;
			S.Max = FMath::Max(S.Max, Queries);
		}
		if (CVarMetrics.GetValueOnAnyThread() != 0)
		{
			UE_LOG(LogGSTNWall, Log, TEXT("Metrics t=%.3f mode=%s speed=%.1f dNormal=%.2fdeg queries=%d"), TimeSeconds, *ModeName.ToString(), Speed, NormalChangeDeg, Queries);
		}
	}

	namespace
	{
		std::atomic<int32> NotedEdge = 0;
		std::atomic<int32> NotedEvents = 0;

		struct FMetricsSink
		{
			FCriticalSection Lock;
			/** Timestamp shared by the files of one run; empty = no run open */
			FString RunStamp;
			TMap<FName, FString> Pending;
			TMap<FName, bool> HeaderWritten;
			TMap<FString, TPair<double, FName>> LastSeen;
		};
		FMetricsSink Sink;

		FString MetricsPath(FName Mode, const FString& Stamp)
		{
			return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Metrics") / FString::Printf(TEXT("wallslide-%s-%s.csv"), *Mode.ToString(), *Stamp));
		}

		/** Appends the buffered rows of one mode to its file (caller holds the lock) */
		void WriteMode(FName Mode)
		{
			FString* Text = Sink.Pending.Find(Mode);
			if (!Text || Text->IsEmpty())
			{
				return;
			}
			FString Out;
			bool& bHeader = Sink.HeaderWritten.FindOrAdd(Mode);
			if (!bHeader)
			{
				Out = MetricsCsvHeader() + TEXT("\n");
				bHeader = true;
			}
			Out += *Text;
			Text->Reset();
			FFileHelper::SaveStringToFile(Out, *MetricsPath(Mode, Sink.RunStamp), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		}

		/** Closes the files of the open run on gstn.Wall.Metrics 0 (or 1 again: a new run) and at exit */
		struct FMetricsLifetime
		{
			FMetricsLifetime()
			{
				CVarMetrics->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*) { FlushMetrics(); }));
				FCoreDelegates::OnEnginePreExit.AddLambda([]() { FlushMetrics(); });
			}
		};
		FMetricsLifetime MetricsLifetime;
	}

	FString MetricsSource(const UMoverComponent* Mover)
	{
		const AActor* Owner = Mover ? Mover->GetOwner() : nullptr;
		if (!Owner)
		{
			return TEXT("unknown");
		}
		const TCHAR* Side = TEXT("standalone");
		switch (Owner->GetNetMode())
		{
		case NM_Client: Side = TEXT("client"); break;
		case NM_DedicatedServer: Side = TEXT("server"); break;
		case NM_ListenServer: Side = TEXT("listen"); break;
		default: break;
		}
		return FString::Printf(TEXT("%s@%s"), *Owner->GetName(), Side);
	}

	void FlushMetrics()
	{
		FScopeLock Lock(&Sink.Lock);
		TArray<FName> Modes;
		Sink.Pending.GetKeys(Modes);
		for (const FName Mode : Modes)
		{
			WriteMode(Mode);
		}
		Sink.RunStamp.Reset();
		Sink.Pending.Reset();
		Sink.HeaderWritten.Reset();
		Sink.LastSeen.Reset();
	}

	void NoteEdgeKind(EGSTN_WallEdgeKind Kind)
	{
		NotedEdge = static_cast<int32>(Kind);
	}

	void NoteEvent(uint8 EventMask)
	{
		NotedEvents |= EventMask;
	}

	FString MetricsCsvHeader()
	{
		return TEXT("time,mode,speed,hspeed,vspeed,dnormal_deg,queries,edge_ahead,events,x,y,z,source");
	}

	const TCHAR* EdgeKindName(EGSTN_WallEdgeKind Kind)
	{
		switch (Kind)
		{
		case EGSTN_WallEdgeKind::None: return TEXT("none");
		case EGSTN_WallEdgeKind::Bump: return TEXT("bump");
		case EGSTN_WallEdgeKind::Slope: return TEXT("slope");
		case EGSTN_WallEdgeKind::Concave: return TEXT("concave");
		case EGSTN_WallEdgeKind::Convex: return TEXT("convex");
		case EGSTN_WallEdgeKind::Blocked: return TEXT("blocked");
		}
		return TEXT("none");
	}

	FString EventsToString(uint8 Events)
	{
		TArray<FString> Names;
		if (Events & Ev_Grab) { Names.Add(TEXT("grab")); }
		if (Events & Ev_Exit) { Names.Add(TEXT("exit")); }
		if (Events & Ev_WallLost) { Names.Add(TEXT("wall-lost")); }
		if (Events & Ev_ModeSwitch) { Names.Add(TEXT("mode-switch")); }
		return FString::Join(Names, TEXT("|"));
	}

	FString FormatMetricsRow(const FMetricsRow& Row)
	{
		return FString::Printf(TEXT("%.3f,%s,%.1f,%.1f,%.1f,%.2f,%d,%s,%s,%.1f,%.1f,%.1f,%s"), Row.Time, *Row.Mode.ToString(), Row.Speed, Row.HorizontalSpeed, Row.VerticalSpeed,
			Row.NormalChangeDeg, Row.Queries, EdgeKindName(Row.EdgeAhead), *EventsToString(Row.Events), Row.Location.X, Row.Location.Y, Row.Location.Z, *Row.Source.Replace(TEXT(","), TEXT(";")));
	}

	bool IsGrabTick(bool bHasPrevious, double PrevTime, FName PrevMode, double Time, FName Mode, double MaxGapSeconds)
	{
		if (!bHasPrevious || PrevMode != Mode)
		{
			return true;
		}
		if (Time <= PrevTime)
		{
			return false;
		}
		return Time - PrevTime > MaxGapSeconds;
	}

	void FinishTick(FName ModeName, double TimeSeconds, const FVector& Velocity, const FVector& GravityUp, float NormalChangeDeg, const FVector& Location, const FString& Source)
	{
		const int32 EdgeAhead = NotedEdge.exchange(0);
		uint8 Events = static_cast<uint8>(NotedEvents.exchange(0));
		const int32 Queries = TickQueries.load();
		const float Speed = static_cast<float>(Velocity.Size());
		// Stats and the log line; resets the query counter
		FinishTick(ModeName, TimeSeconds, Speed, NormalChangeDeg);

		if (CVarMetrics.GetValueOnAnyThread() == 0)
		{
			return;
		}
		FMetricsRow Row;
		Row.Time = TimeSeconds;
		Row.Mode = ModeName;
		Row.Speed = Speed;
		Row.VerticalSpeed = static_cast<float>(Velocity | GravityUp);
		Row.HorizontalSpeed = static_cast<float>(FVector::VectorPlaneProject(Velocity, GravityUp).Size());
		Row.NormalChangeDeg = NormalChangeDeg;
		Row.Queries = Queries;
		Row.EdgeAhead = static_cast<EGSTN_WallEdgeKind>(EdgeAhead);
		Row.Location = Location;
		Row.Source = Source;

		FScopeLock Lock(&Sink.Lock);
		if (Sink.RunStamp.IsEmpty())
		{
			Sink.RunStamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
		}
		const TPair<double, FName>* Prev = Sink.LastSeen.Find(Source);
		if (IsGrabTick(Prev != nullptr, Prev ? Prev->Key : 0.0, Prev ? Prev->Value : FName(), TimeSeconds, ModeName))
		{
			Events |= Ev_Grab;
		}
		Sink.LastSeen.Add(Source, TPair<double, FName>(TimeSeconds, ModeName));
		if ((Events & Ev_ModeSwitch) != 0)
		{
			// A switch is also a way out of the mode: reported once, as the switch
			Events &= ~Ev_Exit;
		}
		Row.Events = Events;
		FString& Text = Sink.Pending.FindOrAdd(ModeName);
		Text += FormatMetricsRow(Row) + TEXT("\n");
		if (Text.Len() > 16 * 1024)
		{
			WriteMode(ModeName);
		}
	}

	void GetQueryStats(FName ModeName, int32& OutTicks, double& OutAverage, int32& OutMax)
	{
		FScopeLock Lock(&StatsLock);
		const FModeQueryStats* S = Stats.Find(ModeName);
		OutTicks = S ? S->Ticks : 0;
		OutAverage = (S && S->Ticks > 0) ? double(S->Total) / S->Ticks : 0.0;
		OutMax = S ? S->Max : 0;
	}

	void ResetQueryStats()
	{
		FScopeLock Lock(&StatsLock);
		Stats.Reset();
		TickQueries = 0;
	}

	static FAutoConsoleCommand CmdQueryStats(TEXT("gstn.Wall.QueryStats"),
		TEXT("Logs per-mode wall query stats (ticks, average, max queries per tick); \"reset\" clears them"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			{
				FScopeLock Lock(&StatsLock);
				for (const TPair<FName, FModeQueryStats>& Pair : Stats)
				{
					UE_LOG(LogGSTNWall, Log, TEXT("QueryStats mode=%s ticks=%d avg=%.2f max=%d"), *Pair.Key.ToString(), Pair.Value.Ticks,
						Pair.Value.Ticks > 0 ? double(Pair.Value.Total) / Pair.Value.Ticks : 0.0, Pair.Value.Max);
				}
			}
			if (Args.Num() > 0 && Args[0].Equals(TEXT("reset"), ESearchCase::IgnoreCase))
			{
				ResetQueryStats();
			}
		}));

	static TAutoConsoleVariable<int32> CVarLog(TEXT("gstn.Wall.Log"), 0,
		TEXT("Wall movement logging. 1 = one line per substep, 2 = also every probe sweep and pivot rotation"), ECVF_Cheat);

	static TAutoConsoleVariable<FString> CVarDebugInput(TEXT("gstn.Wall.DebugInput"), TEXT(""),
		TEXT("Overrides the wall input. \"X Y Z\" = world-space vector projected on the wall plane, ")
		TEXT("\"stick R F\" = camera-relative stick fed through the input mapping, empty = real input"), ECVF_Cheat);

	static FAutoConsoleCommand CmdEnter(TEXT("gstn.Wall.Enter"),
		TEXT("Queues a movement mode (default WallSliding) on every player pawn's Mover in PIE/game worlds"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const FName ModeName = Args.Num() > 0 ? FName(*Args[0]) : FName(TEXT("WallSliding"));
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				UWorld* World = Ctx.World();
				if (!World || !World->IsGameWorld())
				{
					continue;
				}
				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				{
					const APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
					if (UMoverComponent* Mover = Pawn ? Pawn->FindComponentByClass<UMoverComponent>() : nullptr)
					{
						Mover->QueueNextMode(ModeName);
						UE_LOG(LogGSTNWall, Log, TEXT("Queued mode %s on %s"), *ModeName.ToString(), *Pawn->GetName());
					}
				}
			}
		}));

	int32 GetLogLevel()
	{
		return CVarLog.GetValueOnAnyThread();
	}

	EInputOverride GetInputOverride(FVector& OutValue)
	{
		const FString Str = CVarDebugInput.GetValueOnAnyThread();
		TArray<FString> Parts;
		Str.ParseIntoArrayWS(Parts);

		if (Parts.Num() >= 1 && Parts[0].Equals(TEXT("stick"), ESearchCase::IgnoreCase))
		{
			OutValue = FVector(Parts.Num() > 1 ? FCString::Atof(*Parts[1]) : 0.f, Parts.Num() > 2 ? FCString::Atof(*Parts[2]) : 0.f, 0.f);
			return EInputOverride::Stick;
		}
		if (Parts.Num() >= 3)
		{
			OutValue = FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
			return EInputOverride::World;
		}
		return EInputOverride::None;
	}

	bool OverrideMoveInput(const FRotator& ControlRotation, const FVector& GravityUp, FVector& InOutMoveInput)
	{
		FVector DebugValue;
		if (GetInputOverride(DebugValue) != EInputOverride::Stick)
		{
			return false;
		}
		const FVector CamForward = FVector::VectorPlaneProject(ControlRotation.Vector(), GravityUp).GetSafeNormal();
		const FVector CamRight = FVector::CrossProduct(GravityUp, CamForward);
		InOutMoveInput = CamRight * DebugValue.X + CamForward * DebugValue.Y;
		return true;
	}

	FString Vec(const FVector& V)
	{
		return FString::Printf(TEXT("(%.2f %.2f %.2f)"), V.X, V.Y, V.Z);
	}
}

#endif // GSTN_WALL_DEBUG
