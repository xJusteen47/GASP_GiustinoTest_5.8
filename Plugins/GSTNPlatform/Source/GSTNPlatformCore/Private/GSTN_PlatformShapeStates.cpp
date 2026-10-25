// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#include "GSTN_PlatformShapeStates.h"

EGSTN_PlatformShapeState FGSTN_PlatformShapeTimeline::StateAt(int32 Frame) const
{
	if (Frame < ChangeFrame)
	{
		return bSolidBefore ? EGSTN_PlatformShapeState::Active : EGSTN_PlatformShapeState::Inactive;
	}
	if (bTargetOn)
	{
		return EGSTN_PlatformShapeState::Active;
	}
	return Frame < OffFrame ? EGSTN_PlatformShapeState::Breaking : EGSTN_PlatformShapeState::Inactive;
}

void FGSTN_PlatformShapeStates::Reset(uint8 InitialMask)
{
	for (int32 Id = 0; Id < GSTNPlatform::MaxShapes; ++Id)
	{
		const bool bOn = (InitialMask & (1u << Id)) != 0;
		FGSTN_PlatformShapeTimeline& Timeline = Timelines[Id];
		Timeline = FGSTN_PlatformShapeTimeline();
		Timeline.bTargetOn = bOn;
		Timeline.bSolidBefore = bOn;
	}
}

uint8 FGSTN_PlatformShapeStates::Request(uint8 DesiredMask, EGSTN_PlatformShapeState DisableAs, const int32 (&GraceFrames)[GSTNPlatform::MaxShapes], int32 Frame)
{
	uint8 Changed = 0;
	for (int32 Id = 0; Id < GSTNPlatform::MaxShapes; ++Id)
	{
		const bool bWant = (DesiredMask & (1u << Id)) != 0;
		FGSTN_PlatformShapeTimeline& Timeline = Timelines[Id];
		if (bWant == Timeline.bTargetOn)
		{
			// Already going off with a telegraph and now asked to go at once: cut the grace from this frame
			if (!bWant && DisableAs == EGSTN_PlatformShapeState::Inactive && Timeline.StateAt(Frame) == EGSTN_PlatformShapeState::Breaking)
			{
				Timeline.OffFrame = FMath::Max(Frame, Timeline.ChangeFrame);
			}
			continue;
		}

		FGSTN_PlatformShapeTimeline Next;
		// Approximation, by design: the frames before this change keep the solidity of the frame just before it. Two changes closer
		// than a grace telegraph lose the older telegraph for frames before the newer change (a replay that far back is a correction anyway)
		Next.bSolidBefore = GSTNPlatform::IsSolid(Timeline.StateAt(Frame == MIN_int32 ? Frame : Frame - 1));
		Next.ChangeFrame = Frame;
		Next.bTargetOn = bWant;
		const int32 Grace = DisableAs == EGSTN_PlatformShapeState::Breaking ? FMath::Max(0, GraceFrames[Id]) : 0;
		Next.OffFrame = bWant ? Frame : (Frame == MIN_int32 ? MIN_int32 : Frame + Grace);
		Timeline = Next;
		Changed |= uint8(1u << Id);
	}
	return Changed;
}

EGSTN_PlatformShapeState FGSTN_PlatformShapeStates::GetState(int32 ShapeId, int32 Frame) const
{
	return ShapeId >= 0 && ShapeId < GSTNPlatform::MaxShapes ? Timelines[ShapeId].StateAt(Frame) : EGSTN_PlatformShapeState::Inactive;
}

uint8 FGSTN_PlatformShapeStates::GetSolidMask(int32 Frame) const
{
	uint8 Mask = 0;
	for (int32 Id = 0; Id < GSTNPlatform::MaxShapes; ++Id)
	{
		if (GSTNPlatform::IsSolid(Timelines[Id].StateAt(Frame)))
		{
			Mask |= uint8(1u << Id);
		}
	}
	return Mask;
}

uint8 FGSTN_PlatformShapeStates::GetTargetMask() const
{
	uint8 Mask = 0;
	for (int32 Id = 0; Id < GSTNPlatform::MaxShapes; ++Id)
	{
		if (Timelines[Id].bTargetOn)
		{
			Mask |= uint8(1u << Id);
		}
	}
	return Mask;
}
