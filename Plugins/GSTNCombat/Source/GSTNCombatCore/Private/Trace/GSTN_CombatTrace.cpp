#include "Trace/GSTN_CombatTrace.h"

#include "ProfilingDebugging/MiscTrace.h"

#if UE_TRACE_ENABLED
UE_TRACE_CHANNEL_DEFINE(GSTNCombatChannel, "GSTN combat state per simulated frame and combat events (E17)")

UE_TRACE_EVENT_BEGIN(GSTNCombat, CombatFrame)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(int32, EntityId)
	UE_TRACE_EVENT_FIELD(int32, Frame)
	UE_TRACE_EVENT_FIELD(bool, bResimulated)
	UE_TRACE_EVENT_FIELD(uint8, ActionState)
	UE_TRACE_EVENT_FIELD(int32, TimelineIndex)
	UE_TRACE_EVENT_FIELD(int32, TimelineFrame)
	UE_TRACE_EVENT_FIELD(int32, HitstopFrames)
	UE_TRACE_EVENT_FIELD(int32, HitstunFrames)
	UE_TRACE_EVENT_FIELD(int32, HistoryHead)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(GSTNCombat, CombatEvent)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(int32, EntityId)
	UE_TRACE_EVENT_FIELD(int32, Frame)
	UE_TRACE_EVENT_FIELD(uint8, Kind)
	UE_TRACE_EVENT_FIELD(bool, bResimulated)
	UE_TRACE_EVENT_FIELD(int32, A)
	UE_TRACE_EVENT_FIELD(int32, B)
UE_TRACE_EVENT_END()
#endif

#if !UE_BUILD_SHIPPING
IGSTN_CombatTraceSink* GSTNCombatTrace::GSink = nullptr;

void GSTNCombatTrace::SetSink(IGSTN_CombatTraceSink* Sink)
{
	GSink = Sink;
}
#endif

void GSTNCombatTrace::EmitFrame(const FGSTN_CombatTraceFrame& Frame)
{
#if UE_TRACE_ENABLED
	UE_TRACE_LOG(GSTNCombat, CombatFrame, GSTNCombatChannel)
		<< CombatFrame.Cycle(FPlatformTime::Cycles64())
		<< CombatFrame.EntityId(Frame.EntityId)
		<< CombatFrame.Frame(Frame.Frame)
		<< CombatFrame.bResimulated(Frame.bResimulated)
		<< CombatFrame.ActionState(Frame.ActionState)
		<< CombatFrame.TimelineIndex(Frame.TimelineIndex)
		<< CombatFrame.TimelineFrame(Frame.TimelineFrame)
		<< CombatFrame.HitstopFrames(Frame.HitstopFrames)
		<< CombatFrame.HitstunFrames(Frame.HitstunFrames)
		<< CombatFrame.HistoryHead(Frame.HistoryHead);
#endif
}

void GSTNCombatTrace::EmitEvent(const FGSTN_CombatTraceEvent& Event)
{
#if UE_TRACE_ENABLED
	UE_TRACE_LOG(GSTNCombat, CombatEvent, GSTNCombatChannel)
		<< CombatEvent.Cycle(FPlatformTime::Cycles64())
		<< CombatEvent.EntityId(Event.EntityId)
		<< CombatEvent.Frame(Event.Frame)
		<< CombatEvent.Kind(static_cast<uint8>(Event.Kind))
		<< CombatEvent.bResimulated(Event.bResimulated)
		<< CombatEvent.A(Event.A)
		<< CombatEvent.B(Event.B);
	if (Event.Kind == EGSTN_CombatTraceEventKind::Rollback)
	{
		TRACE_BOOKMARK(TEXT("GSTNCombat rollback entity %d: resimulate from frame %d (last simulated %d)"), Event.EntityId, Event.A, Event.B);
	}
#endif
}
