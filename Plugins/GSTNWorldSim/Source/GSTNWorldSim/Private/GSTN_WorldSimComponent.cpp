// Rollback plan F0 spike: a world entity simulated by Network Prediction.

#include "GSTN_WorldSimComponent.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Async/ParallelFor.h"
#include "HAL/PlatformTime.h"
#include "NetworkPredictionDriver.h"
#include "NetworkPredictionModelDef.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionProxyWrite.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNWorldSim, Log, All);

namespace GSTNWorldSim
{
	static int32 SyntheticEntities = 0;
	static FAutoConsoleVariableRef CVarSyntheticEntities(TEXT("gstn.WorldSim.Entities"), SyntheticEntities,
		TEXT("Fake entities the world sim updates per tick (vector math only). Read on the authority and sent in the input, so every machine simulates the same load."));

	static int32 SyntheticTraces = 0;
	static FAutoConsoleVariableRef CVarSyntheticTraces(TEXT("gstn.WorldSim.Traces"), SyntheticTraces,
		TEXT("Line traces against static geometry the world sim runs per tick. Read on the authority and sent in the input."));

	static bool bParallel = false;
	static FAutoConsoleVariableRef CVarParallel(TEXT("gstn.WorldSim.Parallel"), bParallel,
		TEXT("Run the synthetic entities and traces with ParallelFor. The result (the accumulator) must not change: if a client reconciles every frame with it on, the parallel step is not deterministic."));

	static float LogEverySeconds = 0.f;
	static FAutoConsoleVariableRef CVarLogEverySeconds(TEXT("gstn.WorldSim.LogEverySeconds"), LogEverySeconds,
		TEXT("If above 0, every world sim logs its timing report this often (for the network test harness, which reads logs)."));

	/** FNV-1a step: order-sensitive, so a machine that did the same work in another order is caught too */
	FORCEINLINE uint32 Fold(uint32 Accumulator, uint32 Value)
	{
		return (Accumulator ^ Value) * 16777619u;
	}
}

/**
 * Sorted before the standalone input model (PreKinematicMovers - 4) and so before every pawn model: world state a frame reads (a held
 * object's owner, a projectile already in flight) is the state of the frame before, the same on every machine. The number is not
 * shared with any other model (NP warns about equal priorities): -5 is free, see the list in GSTN_InputSimComponent.cpp.
 */
class FGSTN_WorldSimModelDef : public FNetworkPredictionModelDef
{
public:
	NP_MODEL_BODY();

	using Simulation = UGSTN_WorldSimComponent;
	using StateTypes = FGSTN_WorldSimStateTypes;
	using Driver = UGSTN_WorldSimComponent;

	static const TCHAR* GetName() { return TEXT("GSTN_WorldSim"); }
	static constexpr int32 GetSortPriority() { return (int32)ENetworkPredictionSortPriority::PreKinematicMovers - 5; }
};

/**
 * The world sim is forward predicted on simulated proxies whatever the project's SimulatedProxyNetworkLOD says: when the global LOD is
 * not in the model's capable set, Network Prediction takes the highest capable one (FNetworkPredictionDriverBase::GetConfig). Fixed tick
 * only, because independent ticking cannot forward predict a simulated proxy. Pawns keep their own driver and so the global setting.
 */
template<>
struct FNetworkPredictionDriver<FGSTN_WorldSimModelDef> : FNetworkPredictionDriverBase<FGSTN_WorldSimModelDef>
{
	static constexpr FNetworkPredictionModelDefCapabilities GetCapabilities()
	{
		FNetworkPredictionModelDefCapabilities Capabilities;
		Capabilities.FixedNetworkLODs.SP = ENetworkLOD::ForwardPredict;
		Capabilities.SupportedTickingPolicies = ENetworkPredictionTickingPolicy::Fixed;
		return Capabilities;
	}
};

NP_MODEL_REGISTER(FGSTN_WorldSimModelDef);

UGSTN_WorldSimComponent::UGSTN_WorldSimComponent()
{
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void UGSTN_WorldSimComponent::InitializeNetworkPredictionProxy()
{
	NetworkPredictionProxy.Init<FGSTN_WorldSimModelDef>(GetWorld(), GetReplicationProxies(), this, this);
}

const FGSTN_WorldSimSync* UGSTN_WorldSimComponent::GetSyncState() const
{
	return NetworkPredictionProxy.ReadSyncState<FGSTN_WorldSimSync>();
}

void UGSTN_WorldSimComponent::InitializeSimulationState(FGSTN_WorldSimSync* Sync, FGSTN_WorldSimAux* Aux)
{
	if (Sync)
	{
		*Sync = FGSTN_WorldSimSync();
	}
}

void UGSTN_WorldSimComponent::ProduceInput(const int32 DeltaTimeMS, FGSTN_WorldSimInput* Cmd)
{
	// Only the authority produces input (the actor has no owning connection): the cvars are read here, outside the simulation
	*Cmd = FGSTN_WorldSimInput();
	Cmd->SyntheticEntities = (uint16)FMath::Clamp(GSTNWorldSim::SyntheticEntities, 0, 4096);
	Cmd->SyntheticTraces = (uint16)FMath::Clamp(GSTNWorldSim::SyntheticTraces, 0, 4096);
}

void UGSTN_WorldSimComponent::SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<FGSTN_WorldSimStateTypes>& Input,
	const TNetSimOutput<FGSTN_WorldSimStateTypes>& Output)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GSTN_WorldSim_SimulationTick);
	const double StartSeconds = FPlatformTime::Seconds();

	const FGSTN_WorldSimSync& InSync = *Input.Sync;
	FGSTN_WorldSimSync& OutSync = *Output.Sync;
	OutSync = InSync;
	OutSync.Frame = InSync.Frame + 1;

	uint32 Accumulator = GSTNWorldSim::Fold(InSync.Accumulator, (uint32)TimeStep.Frame);
	const FGSTN_WorldSimInput& Cmd = *Input.Cmd;

	// The work of each fake entity reads only the frame and its index and writes only its own result, so it can run on any core in any
	// order; the results are folded in index order afterwards, so the accumulator is the same with or without threads. This is the shape
	// the engaged minions of F9 would take: parallel per-entity step, serial pass for what depends on order (hits, contests)
	const float Dt = TimeStep.StepMS * 0.001f;
	const int32 Frame = TimeStep.Frame;
	const int32 EntityCount = Cmd.SyntheticEntities;
	const int32 TraceCount = Cmd.SyntheticTraces;
	const UWorld* World = GetWorld();
	EntityResults.SetNumUninitialized(EntityCount + TraceCount, EAllowShrinking::No);

	auto StepEntity = [this, Dt, Frame](int32 Index)
	{
		// CPU part of a minion: integrate a position and steer it, from a seed the frame and the index give (no state to keep for a spike)
		const float Phase = (float)((Frame * 31 + Index * 17) % 1024) * (UE_TWO_PI / 1024.f);
		FVector Position(Index * 50.0, FMath::Sin(Phase) * 300.0, 0.0);
		FVector Velocity(FMath::Cos(Phase) * 400.0, FMath::Sin(Phase * 2.f) * 400.0, 0.0);
		const FVector Desired = (FVector::ZeroVector - Position).GetSafeNormal() * 600.0;
		Velocity += (Desired - Velocity).GetClampedToMaxSize(2000.0 * Dt);
		Position += Velocity * Dt;
		EntityResults[Index] = (uint32)FMath::FloorToInt32(Position.X) ^ ((uint32)FMath::FloorToInt32(Position.Y) << 1);
	};
	auto StepTrace = [this, World, Frame, EntityCount](int32 Index)
	{
		// Level-query part of a minion: a ground probe each, on static geometry only so every machine finds the same hits
		static const FName TraceTag(TEXT("GSTN_WorldSim"));
		const FCollisionQueryParams Params(TraceTag, false);
		const double X = ((Index * 7919 + Frame * 13) % 4000) - 2000.0;
		const double Y = ((Index * 104729) % 4000) - 2000.0;
		const bool bHit = World && World->LineTraceTestByChannel(FVector(X, Y, 2000.0), FVector(X, Y, -2000.0), ECC_WorldStatic, Params);
		EntityResults[EntityCount + Index] = bHit ? (uint32)Index : 0u;
	};

	if (GSTNWorldSim::bParallel)
	{
		ParallelFor(EntityCount, StepEntity);
		ParallelFor(TraceCount, StepTrace);
	}
	else
	{
		for (int32 Index = 0; Index < EntityCount; ++Index)
		{
			StepEntity(Index);
		}
		for (int32 Index = 0; Index < TraceCount; ++Index)
		{
			StepTrace(Index);
		}
	}
	for (const uint32 Result : EntityResults)
	{
		Accumulator = GSTNWorldSim::Fold(Accumulator, Result);
	}
	OutSync.Accumulator = Accumulator;

	// Measurement, never read back by the simulation: a tick of a frame already simulated is a re-simulation
	const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
	const bool bResim = TimeStep.Frame <= HighestSimulatedFrame;
	HighestSimulatedFrame = FMath::Max(HighestSimulatedFrame, TimeStep.Frame);
	++Stats.Ticks;
	Stats.TickSeconds += Elapsed;
	if (bResim)
	{
		++Stats.ResimTicks;
		Stats.ResimTickSeconds += Elapsed;
		bCurrentGameFrameResim = true;
	}
	CurrentGameFrameSeconds += Elapsed;
	++CurrentGameFrameTicks;
}

void UGSTN_WorldSimComponent::FinalizeFrame(const FGSTN_WorldSimSync* Sync, const FGSTN_WorldSimAux* Aux)
{
	// Once per game frame: close the frame's measurement
	++Stats.GameFrames;
	if (bCurrentGameFrameResim)
	{
		++Stats.GameFramesWithResim;
	}
	if (CurrentGameFrameSeconds > Stats.WorstGameFrameSeconds)
	{
		Stats.WorstGameFrameSeconds = CurrentGameFrameSeconds;
		Stats.WorstGameFrameTicks = CurrentGameFrameTicks;
	}
	CurrentGameFrameSeconds = 0.0;
	CurrentGameFrameTicks = 0;
	bCurrentGameFrameResim = false;

	if (GSTNWorldSim::LogEverySeconds > 0.f)
	{
		static double LastLogSeconds = 0.0;
		const double Now = FPlatformTime::Seconds();
		// Per component, not static, would be cleaner; one line per interval per process is what the harness needs
		if (Now - LastLogSeconds >= GSTNWorldSim::LogEverySeconds)
		{
			LastLogSeconds = Now;
			UE_LOG(LogGSTNWorldSim, Log, TEXT("%s"), *DescribeStats());
		}
	}
}

void UGSTN_WorldSimComponent::ResetStats()
{
	Stats = FGSTN_WorldSimStats();
}

FString UGSTN_WorldSimComponent::DescribeStats() const
{
	const UWorld* World = GetWorld();
	const TCHAR* Role = !World ? TEXT("?") : World->GetNetMode() == NM_Client ? TEXT("client") : World->GetNetMode() == NM_ListenServer ? TEXT("host") : TEXT("server");
	const FGSTN_WorldSimSync* Sync = GetSyncState();
	const int64 FirstTicks = Stats.Ticks - Stats.ResimTicks;
	const double FirstSeconds = Stats.TickSeconds - Stats.ResimTickSeconds;
	return FString::Printf(TEXT("WorldSim [%s %s%s] frame %d acc %08x | ticks %lld (resim %lld) | avg tick %.3f ms, avg resim tick %.3f ms | game frames %lld (with resim %lld) | worst game frame %.3f ms over %d ticks"),
		Role, *GetNameSafe(World), GSTNWorldSim::bParallel ? TEXT(" parallel") : TEXT(""), Sync ? Sync->Frame : -1, Sync ? Sync->Accumulator : 0u,
		Stats.Ticks, Stats.ResimTicks,
		FirstTicks > 0 ? FirstSeconds * 1000.0 / FirstTicks : 0.0,
		Stats.ResimTicks > 0 ? Stats.ResimTickSeconds * 1000.0 / Stats.ResimTicks : 0.0,
		Stats.GameFrames, Stats.GameFramesWithResim,
		Stats.WorstGameFrameSeconds * 1000.0, Stats.WorstGameFrameTicks);
}

AGSTN_WorldSimActor::AGSTN_WorldSimActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = false;

	SimComponent = CreateDefaultSubobject<UGSTN_WorldSimComponent>(TEXT("WorldSim"));
}
