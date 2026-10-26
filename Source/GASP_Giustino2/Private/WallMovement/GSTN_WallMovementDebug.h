// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WallMovement/GSTN_WallEdgeTypes.h"

/**
 * Debug tooling for the wall movement modes (UGSTN_WallMovementModeBase and subclasses, UGSTN_WallEntryTransition), compiled out of Shipping builds (or set GSTN_WALL_DEBUG=0).
 * Built to drive and inspect the mode from the console / an MCP client, without a keyboard:
 *
 *   gstn.Wall.Enter [ModeName]        Queue a movement mode (default WallSliding) on every player pawn in PIE/game worlds
 *   gstn.Wall.Log 0|1|2               1 = one line per substep (position, rotation, velocity, probe/filtered normal, adhesion)
 *                                     2 = also every probe sweep and every rotation around the pivot
 *   gstn.Wall.DebugInput "X Y Z"      Replace the wall intent with a world-space vector projected on the wall plane
 *   gstn.Wall.DebugInput "stick R F"  Simulate a camera-relative stick, fed through the real input mapping like the player's keys
 *   gstn.Wall.DebugInput ""           Back to real input
 *   gstn.Wall.Launch X Y Z [Add] [Id] Set (or add) the pawn velocity through Mover's ApplyVelocity effect (Id: only the pawn of that PlayerState id)
 *   gstn.Wall.Teleport X Y Z [Yaw] [Id]  Teleport (and stop) the pawn through Mover's Teleport effect: lane starts of the A/B harness
 *   gstn.Wall.Jump                    Simulate one jump press (wall jump)
 *   gstn.Wall.Detach 1|0              Hold/release the detach button (the GASP pawn overwrites the crouch intent every tick)
 *   gstn.Wall.ResetMemory             UGSTN_WallMovementLibrary::ResetWallMemory (forget the last wall / height cap)
 *   gstn.Wall.CountQueries 0|1        Count the queries (sweeps, traces, overlaps) each tick through GSTN_WALL_QUERY_SCOPE
 *   gstn.Wall.QueryStats [reset]      Log per-mode tick count, average and max queries per tick (and reset with "reset")
 *   gstn.Wall.Metrics 0|1             One log line per tick: time, mode, speed, filtered-normal angle change, queries. Also a CSV per run and mode,
 *                                     Saved/Metrics/wallslide-<mode>-<timestamp>.csv (columns: MetricsCsvHeader); 1 starts a run, 0 closes it
 *                                     (written to disk when it is closed, when 16 KB are buffered and at engine exit)
 *
 * Query counting is explicit: engine SCENE_QUERY_STAT tags cannot be counted cheaply, so code that wants to be counted wraps
 * each query in GSTN_WALL_QUERY_SCOPE(). The existing wall modes and utils are NOT instrumented (they stay untouched for the
 * A/B baseline); the v2 code calls the scope around each of its queries, and a later checkpoint may add it to the old ones.
 * The counter is process-wide (a PIE with several pawns shares it), so read it with one wall pawn ticking.
 */

#ifndef GSTN_WALL_DEBUG
	#define GSTN_WALL_DEBUG !UE_BUILD_SHIPPING
#endif

#if GSTN_WALL_DEBUG

class UMoverComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogGSTNWall, Log, All);

namespace GSTNWallDebug
{
	enum class EInputOverride : uint8
	{
		None,
		/** Value is a world-space intent, projected straight on the wall */
		World,
		/** Value is a stick (X = right, Y = forward), turned into a camera-relative move input */
		Stick,
	};

	/** Current gstn.Wall.Log level */
	int32 GetLogLevel();

	/** Parses gstn.Wall.DebugInput */
	EInputOverride GetInputOverride(FVector& OutValue);

	/**
	 * gstn.Wall.DebugInput "stick R F": replaces InOutMoveInput with the simulated stick, turned into a camera-relative
	 * world move input like the player's keys. Returns false (input untouched) without a stick override
	 */
	bool OverrideMoveInput(const FRotator& ControlRotation, const FVector& GravityUp, FVector& InOutMoveInput);

	/** Compact vector formatting for log lines */
	FString Vec(const FVector& V);

	/** True once after gstn.Wall.Jump */
	bool ConsumeJumpRequest();

	/** True while gstn.Wall.Detach 1 */
	bool IsDetachHeld();

	/** Adds one query to the current tick count (no-op unless gstn.Wall.CountQueries 1) */
	void CountQuery();

	/** Queries counted since the last FinishTick */
	int32 GetTickQueryCount();

	/**
	 * Ends the tick of a wall mode: adds the tick's query count to the stats of ModeName, optionally logs the edge metrics
	 * (gstn.Wall.Metrics) and resets the tick counter. Speed in cm/s; NormalChangeDeg is the filtered wall normal change this tick.
	 */
	void FinishTick(FName ModeName, double TimeSeconds, float Speed, float NormalChangeDeg);

	/** Events of a tick, OR-ed into one CSV column. Grab is derived by the sink (first tick of a source in a mode), the rest are noted by the modes */
	enum EMetricEvent : uint8
	{
		Ev_None = 0,
		Ev_Grab = 1 << 0,
		Ev_Exit = 1 << 1,
		Ev_WallLost = 1 << 2,
		Ev_ModeSwitch = 1 << 3,
	};

	/** One tick of the wall metrics, as written to the CSV */
	struct FMetricsRow
	{
		double Time = 0.0;
		FName Mode;
		/** Total, along the gravity-up axis (signed, up positive) and across it, in cm/s */
		float Speed = 0.f;
		float HorizontalSpeed = 0.f;
		float VerticalSpeed = 0.f;
		float NormalChangeDeg = 0.f;
		int32 Queries = 0;
		EGSTN_WallEdgeKind EdgeAhead = EGSTN_WallEdgeKind::None;
		uint8 Events = Ev_None;
		/** World position of the capsule at the end of the tick (cm): lets a summary place events along a lane */
		FVector Location = FVector::ZeroVector;
		/** Which pawn / net side the tick belongs to (a PIE with a host and clients shares the sink) */
		FString Source;
	};

	/** Header line of the metrics CSV (no newline) */
	FString MetricsCsvHeader();

	/** One CSV line (no newline) for Row; columns as MetricsCsvHeader */
	FString FormatMetricsRow(const FMetricsRow& Row);

	/** "grab|exit" style text of an event mask ("" for none) */
	FString EventsToString(uint8 Events);

	/** Name of an edge kind for the CSV ("none", "bump", "slope", "concave", "convex", "blocked") */
	const TCHAR* EdgeKindName(EGSTN_WallEdgeKind Kind);

	/**
	 * True when a tick is the first of a grab: nothing was seen before from this source, the mode changed, or more than MaxGapSeconds passed
	 * since the previous tick. A tick whose time is not after the previous one is a re-simulation of it, never a grab.
	 */
	bool IsGrabTick(bool bHasPrevious, double PrevTime, FName PrevMode, double Time, FName Mode, double MaxGapSeconds = 0.1);

	/** The mode notes what the look-ahead found this tick (read and reset by the next full FinishTick) */
	void NoteEdgeKind(EGSTN_WallEdgeKind Kind);

	/** The mode notes events of this tick (EMetricEvent mask; read and reset by the next full FinishTick) */
	void NoteEvent(uint8 EventMask);

	/**
	 * End of tick for the CSV of gstn.Wall.Metrics: as the short FinishTick, plus the velocity split along GravityUp, the look-ahead kind and
	 * the events noted during the tick, and the source label. Call this one from the modes.
	 */
	void FinishTick(FName ModeName, double TimeSeconds, const FVector& Velocity, const FVector& GravityUp, float NormalChangeDeg, const FVector& Location, const FString& Source);

	/** Label of the pawn and net side a mover belongs to for the CSV: "<actor name>@<server|client|listen|standalone>" */
	FString MetricsSource(const UMoverComponent* Mover);

	/** Closes the current metrics run: writes what is buffered. The next metrics tick starts a new run (new timestamp) */
	void FlushMetrics();

	/** Stats of a mode since the last reset (zeros when never ticked) */
	void GetQueryStats(FName ModeName, int32& OutTicks, double& OutAverage, int32& OutMax);

	/** Clears all per-mode stats and the tick counter */
	void ResetQueryStats();

	/** RAII marker around one query; use through GSTN_WALL_QUERY_SCOPE */
	struct FQueryScope
	{
		FQueryScope() { CountQuery(); }
	};
}

#define GSTN_WALL_LOG(Level, Format, ...) UE_CLOG(GSTNWallDebug::GetLogLevel() >= (Level), LogGSTNWall, Log, Format, ##__VA_ARGS__)

/** Counts one query for gstn.Wall.CountQueries; place right before the sweep / trace / overlap */
#define GSTN_WALL_QUERY_SCOPE() GSTNWallDebug::FQueryScope UE_JOIN(WallQueryScope_, __LINE__)

#else

#define GSTN_WALL_LOG(Level, Format, ...)
#define GSTN_WALL_QUERY_SCOPE()

#endif
