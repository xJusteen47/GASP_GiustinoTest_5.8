// Shared net helpers (GSTNCore, Net/). Pure: Core only.

#pragma once

#include "CoreMinimal.h"

/**
 * How well this machine can host a listen-server session (netcode lever 4: host = the strongest PC of the lobby). The host simulates
 * every minion and uploads their state to every client, so a weak host makes everyone's game worse; the lobby should pick the
 * machine with the highest Score among those that pass bEligible.
 *
 * Inputs: logical and physical core counts, a short fixed single-thread benchmark (a deterministic float workload shaped like a
 * minion step, timed with the platform clock, best of a few runs) and, when the caller knows it, the measured upload bandwidth.
 * The benchmark is a relative number between machines running the same build, not a frame-time prediction.
 *
 * Not used by gameplay and never called from the simulation. Console: gstn.Net.HostScore [UploadKBps] prints the result.
 */
struct GSTNCORE_API FGSTN_HostSuitability
{
	FString CpuBrand;
	int32 PhysicalCores = 0;
	int32 LogicalCores = 0;
	/** Best time of the benchmark workload in milliseconds (lower is better) */
	double BenchmarkMs = 0.0;
	/** Measured upload in KB/s, < 0 when unknown */
	double UploadKBps = -1.0;

	/** 0..1 each: the parts of the score */
	double CpuFactor = 0.0;
	double CoreFactor = 0.0;
	/** 1 when the upload is unknown or reaches RequiredUploadKBps, lower otherwise */
	double UploadFactor = 1.0;

	/** 0..100, compare between lobby members; higher hosts better */
	double Score = 0.0;
	/** False when the machine should never host: fewer than MinPhysicalCores, or a known upload below RequiredUploadKBps */
	bool bEligible = false;

	/** Benchmark time of the reference machine (the dev laptop, Development editor) that maps to CpuFactor 0.5 */
	static constexpr double ReferenceBenchmarkMs = 3.0;
	/** Host upload a session needs: ~25 KB/s per client with minions and Oodle (m8-netcode-34), 3 clients, plus margin */
	static constexpr double RequiredUploadKBps = 100.0;
	static constexpr int32 MinPhysicalCores = 4;

	/** Measures this machine (blocks for a few tens of milliseconds). UploadKBps < 0 = unknown */
	static FGSTN_HostSuitability Measure(double InUploadKBps = -1.0);

	/** Scores already measured numbers (pure, tested): fills the factors, Score and bEligible */
	static void Evaluate(FGSTN_HostSuitability& InOut);

	/** The benchmark workload alone, in milliseconds (best of Runs) */
	static double RunBenchmarkMs(int32 Runs = 5);

	FString ToString() const;
};
