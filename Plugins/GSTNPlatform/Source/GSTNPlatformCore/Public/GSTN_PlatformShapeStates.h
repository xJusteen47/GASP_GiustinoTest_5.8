// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_PlatformShape.h"
#include "GSTN_PlatformTypes.h"

/**
 * The last change of one shape's state, as a function of the simulation frame (traversal-surfaces.md 4.4).
 *
 * Why a timeline and not a state that is stepped: a client re-simulates frames it already simulated. A shape that went Inactive
 * at frame 100 was solid at frames 90..99, and a re-simulation of 90..99 must find it solid again, exactly as the first simulation
 * did and as the server did. Storing when the change happens and evaluating the state at the frame being simulated gives that for
 * free; a stepped state would stay Inactive during the replay.
 */
struct GSTNPLATFORMCORE_API FGSTN_PlatformShapeTimeline
{
	/** First frame of the latest change; frames before it use bSolidBefore. MIN_int32: no change yet (the target holds for all frames) */
	int32 ChangeFrame = MIN_int32;
	/** First frame the shape is Inactive after a change to off (ChangeFrame + grace with a telegraph, ChangeFrame without) */
	int32 OffFrame = MIN_int32;
	/** Collision before ChangeFrame */
	bool bSolidBefore = true;
	/** What the latest change asked for */
	bool bTargetOn = true;

	/** The state at a simulation frame */
	EGSTN_PlatformShapeState StateAt(int32 Frame) const;
};

/**
 * States of the shapes of one platform: the input (a mask of shapes that should be on, and how the others go away) is stamped with
 * the frame it takes effect at, and every query is "the state at frame F". Pure and deterministic: the same requests at the same
 * frames give the same states on every machine.
 */
struct GSTNPLATFORMCORE_API FGSTN_PlatformShapeStates
{
	/** All shapes in InitialMask on, the others off, for every frame */
	void Reset(uint8 InitialMask);

	/**
	 * Asks for the shapes of DesiredMask to be on from Frame on. A shape that turns on is Active at once (gaining a surface never
	 * hurts the player). A shape that turns off is Breaking for its grace frames (GraceFrames[ShapeId]) then Inactive when DisableAs
	 * is Breaking, Inactive at once otherwise; asking for Inactive while a shape is already Breaking cuts its grace. Returns the
	 * shapes whose target changed
	 */
	uint8 Request(uint8 DesiredMask, EGSTN_PlatformShapeState DisableAs, const int32 (&GraceFrames)[GSTNPlatform::MaxShapes], int32 Frame);

	EGSTN_PlatformShapeState GetState(int32 ShapeId, int32 Frame) const;

	/** Shapes (bit per id) with collision at a frame */
	uint8 GetSolidMask(int32 Frame) const;

	/** Shapes the latest requests want on */
	uint8 GetTargetMask() const;

	const FGSTN_PlatformShapeTimeline& GetTimeline(int32 ShapeId) const { return Timelines[ShapeId]; }

private:
	FGSTN_PlatformShapeTimeline Timelines[GSTNPlatform::MaxShapes];
};
