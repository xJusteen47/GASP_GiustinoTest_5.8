// Rollback plan F0 spike: a world entity simulated by Network Prediction.

#pragma once

#include "CoreMinimal.h"
#include "Misc/StringBuilder.h"
#include "NetworkPredictionComponent.h"
#include "NetworkPredictionReplicationProxy.h"
#include "NetworkPredictionSimulation.h"
#include "NetworkPredictionStateTypes.h"
#include "NetworkPredictionTickState.h"
#include "NetworkPredictionTrace.h"
#include "GSTN_WorldSimComponent.generated.h"

/**
 * Input of the world model. The world entity has no player: its "input" is what the authority decides outside the simulation, here
 * the size of the synthetic load. It travels to simulated proxies with the rest of the frame (Network Prediction sends the input to
 * forward-predicted proxies), so every machine simulates the same load without reading a cvar inside the tick.
 */
struct FGSTN_WorldSimInput
{
	/** Fake entities updated per tick (vector math only): the CPU part of a minion's tick */
	uint16 SyntheticEntities = 0;

	/** Line traces per tick against static geometry: the level-query part of a minion's tick, usually the expensive one */
	uint16 SyntheticTraces = 0;

	void NetSerialize(const FNetSerializeParams& P)
	{
		P.Ar << SyntheticEntities;
		P.Ar << SyntheticTraces;
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("Entities %u Traces %u\n", SyntheticEntities, SyntheticTraces);
	}
};

/**
 * Sync state of the world model. Frame counts the simulated ticks; Accumulator folds every result of the synthetic work, so the work
 * cannot be optimised away and a machine that computed anything differently is reconciled (the spike's determinism check).
 */
struct FGSTN_WorldSimSync
{
	int32 Frame = 0;
	uint32 Accumulator = 0;

	void NetSerialize(const FNetSerializeParams& P)
	{
		P.Ar << Frame;
		P.Ar << Accumulator;
	}

	bool ShouldReconcile(const FGSTN_WorldSimSync& AuthorityState) const
	{
		UE_NP_TRACE_RECONCILE(Frame != AuthorityState.Frame, "Frame:");
		UE_NP_TRACE_RECONCILE(Accumulator != AuthorityState.Accumulator, "Accumulator:");
		return false;
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("Frame %d Accumulator %08x\n", Frame, Accumulator);
	}

	void Interpolate(const FGSTN_WorldSimSync* From, const FGSTN_WorldSimSync* To, float PCT)
	{
		*this = *To;
	}
};

/** No auxiliary state */
struct FGSTN_WorldSimAux
{
	void NetSerialize(const FNetSerializeParams& P) {}
	bool ShouldReconcile(const FGSTN_WorldSimAux& AuthorityState) const { return false; }
	void ToString(FAnsiStringBuilderBase& Out) const {}
	void Interpolate(const FGSTN_WorldSimAux* From, const FGSTN_WorldSimAux* To, float PCT) {}
};

using FGSTN_WorldSimStateTypes = TNetworkPredictionStateTypes<FGSTN_WorldSimInput, FGSTN_WorldSimSync, FGSTN_WorldSimAux>;

/** What the component measured since the last reset, outside the simulation (never read by it) */
struct FGSTN_WorldSimStats
{
	int64 Ticks = 0;
	int64 ResimTicks = 0;
	double TickSeconds = 0.0;
	double ResimTickSeconds = 0.0;
	int64 GameFrames = 0;
	int64 GameFramesWithResim = 0;
	double WorstGameFrameSeconds = 0.0;
	int32 WorstGameFrameTicks = 0;
};

/**
 * Rollback plan F0 (docs/architecture/tag-verbi-simulazione-rollback.md): proves that a world entity (held objects, projectiles, KO
 * and lives, missions) can live in the rollback simulation next to the pawns. The model is forced to forward prediction on simulated
 * proxies through its driver's capabilities, so it rolls back and re-simulates with the predicted pawns while the project keeps
 * interpolating other models (SimulatedProxyNetworkLOD).
 *
 * The tick does synthetic work sized by the input (gstn.WorldSim.Entities / gstn.WorldSim.Traces on the authority) and the component
 * times every tick, split into first simulations and re-simulations (a tick of a frame already simulated). gstn.WorldSim.Report prints
 * the numbers; np.ForceReconcile forces corrections to measure the worst case.
 */
UCLASS(ClassGroup = (Simulation), meta = (BlueprintSpawnableComponent))
class GSTNWORLDSIM_API UGSTN_WorldSimComponent : public UNetworkPredictionComponent
{
	GENERATED_BODY()

public:
	UGSTN_WorldSimComponent();

	// Network Prediction driver and simulation
	void InitializeSimulationState(FGSTN_WorldSimSync* Sync, FGSTN_WorldSimAux* Aux);
	void ProduceInput(const int32 DeltaTimeMS, FGSTN_WorldSimInput* Cmd);
	void SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<FGSTN_WorldSimStateTypes>& Input, const TNetSimOutput<FGSTN_WorldSimStateTypes>& Output);
	void FinalizeFrame(const FGSTN_WorldSimSync* Sync, const FGSTN_WorldSimAux* Aux);

	const FGSTN_WorldSimSync* GetSyncState() const;
	const FGSTN_WorldSimStats& GetStats() const { return Stats; }
	void ResetStats();

	/** One line per stat, for the report command and logs */
	FString DescribeStats() const;

protected:
	virtual void InitializeNetworkPredictionProxy() override;

private:
	// Scratch of one tick (one result per fake entity and trace), filled and folded inside SimulationTick: never carries state across ticks
	TArray<uint32> EntityResults;

	// Measurement only, outside the sync state: never read by SimulationTick
	FGSTN_WorldSimStats Stats;
	int32 HighestSimulatedFrame = INDEX_NONE;
	double CurrentGameFrameSeconds = 0.0;
	int32 CurrentGameFrameTicks = 0;
	bool bCurrentGameFrameResim = false;
};

/** The replicated, always relevant actor that carries the world sim. Spawned by UGSTN_WorldSimSubsystem on the authority */
UCLASS(NotPlaceable, Transient)
class GSTNWORLDSIM_API AGSTN_WorldSimActor : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_WorldSimActor();

	UGSTN_WorldSimComponent* GetSimComponent() const { return SimComponent; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Simulation")
	TObjectPtr<UGSTN_WorldSimComponent> SimComponent;
};
