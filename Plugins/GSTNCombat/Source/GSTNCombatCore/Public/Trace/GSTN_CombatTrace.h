// The combat trace (combat-m5-editor 10.4, E17): a Unreal Insights channel with one event per simulated frame per combat entity and one
// per notable combat event, plus the tiny sink interface the simulation adapter calls so that it never depends on the debug module.

#pragma once

#include "CoreMinimal.h"
#include "Trace/Trace.h"

/** Channel "GSTNCombat": open an Insights session with -trace=default,GSTNCombat */
UE_TRACE_CHANNEL_EXTERN(GSTNCombatChannel, GSTNCOMBATCORE_API)

/** What a CombatEvent reports. A and B of the event mean different things per kind (see FGSTN_CombatTraceEvent) */
enum class EGSTN_CombatTraceEventKind : uint8
{
	/** A command resolved into a request this frame. A = binding index, B = timeline registry index of the binding (INDEX_NONE for a plain ability) */
	CommandExecuted,
	/** The entity landed a hit. A = victim entity id, B = hitbox group of the hit */
	Hit,
	/** A timeline ability that something other than a command activated was adopted. A = timeline registry index, B = 0 */
	Adoption,
	/** The running timeline was cancelled by another command. A = the cancelled timeline registry index, B = 0 */
	Cancel,
	/** The model was restored for a re-simulation. A = first re-simulated frame, B = last frame simulated before the restore (INDEX_NONE if unknown) */
	Rollback
};

/** The combat state of one entity at the end of one simulated frame. Plain data: read from the state the tick already holds */
struct FGSTN_CombatTraceFrame
{
	int32 EntityId = INDEX_NONE;
	int32 Frame = INDEX_NONE;
	/** The frame is a re-simulation of one that was already simulated (after a rollback) */
	bool bResimulated = false;
	/** EGSTN_ActionState as a byte */
	uint8 ActionState = 0;
	/** Registry index of the running timeline, INDEX_NONE outside one */
	int32 TimelineIndex = INDEX_NONE;
	/** Frame of the running timeline (the paused hitstop frames excluded), 0 outside one */
	int32 TimelineFrame = 0;
	/** Frames of hitstop and hitstun still to come after this frame, 0 when none */
	int32 HitstopFrames = 0;
	int32 HitstunFrames = 0;
	/** Latest frame the input history holds (the head of the buffer) */
	int32 HistoryHead = INDEX_NONE;
};

struct FGSTN_CombatTraceEvent
{
	int32 EntityId = INDEX_NONE;
	int32 Frame = INDEX_NONE;
	EGSTN_CombatTraceEventKind Kind = EGSTN_CombatTraceEventKind::CommandExecuted;
	bool bResimulated = false;
	int32 A = 0;
	int32 B = 0;
};

/**
 * What the simulation adapter calls once per simulated frame per entity and at the combat events. Implemented by the debug module (it
 * emits the trace events); tests install their own. Called on the game thread inside the simulation tick: an implementation must only
 * read what it is given and must never touch simulation state.
 */
class IGSTN_CombatTraceSink
{
public:
	virtual ~IGSTN_CombatTraceSink() = default;
	virtual void OnCombatFrame(const FGSTN_CombatTraceFrame& Frame) = 0;
	virtual void OnCombatEvent(const FGSTN_CombatTraceEvent& Event) = 0;
};

namespace GSTNCombatTrace
{
#if !UE_BUILD_SHIPPING
	/** The installed sink, null by default. A plain pointer: the cost of the call site with no sink is one load and one branch */
	extern GSTNCOMBATCORE_API IGSTN_CombatTraceSink* GSink;

	/** The sink is not owned; the installer removes it (SetSink(nullptr)) before it dies */
	GSTNCOMBATCORE_API void SetSink(IGSTN_CombatTraceSink* Sink);
	FORCEINLINE IGSTN_CombatTraceSink* GetSink() { return GSink; }
#else
	// Shipping: no sink exists and the call sites are compiled out
	FORCEINLINE void SetSink(IGSTN_CombatTraceSink*) {}
	FORCEINLINE IGSTN_CombatTraceSink* GetSink() { return nullptr; }
#endif

	/** Writes a CombatFrame / CombatEvent on the GSTNCombat channel (a no-op while the channel is off, and where tracing is compiled out). A Rollback event also places a bookmark */
	GSTNCOMBATCORE_API void EmitFrame(const FGSTN_CombatTraceFrame& Frame);
	GSTNCOMBATCORE_API void EmitEvent(const FGSTN_CombatTraceEvent& Event);
}
