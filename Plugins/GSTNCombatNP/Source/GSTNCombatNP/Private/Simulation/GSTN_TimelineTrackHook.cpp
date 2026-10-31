// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_TimelineTrackHook.h"

#include "Simulation/GSTN_InputSimTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "Timeline/GSTN_CombatTimeline.h"

namespace
{
	/** Windows one evaluation reports at most; the output list is inline for this many */
	constexpr int32 MaxStampedWindows = 2;
}

FGSTN_TimelineTrackHookRegistry& FGSTN_TimelineTrackHookRegistry::Get()
{
	static FGSTN_TimelineTrackHookRegistry Registry;
	return Registry;
}

bool FGSTN_TimelineTrackHookRegistry::Register(const UScriptStruct* TrackStruct, IGSTN_TimelineTrackHook* Hook)
{
	if (!TrackStruct || !Hook)
	{
		return false;
	}
	for (const FEntry& Entry : Hooks)
	{
		if (Entry.TrackStruct == TrackStruct)
		{
			return false;
		}
	}
	FEntry NewEntry;
	NewEntry.TrackStruct = TrackStruct;
	NewEntry.Hook = Hook;
	Hooks.Add(NewEntry);
	return true;
}

void FGSTN_TimelineTrackHookRegistry::Unregister(IGSTN_TimelineTrackHook* Hook)
{
	Hooks.RemoveAll([Hook](const FEntry& Entry) { return Entry.Hook == Hook; });
}

bool FGSTN_TimelineTrackHookRegistry::Run(const FGSTN_CombatTimelineData& Data, int32 Frame, int32 TimelineStartFrame, int32 TimelineFrame, bool bHitstopFrame,
	const FGSTN_InputSimState& Input, FGSTN_StampedWindowList& OutWindows, const FGSTN_TrackHookEnvironment* Environment) const
{
	bool bStall = false;
	if (Hooks.IsEmpty())
	{
		return bStall;
	}
	// The order of the tracks in the data, the same on every machine; the registry only says which struct has a hook
	for (int32 TrackIndex = 0; TrackIndex < Data.Tracks.Num(); ++TrackIndex)
	{
		const TInstancedStruct<FGSTN_TimelineTrack>& Instance = Data.Tracks[TrackIndex];
		const UScriptStruct* TrackStruct = Instance.GetScriptStruct();
		const FGSTN_TimelineTrack* Track = Instance.GetPtr();
		if (!TrackStruct || !Track)
		{
			continue;
		}
		for (const FEntry& Entry : Hooks)
		{
			if (Entry.TrackStruct != TrackStruct)
			{
				continue;
			}
			FGSTN_TrackHookContext Context(*Track, Input);
			Context.TrackIndex = TrackIndex;
			Context.Frame = Frame;
			Context.TimelineStartFrame = TimelineStartFrame;
			Context.TimelineFrame = TimelineFrame;
			Context.bHitstopFrame = bHitstopFrame;
			if (Environment)
			{
				Context.Buttons = Environment->Buttons;
				Context.Fragments = Environment->Fragments;
			}
			const FGSTN_TrackHookResult Result = Entry.Hook->Evaluate(Context);
			bStall |= Result.bStallThisFrame;
			if (Result.bStampWindow && OutWindows.Num() < MaxStampedWindows)
			{
				OutWindows.Add(Result.Window);
			}
			break;
		}
	}
	return bStall;
}
