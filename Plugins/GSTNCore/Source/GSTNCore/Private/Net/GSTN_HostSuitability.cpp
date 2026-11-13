// Shared net helpers (GSTNCore, Net/). Pure: Core only.

#include "Net/GSTN_HostSuitability.h"

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"

namespace
{
	/** 150 minion-like bodies, 2000 steps: integrate, steer toward a target, clamp speed. Volatile sink keeps the optimizer honest */
	double RunWorkloadOnce()
	{
		constexpr int32 Bodies = 150;
		constexpr int32 Steps = 2000;
		FVector3f Position[Bodies];
		FVector3f Velocity[Bodies];
		for (int32 Index = 0; Index < Bodies; ++Index)
		{
			Position[Index] = FVector3f(static_cast<float>(Index * 37 % 1000), static_cast<float>(Index * 91 % 1000), 0.f);
			Velocity[Index] = FVector3f::ZeroVector;
		}
		const double Start = FPlatformTime::Seconds();
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const FVector3f Target(500.f * FMath::Sin(Step * 0.01f), 500.f * FMath::Cos(Step * 0.013f), 0.f);
			for (int32 Index = 0; Index < Bodies; ++Index)
			{
				const FVector3f ToTarget = Target - Position[Index];
				const float Distance = ToTarget.Size() + 1.f;
				Velocity[Index] += ToTarget / Distance * 25.f;
				Velocity[Index] = Velocity[Index].GetClampedToMaxSize(600.f);
				Position[Index] += Velocity[Index] * 0.016f;
			}
		}
		const double Elapsed = (FPlatformTime::Seconds() - Start) * 1000.0;
		volatile float Sink = Position[Bodies - 1].X;
		(void)Sink;
		return Elapsed;
	}
}

double FGSTN_HostSuitability::RunBenchmarkMs(int32 Runs)
{
	double Best = TNumericLimits<double>::Max();
	for (int32 Run = 0; Run < FMath::Max(Runs, 1); ++Run)
	{
		Best = FMath::Min(Best, RunWorkloadOnce());
	}
	return Best;
}

void FGSTN_HostSuitability::Evaluate(FGSTN_HostSuitability& InOut)
{
	// 0.5 at the reference machine, toward 1 when faster, toward 0 when slower
	InOut.CpuFactor = InOut.BenchmarkMs > 0.0 ? ReferenceBenchmarkMs / (ReferenceBenchmarkMs + InOut.BenchmarkMs) : 0.0;
	// The game thread hosts the simulation; extra cores carry the net driver, Oodle and the render thread. Saturates at 8
	InOut.CoreFactor = FMath::Clamp(InOut.PhysicalCores / 8.0, 0.0, 1.0);
	InOut.UploadFactor = InOut.UploadKBps < 0.0 ? 1.0 : FMath::Clamp(InOut.UploadKBps / RequiredUploadKBps, 0.0, 1.0);
	InOut.Score = 100.0 * (0.7 * InOut.CpuFactor + 0.3 * InOut.CoreFactor) * InOut.UploadFactor;
	InOut.bEligible = InOut.PhysicalCores >= MinPhysicalCores && (InOut.UploadKBps < 0.0 || InOut.UploadKBps >= RequiredUploadKBps);
}

FGSTN_HostSuitability FGSTN_HostSuitability::Measure(double InUploadKBps)
{
	FGSTN_HostSuitability Result;
	Result.CpuBrand = FPlatformMisc::GetCPUBrand().TrimStartAndEnd();
	Result.PhysicalCores = FPlatformMisc::NumberOfCores();
	Result.LogicalCores = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
	Result.BenchmarkMs = RunBenchmarkMs();
	Result.UploadKBps = InUploadKBps;
	Evaluate(Result);
	return Result;
}

FString FGSTN_HostSuitability::ToString() const
{
	return FString::Printf(TEXT("HostScore score=%.1f eligible=%d cpu='%s' cores=%d/%d bench=%.3fms upload=%s factors cpu=%.2f cores=%.2f upload=%.2f"),
		Score, bEligible ? 1 : 0, *CpuBrand, PhysicalCores, LogicalCores, BenchmarkMs,
		UploadKBps < 0.0 ? TEXT("unknown") : *FString::Printf(TEXT("%.0fKB/s"), UploadKBps), CpuFactor, CoreFactor, UploadFactor);
}

namespace
{
	FAutoConsoleCommand HostScoreCommand(
		TEXT("gstn.Net.HostScore"),
		TEXT("Prints this machine's host suitability (lever 4). Optional argument: measured upload in KB/s."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const double Upload = Args.Num() > 0 ? FCString::Atod(*Args[0]) : -1.0;
			UE_LOG(LogTemp, Display, TEXT("%s"), *FGSTN_HostSuitability::Measure(Upload).ToString());
		}));
}
