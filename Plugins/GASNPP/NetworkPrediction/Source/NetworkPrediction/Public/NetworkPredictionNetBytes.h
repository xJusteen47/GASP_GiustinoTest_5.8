// GSTN fork: debug byte counter for the server's Network Prediction sends (netcode lever 1, 2026-11-09).
// Off by default (np.Debug.NetBytes 0): when off the send path only reads one int. When on, every AP/SP send adds its bits under
// "Model|Target|Part" so `np.Debug.NetBytes.Dump <label>` can say which model and which part (input, sync, aux, cues, header) fills the
// players' replication traffic. It never changes what is written on the wire.
#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreNet.h"

namespace UE::NetworkPrediction::NetBytes
{
	extern NETWORKPREDICTION_API int32 GEnabled;
	// Optional label set by a caller around a nested serialize (e.g. "MoverSync") so inner code can split its bytes; null = do not split
	extern NETWORKPREDICTION_API const TCHAR* GContext;

	NETWORKPREDICTION_API void Add(const TCHAR* Model, const TCHAR* Target, const TCHAR* Part, int64 Bits);
	NETWORKPREDICTION_API const TCHAR* TargetName(uint8 Target);

	// Every NP send archive is an FNetBitWriter (legacy replication); only called while GEnabled
	inline int64 Bits(FArchive& Ar) { return static_cast<FNetBitWriter&>(Ar).GetNumBits(); }
}
