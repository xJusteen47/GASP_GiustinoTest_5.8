// Single entry point of the collision queries the rollback simulation makes.

#include "Sim/GSTN_SimQuery.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeLock.h"
#include "UObject/ObjectKey.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNSimQuery, Log, All);

namespace GSTNSimQuery
{
	static bool bHash = false;
	static FAutoConsoleVariableRef CVarHash(TEXT("gstn.SimQuery.Hash"), bHash,
		TEXT("Fold every simulation collision query (inputs and answer, bit for bit) into a per-frame hash, to find the first frame where two machines diverge (gstn.SimQuery.Dump)."));

	enum class EKind : uint32
	{
		LineTest = 1, LineSingle, LineMulti, LineSingleObject, SweepTest, SweepSingle, SweepMulti, SweepSingleObject, OverlapTest,
		OverlapMulti, OverlapMultiObject,
	};

	constexpr int32 RingSize = 256;

	struct FFrameHash
	{
		int32 Frame = INDEX_NONE;
		uint32 Hash = 0;
		int32 Queries = 0;
	};

	struct FWorldHashes
	{
		int32 CurrentFrame = INDEX_NONE;
		FFrameHash Ring[RingSize];
	};

	static FCriticalSection Lock;
	static TMap<TObjectKey<UWorld>, FWorldHashes> Worlds;

	/** Bit-exact hash of one query: FNV-1a over the raw bytes, so a difference in the last bit of a float shows */
	struct FQueryHasher
	{
		uint32 Value = 2166136261u;

		void Bytes(const void* Data, int32 Size)
		{
			const uint8* Byte = static_cast<const uint8*>(Data);
			for (int32 Index = 0; Index < Size; ++Index)
			{
				Value = (Value ^ Byte[Index]) * 16777619u;
			}
		}
		template<typename T> void Add(const T& Pod) { Bytes(&Pod, sizeof(T)); }
		void Add(const FVector& V) { Add(V.X); Add(V.Y); Add(V.Z); }
		void Add(const FQuat& Q) { Add(Q.X); Add(Q.Y); Add(Q.Z); Add(Q.W); }

		void Add(const FCollisionShape& Shape)
		{
			Add((uint8)Shape.ShapeType);
			Add(Shape.GetExtent());
		}

		void Add(const FHitResult& Hit)
		{
			// Geometry only: which component was hit is a pointer, different on every machine
			Add((uint8)Hit.bBlockingHit);
			Add((uint8)Hit.bStartPenetrating);
			Add(Hit.Time);
			Add(Hit.Location);
			Add(Hit.ImpactPoint);
			Add(Hit.Normal);
			Add(Hit.ImpactNormal);
			Add(Hit.PenetrationDepth);
			Add(Hit.FaceIndex);
		}

		void Add(const FOverlapResult& Overlap)
		{
			Add((uint8)Overlap.bBlockingHit);
			Add(Overlap.ItemIndex);
		}
	};

	/**
	 * Adds one query to its world's current frame. Queries are summed, not chained, so the hash does not depend on the order queries of
	 * one frame ran in (a parallel step stays comparable); what it detects is a query that was asked or answered differently
	 */
	static void Fold(const UWorld* World, uint32 QueryHash)
	{
		FScopeLock ScopeLock(&Lock);
		FWorldHashes* Hashes = Worlds.Find(World);
		if (!Hashes || Hashes->CurrentFrame == INDEX_NONE)
		{
			return;
		}
		FFrameHash& Entry = Hashes->Ring[Hashes->CurrentFrame % RingSize];
		Entry.Hash += QueryHash;
		++Entry.Queries;
	}

	template<typename FillFunc>
	static void Record(const UWorld* World, EKind Kind, FillFunc&& Fill)
	{
		if (!bHash || !World)
		{
			return;
		}
		FQueryHasher Hasher;
		Hasher.Add((uint32)Kind);
		Fill(Hasher);
		Fold(World, Hasher.Value);
	}

	static FAutoConsoleCommandWithWorldAndArgs DumpCommand(TEXT("gstn.SimQuery.Dump"),
		TEXT("gstn.SimQuery.Dump [Count=60]: log frame, hash and query count of the last Count simulation frames of every game world (compare two machines frame by frame)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			const int32 Count = FMath::Clamp(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 60, 1, RingSize);
			FScopeLock ScopeLock(&Lock);
			for (const TPair<TObjectKey<UWorld>, FWorldHashes>& Pair : Worlds)
			{
				const UWorld* World = Pair.Key.ResolveObjectPtr();
				if (!World)
				{
					continue;
				}
				const TCHAR* Role = World->GetNetMode() == NM_Client ? TEXT("client") : World->GetNetMode() == NM_ListenServer ? TEXT("host") : TEXT("server");
				const int32 Last = Pair.Value.CurrentFrame;
				for (int32 Frame = FMath::Max(0, Last - Count + 1); Frame <= Last; ++Frame)
				{
					const FFrameHash& Entry = Pair.Value.Ring[Frame % RingSize];
					if (Entry.Frame == Frame)
					{
						UE_LOG(LogGSTNSimQuery, Display, TEXT("SimQuery [%s %s] frame %d hash %08x queries %d"), Role, *World->GetName(), Frame, Entry.Hash, Entry.Queries);
					}
				}
			}
		}));
}

using namespace GSTNSimQuery;

bool FGSTN_SimQuery::IsHashEnabled()
{
	return GSTNSimQuery::bHash;
}

void FGSTN_SimQuery::BeginFrame(const UWorld* World, int32 Frame)
{
	if (!GSTNSimQuery::bHash || !World || Frame < 0)
	{
		return;
	}
	FScopeLock ScopeLock(&Lock);
	FWorldHashes& Hashes = Worlds.FindOrAdd(World);
	Hashes.CurrentFrame = Frame;
	FFrameHash& Entry = Hashes.Ring[Frame % RingSize];
	Entry = FFrameHash();
	Entry.Frame = Frame;

	// Worlds that ended (PIE stopped) are dropped here, the only place that adds
	for (auto It = Worlds.CreateIterator(); It; ++It)
	{
		if (!It.Key().ResolveObjectPtr())
		{
			It.RemoveCurrent();
		}
	}
}

bool FGSTN_SimQuery::GetFrameHash(const UWorld* World, int32 Frame, uint32& OutHash, int32& OutQueries)
{
	FScopeLock ScopeLock(&Lock);
	const FWorldHashes* Hashes = Worlds.Find(World);
	if (!Hashes || Frame < 0)
	{
		return false;
	}
	const FFrameHash& Entry = Hashes->Ring[Frame % RingSize];
	if (Entry.Frame != Frame)
	{
		return false;
	}
	OutHash = Entry.Hash;
	OutQueries = Entry.Queries;
	return true;
}

bool FGSTN_SimQuery::LineTraceTestByChannel(const UWorld* World, const FVector& Start, const FVector& End, ECollisionChannel Channel,
	const FCollisionQueryParams& Params, const FCollisionResponseParams& Response)
{
	const bool bResult = World && World->LineTraceTestByChannel(Start, End, Channel, Params, Response);
	Record(World, EKind::LineTest, [&](FQueryHasher& H) { H.Add(Start); H.Add(End); H.Add((uint8)Channel); H.Add((uint8)bResult); });
	return bResult;
}

bool FGSTN_SimQuery::LineTraceSingleByChannel(const UWorld* World, FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel Channel,
	const FCollisionQueryParams& Params, const FCollisionResponseParams& Response)
{
	const bool bResult = World && World->LineTraceSingleByChannel(OutHit, Start, End, Channel, Params, Response);
	Record(World, EKind::LineSingle, [&](FQueryHasher& H) { H.Add(Start); H.Add(End); H.Add((uint8)Channel); H.Add((uint8)bResult); H.Add(OutHit); });
	return bResult;
}

bool FGSTN_SimQuery::LineTraceMultiByChannel(const UWorld* World, TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel Channel,
	const FCollisionQueryParams& Params, const FCollisionResponseParams& Response)
{
	const bool bResult = World && World->LineTraceMultiByChannel(OutHits, Start, End, Channel, Params, Response);
	Record(World, EKind::LineMulti, [&](FQueryHasher& H)
	{
		H.Add(Start); H.Add(End); H.Add((uint8)Channel); H.Add((uint8)bResult); H.Add(OutHits.Num());
		for (const FHitResult& Hit : OutHits) { H.Add(Hit); }
	});
	return bResult;
}

bool FGSTN_SimQuery::LineTraceSingleByObjectType(const UWorld* World, FHitResult& OutHit, const FVector& Start, const FVector& End,
	const FCollisionObjectQueryParams& ObjectParams, const FCollisionQueryParams& Params)
{
	const bool bResult = World && World->LineTraceSingleByObjectType(OutHit, Start, End, ObjectParams, Params);
	Record(World, EKind::LineSingleObject, [&](FQueryHasher& H) { H.Add(Start); H.Add(End); H.Add(ObjectParams.GetQueryBitfield()); H.Add((uint8)bResult); H.Add(OutHit); });
	return bResult;
}

bool FGSTN_SimQuery::SweepTestByChannel(const UWorld* World, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel Channel,
	const FCollisionShape& Shape, const FCollisionQueryParams& Params, const FCollisionResponseParams& Response)
{
	const bool bResult = World && World->SweepTestByChannel(Start, End, Rot, Channel, Shape, Params, Response);
	Record(World, EKind::SweepTest, [&](FQueryHasher& H) { H.Add(Start); H.Add(End); H.Add(Rot); H.Add((uint8)Channel); H.Add(Shape); H.Add((uint8)bResult); });
	return bResult;
}

bool FGSTN_SimQuery::SweepSingleByChannel(const UWorld* World, FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot,
	ECollisionChannel Channel, const FCollisionShape& Shape, const FCollisionQueryParams& Params, const FCollisionResponseParams& Response)
{
	const bool bResult = World && World->SweepSingleByChannel(OutHit, Start, End, Rot, Channel, Shape, Params, Response);
	Record(World, EKind::SweepSingle, [&](FQueryHasher& H) { H.Add(Start); H.Add(End); H.Add(Rot); H.Add((uint8)Channel); H.Add(Shape); H.Add((uint8)bResult); H.Add(OutHit); });
	return bResult;
}

bool FGSTN_SimQuery::SweepMultiByChannel(const UWorld* World, TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot,
	ECollisionChannel Channel, const FCollisionShape& Shape, const FCollisionQueryParams& Params, const FCollisionResponseParams& Response)
{
	const bool bResult = World && World->SweepMultiByChannel(OutHits, Start, End, Rot, Channel, Shape, Params, Response);
	Record(World, EKind::SweepMulti, [&](FQueryHasher& H)
	{
		H.Add(Start); H.Add(End); H.Add(Rot); H.Add((uint8)Channel); H.Add(Shape); H.Add((uint8)bResult); H.Add(OutHits.Num());
		for (const FHitResult& Hit : OutHits) { H.Add(Hit); }
	});
	return bResult;
}

bool FGSTN_SimQuery::SweepSingleByObjectType(const UWorld* World, FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot,
	const FCollisionObjectQueryParams& ObjectParams, const FCollisionShape& Shape, const FCollisionQueryParams& Params)
{
	const bool bResult = World && World->SweepSingleByObjectType(OutHit, Start, End, Rot, ObjectParams, Shape, Params);
	Record(World, EKind::SweepSingleObject, [&](FQueryHasher& H) { H.Add(Start); H.Add(End); H.Add(Rot); H.Add(ObjectParams.GetQueryBitfield()); H.Add(Shape); H.Add((uint8)bResult); H.Add(OutHit); });
	return bResult;
}

bool FGSTN_SimQuery::OverlapBlockingTestByChannel(const UWorld* World, const FVector& Pos, const FQuat& Rot, ECollisionChannel Channel,
	const FCollisionShape& Shape, const FCollisionQueryParams& Params, const FCollisionResponseParams& Response)
{
	const bool bResult = World && World->OverlapBlockingTestByChannel(Pos, Rot, Channel, Shape, Params, Response);
	Record(World, EKind::OverlapTest, [&](FQueryHasher& H) { H.Add(Pos); H.Add(Rot); H.Add((uint8)Channel); H.Add(Shape); H.Add((uint8)bResult); });
	return bResult;
}

bool FGSTN_SimQuery::OverlapMultiByChannel(const UWorld* World, TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot,
	ECollisionChannel Channel, const FCollisionShape& Shape, const FCollisionQueryParams& Params, const FCollisionResponseParams& Response)
{
	const bool bResult = World && World->OverlapMultiByChannel(OutOverlaps, Pos, Rot, Channel, Shape, Params, Response);
	Record(World, EKind::OverlapMulti, [&](FQueryHasher& H)
	{
		H.Add(Pos); H.Add(Rot); H.Add((uint8)Channel); H.Add(Shape); H.Add((uint8)bResult); H.Add(OutOverlaps.Num());
		for (const FOverlapResult& Overlap : OutOverlaps) { H.Add(Overlap); }
	});
	return bResult;
}

bool FGSTN_SimQuery::OverlapMultiByObjectType(const UWorld* World, TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot,
	const FCollisionObjectQueryParams& ObjectParams, const FCollisionShape& Shape, const FCollisionQueryParams& Params)
{
	const bool bResult = World && World->OverlapMultiByObjectType(OutOverlaps, Pos, Rot, ObjectParams, Shape, Params);
	Record(World, EKind::OverlapMultiObject, [&](FQueryHasher& H)
	{
		H.Add(Pos); H.Add(Rot); H.Add(ObjectParams.GetQueryBitfield()); H.Add(Shape); H.Add((uint8)bResult); H.Add(OutOverlaps.Num());
		for (const FOverlapResult& Overlap : OutOverlaps) { H.Add(Overlap); }
	});
	return bResult;
}
