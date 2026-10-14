// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * Debug tooling for UGSTN_SimpleWallMovementMode, compiled out of Shipping builds (or set GSTN_WALL_DEBUG=0).
 * Built to drive and inspect the mode from the console / an MCP client, without a keyboard:
 *
 *   gstn.Wall.Enter [ModeName]        Queue a movement mode (default WallSliding) on every player pawn in PIE/game worlds
 *   gstn.Wall.Log 0|1|2               1 = one line per substep (position, rotation, velocity, probe/filtered normal, adhesion)
 *                                     2 = also every probe sweep and every rotation around the pivot
 *   gstn.Wall.DebugInput "X Y Z"      Replace the wall intent with a world-space vector projected on the wall plane
 *   gstn.Wall.DebugInput "stick R F"  Simulate a camera-relative stick, fed through the real input mapping like the player's keys
 *   gstn.Wall.DebugInput ""           Back to real input
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

	/** Compact vector formatting for log lines */
	FString Vec(const FVector& V);
}

#define GSTN_WALL_LOG(Level, Format, ...) UE_CLOG(GSTNWallDebug::GetLogLevel() >= (Level), LogGSTNWall, Log, Format, ##__VA_ARGS__)

#else

#define GSTN_WALL_LOG(Level, Format, ...)

#endif
