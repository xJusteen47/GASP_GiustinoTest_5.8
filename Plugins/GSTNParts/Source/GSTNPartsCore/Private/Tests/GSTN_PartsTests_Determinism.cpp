// Determinism and allocation tests of the part math (plan P3): the same inputs give the same bits on every machine and the hit
// path allocates nothing.

#include "Data/GSTN_PartBreakEffects.h"
#include "GSTN_PartsTestUtils.h"
#include "HAL/Memory.h"
#include "HAL/MemoryBase.h"
#include "HAL/PlatformTLS.h"
#include "Math/GSTN_PartMath.h"
#include "Math/RandomStream.h"
#include <atomic>

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPartsTest
{
	/** FNV-1a over raw bytes: a stable fingerprint of a whole run */
	void Mix(uint64& Hash, const void* Data, SIZE_T Size)
	{
		const uint8* Bytes = static_cast<const uint8*>(Data);
		for (SIZE_T Index = 0; Index < Size; ++Index)
		{
			Hash ^= Bytes[Index];
			Hash *= 1099511628211ull;
		}
	}

	template <typename T>
	void MixValue(uint64& Hash, const T& Value)
	{
		Mix(Hash, &Value, sizeof(T));
	}

	/** A part set from a seed: random hardness, caps (some unbreakable), transfers, policies and parents that always point to a higher index */
	TArray<FGSTN_PartDefinition> MakeRandomParts(int32 Seed, int32 Num)
	{
		FRandomStream Stream(Seed);
		const float Caps[] = { 0.f, 25.f, 60.f, 100.f, 250.f };
		const float Transfers[] = { 0.25f, 0.5f, 1.f, 2.f };
		const float Scales[] = { 0.5f, 1.f, 2.f };
		const float Lost[] = { 0.f, 0.1f, 0.25f };

		TArray<FGSTN_PartDefinition> Parts;
		for (int32 Index = 0; Index < Num; ++Index)
		{
			FGSTN_PartDefinition Part = MakePart(*FString::Printf(TEXT("P%d"), Index), { *FString::Printf(TEXT("H%d"), Index) },
				Stream.RandRange(0, 15), Caps[Stream.RandRange(0, 4)], Transfers[Stream.RandRange(0, 3)]);
			Part.PartDamageScale = Scales[Stream.RandRange(0, 2)];
			Part.LostMassFraction = Lost[Stream.RandRange(0, 2)];
			Part.BrokenPolicy = static_cast<EGSTN_BrokenHurtboxPolicy>(Index % 3);
			Part.ParentIndex = (Index < Num - 1 && Stream.RandRange(0, 2) > 0) ? Stream.RandRange(Index + 1, Num - 1) : INDEX_NONE;
			if (Index % 4 == 0)
			{
				FGSTN_PartBreakEffect_MainDamage Bonus;
				Bonus.AddPercent = static_cast<float>(Index);
				Bonus.bKill = Index % 8 == 0;
				Part.OnBreak.Add(FInstancedStruct::Make(Bonus));
			}
			Parts.Add(Part);
		}
		return Parts;
	}

	/** A hit made only of values that are exact in binary (quarters), so the products are the same wherever they are computed */
	FGSTN_PartHitInput MakeRandomHit(FRandomStream& Stream, int32 NumParts)
	{
		const float Scales[] = { 0.5f, 1.f, 2.f };
		FGSTN_PartHitInput Input;
		Input.Damage = static_cast<float>(Stream.RandRange(0, 320)) * 0.25f;
		Input.PartIndex = Stream.RandRange(-1, NumParts); // the body, a part, or one past the end
		Input.Penetration = Stream.RandRange(0, 15);
		Input.PartDamageScale = Scales[Stream.RandRange(0, 2)];
		Input.bIgnoreHardness = Stream.FRand() < 0.1f;
		Input.bMainOnly = Stream.FRand() < 0.1f;
		return Input;
	}

	/** Plays one sequence of hits on a fresh state, folding every state and result into the hash. Returns false (with Reason) on a broken invariant */
	bool PlaySequence(const TArray<FGSTN_PartDefinition>& Parts, TConstArrayView<FGSTN_PenetrationBand> Bands, int32 Seed, int32 Hits,
		uint64& Hash, FString& Reason)
	{
		FRandomStream Stream(Seed);
		FGSTN_PartsState State;
		for (int32 HitIndex = 0; HitIndex < Hits; ++HitIndex)
		{
			const FGSTN_PartsState Old = State;
			const FGSTN_PartHitInput Input = MakeRandomHit(Stream, Parts.Num());
			const FGSTN_PartHitResult R = UGSTN_PartMath::ApplyHit(Parts, Bands, State, Input);

			// Folded in field by field: padding bytes of the structs are not part of the fingerprint
			MixValue(Hash, State.BrokenMask);
			Mix(Hash, State.PartDamage, sizeof(State.PartDamage));
			MixValue(Hash, State.BreakSerial);
			MixValue(Hash, R.PartIndex);
			MixValue(Hash, R.BandIndex);
			MixValue(Hash, R.NewlyBrokenMask);
			MixValue(Hash, R.NewPartDamage);
			MixValue(Hash, R.PartDelta);
			MixValue(Hash, R.HitMainDelta);
			MixValue(Hash, R.BonusMainDamage);
			MixValue(Hash, R.MainDelta);
			MixValue(Hash, R.StaggerScale);
			MixValue(Hash, R.HitstopScale);
			const uint8 ResultFlags = (R.bRicochet ? 1 : 0) | (R.bPassedToBody ? 2 : 0) | (R.bHitBrokenPart ? 4 : 0) | (R.bPartDamaged ? 8 : 0)
				| (R.bWeakPointHit ? 16 : 0) | (R.bKill ? 32 : 0) | (R.bAllBrokenReached ? 64 : 0);
			MixValue(Hash, ResultFlags);

			// Invariants of the rule
			if ((State.BrokenMask & Old.BrokenMask) != Old.BrokenMask)
			{
				Reason = FString::Printf(TEXT("hit %d: the mask lost a bit"), HitIndex);
				return false;
			}
			if (R.NewlyBrokenMask != static_cast<uint16>(State.BrokenMask & ~Old.BrokenMask))
			{
				Reason = FString::Printf(TEXT("hit %d: newly broken disagrees with the mask"), HitIndex);
				return false;
			}
			if (State.BreakSerial != ((Old.BreakSerial + (R.NewlyBrokenMask != 0 ? 1 : 0)) & 15))
			{
				Reason = FString::Printf(TEXT("hit %d: the serial did not follow the breaks"), HitIndex);
				return false;
			}
			if (UGSTN_PartMath::ComputeCascade(Parts, State.BrokenMask) != State.BrokenMask)
			{
				Reason = FString::Printf(TEXT("hit %d: a child outlived its broken parent"), HitIndex);
				return false;
			}
			if (!(FMath::IsFinite(R.MainDelta) && R.MainDelta >= 0.f))
			{
				Reason = FString::Printf(TEXT("hit %d: main damage %g is not a finite non-negative number"), HitIndex, R.MainDelta);
				return false;
			}
			if (R.bRicochet && State != Old)
			{
				Reason = FString::Printf(TEXT("hit %d: a ricochet changed the state"), HitIndex);
				return false;
			}
			for (int32 Part = 0; Part < GSTN_PartsLimits::MaxParts; ++Part)
			{
				const bool bPartExists = Part < Parts.Num();
				const bool bBroken = State.IsBroken(Part);
				if (State.PartDamage[Part] < Old.PartDamage[Part] && !bBroken)
				{
					Reason = FString::Printf(TEXT("hit %d: part %d healed"), HitIndex, Part);
					return false;
				}
				if (!bPartExists && (bBroken || State.PartDamage[Part] != 0))
				{
					Reason = FString::Printf(TEXT("hit %d: a part that does not exist changed"), HitIndex);
					return false;
				}
				if (bPartExists && bBroken != (State.PartDamage[Part] == 255))
				{
					Reason = FString::Printf(TEXT("hit %d: part %d broken flag and byte disagree"), HitIndex, Part);
					return false;
				}
				if (bPartExists && !bBroken && !Parts[Part].IsBreakable() && State.PartDamage[Part] != 0)
				{
					Reason = FString::Printf(TEXT("hit %d: unbreakable part %d took damage"), HitIndex, Part);
					return false;
				}
			}
		}
		return true;
	}

	/** Pinned once from a run, then checked on every machine and build configuration: it must not change unless the rule does */
	constexpr uint64 GoldenHash = 0x827A1F0F6B36481Aull;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsRandomSequencesTest, "GSTNParts.Core.Determinism.1000RandomSequencesTwoConfigs", GSTNPartsTest::Flags)
bool FGSTN_PartsRandomSequencesTest::RunTest(const FString& Parameters)
{
	TArray<FGSTN_PartDefinition> Golden = GSTNPartsTest::MakeEnemyParts();
	GSTNPartsTest::AddMainDamage(Golden[1], 10.f);
	const TArray<TArray<FGSTN_PartDefinition>> Configs = { Golden, GSTNPartsTest::MakeRandomParts(20261023, 12) };
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();

	uint64 AllHash = 14695981039346656037ull;
	for (int32 ConfigIndex = 0; ConfigIndex < Configs.Num(); ++ConfigIndex)
	{
		for (int32 Sequence = 0; Sequence < 1000; ++Sequence)
		{
			const int32 Seed = (Sequence + 1) * 7919 + ConfigIndex * 104729;
			uint64 First = 14695981039346656037ull;
			uint64 Second = 14695981039346656037ull;
			FString Reason;
			if (!GSTNPartsTest::PlaySequence(Configs[ConfigIndex], Bands, Seed, 40, First, Reason))
			{
				AddError(FString::Printf(TEXT("Config %d sequence %d: %s"), ConfigIndex, Sequence, *Reason));
				return false;
			}
			GSTNPartsTest::PlaySequence(Configs[ConfigIndex], Bands, Seed, 40, Second, Reason);
			if (First != Second)
			{
				AddError(FString::Printf(TEXT("Config %d sequence %d: two runs of the same inputs differ"), ConfigIndex, Sequence));
				return false;
			}
			GSTNPartsTest::MixValue(AllHash, First);
		}
	}

	AddInfo(FString::Printf(TEXT("Fingerprint of 2000 sequences: 0x%016llX"), AllHash));
	TestTrue(TEXT("The fingerprint equals the pinned one (same bits as every other machine and build configuration)"), AllHash == GSTNPartsTest::GoldenHash);
	return true;
}

namespace GSTNPartsTest
{
	/**
	 * A pass-through allocator that counts the allocations made by one thread while it is switched on. Installed the way the engine's
	 * own debugging proxies are (a compare and swap on the global allocator pointer), and never destroyed, so a thread that is in
	 * the middle of a call while it is removed still finds it alive
	 */
	class FCountingMalloc : public FMalloc
	{
	public:
		explicit FCountingMalloc(FMalloc* InUsed) : Used(InUsed) {}

		std::atomic<uint32> Watched{ 0 };
		std::atomic<int32> Count{ 0 };

		void Note()
		{
			if (Watched.load(std::memory_order_relaxed) != 0 && Watched.load(std::memory_order_relaxed) == FPlatformTLS::GetCurrentThreadId())
			{
				Count.fetch_add(1, std::memory_order_relaxed);
			}
		}

		virtual void* Malloc(SIZE_T Size, uint32 Alignment) override { Note(); return Used->Malloc(Size, Alignment); }
		virtual void* TryMalloc(SIZE_T Size, uint32 Alignment) override { Note(); return Used->TryMalloc(Size, Alignment); }
		virtual void* Realloc(void* Ptr, SIZE_T Size, uint32 Alignment) override { Note(); return Used->Realloc(Ptr, Size, Alignment); }
		virtual void* TryRealloc(void* Ptr, SIZE_T Size, uint32 Alignment) override { Note(); return Used->TryRealloc(Ptr, Size, Alignment); }
		virtual void Free(void* Ptr) override { Used->Free(Ptr); }
		virtual void* MallocZeroed(SIZE_T Size, uint32 Alignment) override { Note(); return Used->MallocZeroed(Size, Alignment); }
		virtual void* TryMallocZeroed(SIZE_T Size, uint32 Alignment) override { Note(); return Used->TryMallocZeroed(Size, Alignment); }
		virtual SIZE_T QuantizeSize(SIZE_T Size, uint32 Alignment) override { return Used->QuantizeSize(Size, Alignment); }
		virtual bool GetAllocationSize(void* Original, SIZE_T& SizeOut) override { return Used->GetAllocationSize(Original, SizeOut); }
		virtual void Trim(bool bTrimThreadCaches) override { Used->Trim(bTrimThreadCaches); }
		virtual void SetupTLSCachesOnCurrentThread() override { Used->SetupTLSCachesOnCurrentThread(); }
		virtual void MarkTLSCachesAsUsedOnCurrentThread() override { Used->MarkTLSCachesAsUsedOnCurrentThread(); }
		virtual void MarkTLSCachesAsUnusedOnCurrentThread() override { Used->MarkTLSCachesAsUnusedOnCurrentThread(); }
		virtual void ClearAndDisableTLSCachesOnCurrentThread() override { Used->ClearAndDisableTLSCachesOnCurrentThread(); }
		virtual void InitializeStatsMetadata() override { Used->InitializeStatsMetadata(); }
		virtual void UpdateStats() override { Used->UpdateStats(); }
		virtual void GetAllocatorStats(FGenericMemoryStats& OutStats) override { Used->GetAllocatorStats(OutStats); }
		virtual void DumpAllocatorStats(FOutputDevice& Ar) override { Used->DumpAllocatorStats(Ar); }
		virtual bool IsInternallyThreadSafe() const override { return Used->IsInternallyThreadSafe(); }
		virtual bool ValidateHeap() override { return Used->ValidateHeap(); }
#if UE_ALLOW_EXEC_COMMANDS
		virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override { return Used->Exec(InWorld, Cmd, Ar); }
#endif
		virtual const TCHAR* GetDescriptiveName() override { return Used->GetDescriptiveName(); }
		virtual void OnMallocInitialized() override { Used->OnMallocInitialized(); }
		virtual void OnPreFork() override { Used->OnPreFork(); }
		virtual void OnPostFork() override { Used->OnPostFork(); }
		virtual uint64 GetImmediatelyFreeableCachedMemorySize() const override { return Used->GetImmediatelyFreeableCachedMemorySize(); }
		virtual uint64 GetTotalFreeCachedMemorySize() const override { return Used->GetTotalFreeCachedMemorySize(); }

	private:
		FMalloc* Used;
	};

	/** Counts the allocations Work makes on this thread; -1 when the counting allocator cannot be installed on this platform */
	int32 CountAllocations(TFunctionRef<void()> Work)
	{
#if defined(PLATFORM_USES_FIXED_GMalloc_CLASS) && PLATFORM_USES_FIXED_GMalloc_CLASS
		return -1;
#else
		static FCountingMalloc* Proxy = nullptr;
		FMalloc* Original = UE::Private::GMalloc;
		if (!Proxy)
		{
			Proxy = new FCountingMalloc(Original); // FMalloc's operator new uses the system allocator, not the global one
		}
		if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&UE::Private::GMalloc, Proxy, Original) != Original)
		{
			return -1;
		}

		Proxy->Count.store(0);
		Proxy->Watched.store(FPlatformTLS::GetCurrentThreadId());
		Work();
		Proxy->Watched.store(0);
		const int32 Result = Proxy->Count.load();

		FPlatformAtomics::InterlockedCompareExchangePointer((void**)&UE::Private::GMalloc, Original, Proxy);
		return Result;
#endif
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsAllocationTest, "GSTNParts.Core.Math.ApplyHitAllocatesNothing", GSTNPartsTest::Flags)
bool FGSTN_PartsAllocationTest::RunTest(const FString& Parameters)
{
	// Everything the hit path needs exists before the count starts
	TArray<FGSTN_PartDefinition> Parts = GSTNPartsTest::MakeEnemyParts();
	GSTNPartsTest::AddMainDamage(Parts[1], 10.f);
	const TArray<FGSTN_PenetrationBand> Bands = UGSTN_PenetrationTable::MakeDefaultBands();
	FGSTN_PartsState State;
	FRandomStream Stream(77);
	TArray<FGSTN_PartHitInput> Hits;
	for (int32 Index = 0; Index < 4000; ++Index)
	{
		Hits.Add(GSTNPartsTest::MakeRandomHit(Stream, Parts.Num()));
	}
	volatile float Sink = 0.f;

	// The control: the counter must see an allocation when there is one, or a zero below means nothing
	const int32 Control = GSTNPartsTest::CountAllocations([]()
	{
		TArray<int32> Grow;
		for (int32 Index = 0; Index < 100; ++Index)
		{
			Grow.Add(Index);
		}
	});
	if (Control < 0)
	{
		AddWarning(TEXT("The counting allocator cannot be installed on this platform; the allocation test is skipped"));
		return true;
	}
	TestTrue(TEXT("The counter sees the allocations of a growing array"), Control > 0);

	const int32 Allocations = GSTNPartsTest::CountAllocations([&]()
	{
		for (int32 Repeat = 0; Repeat < 5; ++Repeat)
		{
			State = FGSTN_PartsState();
			for (const FGSTN_PartHitInput& Hit : Hits)
			{
				Sink = Sink + UGSTN_PartMath::ApplyHit(Parts, Bands, State, Hit).MainDelta;
			}
		}
	});
	TestEqual(TEXT("Twenty thousand hits (breaks, cascades, ricochets, body hits) allocate nothing"), Allocations, 0);

	// The helpers on the same path
	const int32 HelperAllocations = GSTNPartsTest::CountAllocations([&]()
	{
		const FGSTN_PartsBakedCurve Curve = FGSTN_PartsBakedCurve::MakeLinear(1.f, 0.6f);
		for (int32 Index = 0; Index < 2000; ++Index)
		{
			Sink = Sink + UGSTN_PartMath::EffectiveWeight(80.f, Parts, Curve, Index / 2000.f, static_cast<uint16>(Index & 15), 0.35f);
			Sink = Sink + static_cast<float>(UGSTN_PartMath::ComputeCascade(Parts, static_cast<uint16>(Index & 15)));
			Sink = Sink + static_cast<float>(UGSTN_PartMath::AddDamage(static_cast<uint8>(Index & 127), 3.f, 40.f));
			Sink = Sink + (UGSTN_PartMath::IsGrabLocked(FGSTN_GrabGate(), static_cast<uint16>(Index)) ? 1.f : 0.f);
		}
	});
	TestEqual(TEXT("The weight, cascade and quantise helpers allocate nothing"), HelperAllocations, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
