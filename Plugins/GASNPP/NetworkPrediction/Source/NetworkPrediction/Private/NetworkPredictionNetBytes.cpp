// GSTN fork: see NetworkPredictionNetBytes.h
#include "NetworkPredictionNetBytes.h"

#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NetworkPredictionLog.h"

namespace UE::NetworkPrediction::NetBytes
{
	int32 GEnabled = 0;
	const TCHAR* GContext = nullptr;
	static FAutoConsoleVariableRef CVarEnabled(TEXT("np.Debug.NetBytes"), GEnabled,
		TEXT("1: count the bits of every server NP send (AP/SP) per model and part, for np.Debug.NetBytes.Dump. Debug only, wire unchanged."));

	struct FEntry
	{
		int64 Bits = 0;
		int64 Calls = 0;
	};

	static TMap<FString, FEntry> GEntries;
	static double GWindowStart = 0.0;

	void Add(const TCHAR* Model, const TCHAR* Target, const TCHAR* Part, int64 Bits)
	{
		check(IsInGameThread());
		FEntry& E = GEntries.FindOrAdd(FString::Printf(TEXT("%s|%s|%s"), Model ? Model : TEXT("?"), Target, Part));
		E.Bits += Bits;
		E.Calls++;
	}

	const TCHAR* TargetName(uint8 Target)
	{
		switch (Target)
		{
		case 0: return TEXT("RPC");
		case 1: return TEXT("AP");
		case 2: return TEXT("SP");
		default: return TEXT("Replay");
		}
	}

	// Writes the window since the last dump to Saved/Logs/NPNetBytes.txt (appended), sorted by bytes, then resets. Prints only a summary line.
	static void Dump(const TArray<FString>& Args)
	{
		const double Now = FPlatformTime::Seconds();
		const double Dt = GWindowStart > 0.0 ? FMath::Max(Now - GWindowStart, 0.001) : 0.0;
		const FString Label = Args.Num() > 0 ? Args[0] : TEXT("-");

		TArray<TPair<FString, FEntry>> Rows;
		int64 TotalBits = 0;
		for (const TPair<FString, FEntry>& It : GEntries)
		{
			Rows.Add(It);
			if (It.Key.EndsWith(TEXT("|Total")))
			{
				TotalBits += It.Value.Bits;
			}
		}
		Rows.Sort([](const TPair<FString, FEntry>& A, const TPair<FString, FEntry>& B) { return A.Value.Bits > B.Value.Bits; });

		FString Out = FString::Printf(TEXT("=== window %s dt=%.2fs npTotalBytes=%lld npBps=%.0f\n"), *Label, Dt, TotalBits / 8, Dt > 0.0 ? TotalBits / 8.0 / Dt : 0.0);

		// Server connections: wire bytes and the rate cap they run under, to tell a saturated link from a real demand
		if (GEngine)
		{
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				const UWorld* World = Ctx.World();
				UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
				if (Driver && Driver->IsServer())
				{
					for (UNetConnection* Conn : Driver->ClientConnections)
					{
						if (Conn)
						{
							Out += FString::Printf(TEXT("conn %s CurrentNetSpeed=%d OutTotalBytes=%lld\n"), *Conn->LowLevelGetRemoteAddress(), Conn->CurrentNetSpeed, (long long)Conn->OutTotalBytes);
						}
					}
				}
			}
		}

		for (const TPair<FString, FEntry>& R : Rows)
		{
			Out += FString::Printf(TEXT("%-60s bytes=%9lld calls=%7lld avgB=%7.1f Bps=%8.0f callsPerS=%6.1f\n"), *R.Key, R.Value.Bits / 8, R.Value.Calls,
				R.Value.Calls ? R.Value.Bits / 8.0 / R.Value.Calls : 0.0, Dt > 0.0 ? R.Value.Bits / 8.0 / Dt : 0.0, Dt > 0.0 ? R.Value.Calls / Dt : 0.0);
		}

		const FString Path = FPaths::ProjectLogDir() / TEXT("NPNetBytes.txt");
		FFileHelper::SaveStringToFile(Out, *Path, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
		UE_LOG(LogNetworkPrediction, Display, TEXT("NPNetBytes window %s dt=%.2f npBps=%.0f rows=%d -> %s"), *Label, Dt, Dt > 0.0 ? TotalBits / 8.0 / Dt : 0.0, Rows.Num(), *Path);

		GEntries.Reset();
		GWindowStart = Now;
	}

	static FAutoConsoleCommand CmdDump(TEXT("np.Debug.NetBytes.Dump"),
		TEXT("Append the NP send bytes since the last dump (per Model|Target|Part) to Saved/Logs/NPNetBytes.txt and reset. Arg: window label."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Dump));
}
