// The debug module's sink of the combat trace (E17): writes the CombatFrame / CombatEvent events on the GSTNCombat channel and counts what it
// saw. Installed only while gstn.Combat.Debug.Trace is 1.

#pragma once

#include "CoreMinimal.h"
#include "Trace/GSTN_CombatTrace.h"

class IConsoleVariable;

/** Forwards to the trace events and counts. Tests construct one of their own and install it with GSTNCombatTrace::SetSink */
class FGSTN_CombatTraceSink : public IGSTN_CombatTraceSink
{
public:
	virtual void OnCombatFrame(const FGSTN_CombatTraceFrame& Frame) override;
	virtual void OnCombatEvent(const FGSTN_CombatTraceEvent& Event) override;

	int32 NumFrames = 0;
	int32 NumResimulatedFrames = 0;
	int32 NumEvents = 0;
	int32 NumRollbacks = 0;
	int32 LastFrame = INDEX_NONE;
};

/** Owns gstn.Combat.Debug.Trace and the sink it installs (module startup and shutdown) */
class FGSTN_CombatTraceController
{
public:
	static void Startup();
	static void Shutdown();

	/** The sink the variable installs, null while the variable is 0 (for tests and the module) */
	static const FGSTN_CombatTraceSink* GetInstalledSink();

	/** Applies the current value of the variable (install or remove the sink) */
	static void Apply(bool bEnabled);
};
