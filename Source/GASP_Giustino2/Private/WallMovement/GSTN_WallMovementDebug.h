// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

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
 *   gstn.Wall.Launch X Y Z [Add]      Set (or add) the pawn velocity through Mover's ApplyVelocity effect
 *   gstn.Wall.Jump                    Simulate one jump press (wall jump)
 *   gstn.Wall.Detach 1|0              Hold/release the detach button (the GASP pawn overwrites the crouch intent every tick)
 *   gstn.Wall.ResetMemory             UGSTN_WallMovementLibrary::ResetWallMemory (forget the last wall / height cap)
 *   gstn.Wall.CountQueries 0|1        Count the queries (sweeps, traces, overlaps) each tick through GSTN_WALL_QUERY_SCOPE
 *   gstn.Wall.QueryStats [reset]      Log per-mode tick count, average and max queries per tick (and reset with "reset")
 *   gstn.Wall.Metrics 0|1             One log line per tick: time, mode, speed, filtered-normal angle change, queries
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
#define GSTN_WALL_QUERY_SCOPE() GSTNWallDebug::FQueryScope PREPROCESSOR_JOIN(WallQueryScope_, __LINE__)

#else

#define GSTN_WALL_LOG(Level, Format, ...)
#define GSTN_WALL_QUERY_SCOPE()

#endif
