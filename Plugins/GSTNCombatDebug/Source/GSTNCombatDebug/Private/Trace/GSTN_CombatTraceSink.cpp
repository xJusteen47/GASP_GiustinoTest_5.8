#include "Trace/GSTN_CombatTraceSink.h"

#include "HAL/IConsoleManager.h"

void FGSTN_CombatTraceSink::OnCombatFrame(const FGSTN_CombatTraceFrame& Frame)
{
	++NumFrames;
	NumResimulatedFrames += Frame.bResimulated ? 1 : 0;
	LastFrame = Frame.Frame;
	GSTNCombatTrace::EmitFrame(Frame);
}

void FGSTN_CombatTraceSink::OnCombatEvent(const FGSTN_CombatTraceEvent& Event)
{
	++NumEvents;
	NumRollbacks += Event.Kind == EGSTN_CombatTraceEventKind::Rollback ? 1 : 0;
	GSTNCombatTrace::EmitEvent(Event);
}

namespace
{
	IConsoleVariable* TraceVariable = nullptr;
	FGSTN_CombatTraceSink* InstalledSink = nullptr;

	void OnTraceVariableChanged(IConsoleVariable* Variable)
	{
		FGSTN_CombatTraceController::Apply(Variable->GetInt() != 0);
	}
}

void FGSTN_CombatTraceController::Apply(bool bEnabled)
{
	if (bEnabled && !InstalledSink)
	{
		InstalledSink = new FGSTN_CombatTraceSink();
		GSTNCombatTrace::SetSink(InstalledSink);
	}
	else if (!bEnabled && InstalledSink)
	{
		// Only take the slot back if it is still ours: a test may have installed its own sink over it
		if (GSTNCombatTrace::GetSink() == InstalledSink)
		{
			GSTNCombatTrace::SetSink(nullptr);
		}
		delete InstalledSink;
		InstalledSink = nullptr;
	}
}

void FGSTN_CombatTraceController::Startup()
{
	TraceVariable = IConsoleManager::Get().RegisterConsoleVariable(TEXT("gstn.Combat.Debug.Trace"), 0,
		TEXT("Writes the combat state to the Unreal Insights channel GSTNCombat (one event per simulated frame per combat entity, plus combat events). Start the session with -trace=default,GSTNCombat. 0 = off (no sink installed), 1 = on."), ECVF_Default);
	if (TraceVariable)
	{
		TraceVariable->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnTraceVariableChanged));
		// A value from a config file or the command line is already set when the variable is registered
		Apply(TraceVariable->GetInt() != 0);
	}
}

void FGSTN_CombatTraceController::Shutdown()
{
	Apply(false);
	if (TraceVariable)
	{
		IConsoleManager::Get().UnregisterConsoleObject(TraceVariable, false);
		TraceVariable = nullptr;
	}
}

const FGSTN_CombatTraceSink* FGSTN_CombatTraceController::GetInstalledSink()
{
	return InstalledSink;
}
