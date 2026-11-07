// Tests of the pure replication helpers of G6: the tier scheduler, the codec bounds with the optional type and Teleport, the dirty
// rule, the budgeted relevancy and the registry hash. No world.

#include "GSTN_MinionTestUtils.h"
#include "Net/GSTN_MinionNetCodec.h"
#include "Net/GSTN_MinionNetRates.h"
#include "Net/GSTN_MinionNetRelevancy.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** The GCRA bound: at most this many sends between two sends Span frames apart (both included) */
	int32 MaxSendsInSpan(int32 SpanFrames, int32 RateHz)
	{
		const int64 Period = GSTNMinionNetRate::PeriodMilliFrames(RateHz);
		return 1 + static_cast<int32>((static_cast<int64>(SpanFrames) * 1000 + GSTNMinionNetRate::ToleranceMilliFrames) / Period);
	}

	/** Checks the bound on every pair of sends of one send list (frames ascending). Returns the first violation, or an empty string */
	FString CheckRateBound(const TArray<int32>& SendFrames, int32 RateHz)
	{
		for (int32 First = 0; First < SendFrames.Num(); ++First)
		{
			for (int32 Last = First; Last < SendFrames.Num(); ++Last)
			{
				const int32 Count = Last - First + 1;
				const int32 Span = SendFrames[Last] - SendFrames[First];
				if (Count > MaxSendsInSpan(Span, RateHz))
				{
					return FString::Printf(TEXT("%d sends in %d frames at %d Hz (frames %d..%d)"), Count, Span, RateHz, SendFrames[First], SendFrames[Last]);
				}
			}
		}
		return FString();
	}

	FGSTN_MinionPackCandidate Candidate(int32 PackId, int32 Items, float Distance, bool bAttacking = false, bool bHit = false)
	{
		FGSTN_MinionPackCandidate C;
		C.PackId = PackId;
		C.ItemCount = Items;
		C.PlannedBytesPerSecond = Items * GSTNMinionNetRate::PlannedBytesPerSecond(EGSTN_MinionLODTier::Combat);
		C.NearestDistance = Distance;
		C.bAttacking = bAttacking;
		C.bRecentlyHit = bHit;
		return C;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionNetSchedulerRateTest, "GSTNMinion.Core.Net.SchedulerNeverExceedsItemRateOrPackCeiling", GSTNMinionTest::Flags)
bool FGSTN_MinionNetSchedulerRateTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("T0 15 Hz"), GSTNMinionNetRate::RateHz(EGSTN_MinionLODTier::Combat), 15);
	TestEqual(TEXT("T1 5 Hz"), GSTNMinionNetRate::RateHz(EGSTN_MinionLODTier::Near), 5);
	TestEqual(TEXT("T2 1 Hz"), GSTNMinionNetRate::RateHz(EGSTN_MinionLODTier::Far), 1);
	TestEqual(TEXT("T3 never"), GSTNMinionNetRate::RateHz(EGSTN_MinionLODTier::Dormant), 0);

	const uint8 MixedTiers[] = { 1, 2, 1, 3 };
	TestEqual(TEXT("ceiling = best member rate"), GSTNMinionNetRate::PackCeilingHz(MixedTiers), 5);
	const uint8 Asleep[] = { 3, 3 };
	TestEqual(TEXT("all T3 = ceiling 0"), GSTNMinionNetRate::PackCeilingHz(Asleep), 0);

	// A 16-item pack with a random tier mix and a random dirty pattern, 40 s of frames; one run with every item always dirty
	for (int32 Run = 0; Run < 4; ++Run)
	{
		FRandomStream Random(100 + Run);
		constexpr int32 Items = 16;
		constexpr int32 Frames = 2500;
		TArray<uint8> Tiers;
		Tiers.SetNum(Items);
		for (int32 Item = 0; Item < Items; ++Item)
		{
			// Run 0: all T0 (the ceiling is the item rate); later runs mix every tier
			Tiers[Item] = static_cast<uint8>(Run == 0 ? 0 : Random.RandRange(0, 3));
		}
		const int32 Ceiling = GSTNMinionNetRate::PackCeilingHz(Tiers);
		const float DirtyChance = Run <= 1 ? 1.f : 0.3f;

		FGSTN_MinionNetRateLimiter PackLimiter;
		TArray<FGSTN_MinionNetRateLimiter> ItemLimiters;
		ItemLimiters.SetNum(Items);
		TArray<TArray<int32>> ItemSends;
		ItemSends.SetNum(Items);
		TArray<int32> PackSends;
		TArray<bool> Dirty;
		Dirty.SetNum(Items);
		TArray<int32> Sent;
		for (int32 Frame = 1000; Frame < 1000 + Frames; ++Frame)
		{
			for (int32 Item = 0; Item < Items; ++Item)
			{
				Dirty[Item] = Random.FRand() < DirtyChance;
			}
			Sent.Reset();
			if (GSTNMinionNetRate::SelectPackSends(Frame, PackLimiter, Ceiling, Tiers, Dirty, ItemLimiters, Sent) > 0)
			{
				PackSends.Add(Frame);
			}
			for (const int32 Item : Sent)
			{
				ItemSends[Item].Add(Frame);
			}
		}

		for (int32 Item = 0; Item < Items; ++Item)
		{
			const int32 Rate = GSTNMinionNetRate::RateHz(static_cast<EGSTN_MinionLODTier>(Tiers[Item]));
			if (Rate == 0)
			{
				TestEqual(FString::Printf(TEXT("Run %d item %d (T3) never sent"), Run, Item), ItemSends[Item].Num(), 0);
				continue;
			}
			const FString Violation = CheckRateBound(ItemSends[Item], Rate);
			TestTrue(FString::Printf(TEXT("Run %d item %d keeps its %d Hz: %s"), Run, Item, Rate, *Violation), Violation.IsEmpty());
			const FString CeilingViolation = CheckRateBound(ItemSends[Item], Ceiling);
			TestTrue(FString::Printf(TEXT("Run %d item %d keeps the pack ceiling: %s"), Run, Item, *CeilingViolation), CeilingViolation.IsEmpty());
			if (Run <= 1)
			{
				// Always dirty: the item is not starved (at least 80 % of its nominal rate: a slower item waits for the next pack frame over 40 s)
				TestTrue(FString::Printf(TEXT("Run %d item %d gets its rate (%d sends)"), Run, Item, ItemSends[Item].Num()),
					ItemSends[Item].Num() * 10 >= Rate * 40 * 8);
			}
		}
		const FString PackViolation = CheckRateBound(PackSends, FMath::Max(Ceiling, 1));
		TestTrue(FString::Printf(TEXT("Run %d pack frames keep the %d Hz ceiling: %s"), Run, Ceiling, *PackViolation), Ceiling == 0 ? PackSends.Num() == 0 : PackViolation.IsEmpty());
	}

	// Deterministic: the same inputs give the same sends
	FGSTN_MinionNetRateLimiter A;
	FGSTN_MinionNetRateLimiter B;
	for (int32 Frame = 0; Frame < 200; ++Frame)
	{
		const bool bA = A.CanSend(Frame, 15);
		const bool bB = B.CanSend(Frame, 15);
		TestTrue(TEXT("same frame, same decision"), bA == bB);
		if (bA)
		{
			A.OnSent(Frame, 15);
			B.OnSent(Frame, 15);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionNetCodecTypeTeleportTest, "GSTNMinion.Core.Net.CodecBoundsWithTypeAndTeleport", GSTNMinionTest::Flags)
bool FGSTN_MinionNetCodecTypeTeleportTest::RunTest(const FString& Parameters)
{
	FGSTN_MinionNetQuantization Quant;
	Quant.WorldOrigin = FVector3f(1000.f, -2000.f, 50.f);
	Quant.MaxSpeed = 450.f;
	const FGSTN_MinionNetFormat Format;

	// Positions beyond the 19-bit range clamp to its ends instead of wrapping (a minion past 2.6 km reads at the border, not across the map)
	const float Range = static_cast<float>(GSTNMinionQuant::PositionBias);
	const float Probes[] = { -Range * 2.f, -Range, -Range + 0.4f, 0.f, Range - 1.f, Range * 3.f };
	for (const float Offset : Probes)
	{
		for (const bool bHasType : { false, true })
		{
			for (const bool bTeleport : { false, true })
			{
				FGSTN_MinionNetState State;
				State.Position = Quant.WorldOrigin + FVector3f(Offset, -Offset, Offset * 0.5f);
				State.YawDegrees = 359.9f;
				State.FrameStamp = 4096;
				State.MoveId = 63;
				State.MoveStartFrame = 4096 - 1023;
				State.HitCounter = 7;
				State.HealthFraction = 1.f;
				State.Speed = 10000.f;
				State.ActionState = 15;
				State.Flags = bTeleport ? GSTNMinion::NetFlags::Teleport : 0;

				FGSTN_MinionNetWireItem Item;
				FGSTN_MinionNetCodec::Quantize(State, Quant, Item);
				Item.Id = 15;
				Item.bHasType = bHasType;
				Item.TypeIndex = bHasType ? 255 : 0;

				FBitWriter Writer(512, true);
				FGSTN_MinionNetWireItem Copy = Item;
				bool bOk = FGSTN_MinionNetCodec::Serialize(Writer, Copy, Format);
				FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
				FGSTN_MinionNetWireItem Read;
				bOk &= FGSTN_MinionNetCodec::Serialize(Reader, Read, Format);
				const FString Case = FString::Printf(TEXT("offset %.1f type %d teleport %d"), Offset, bHasType ? 1 : 0, bTeleport ? 1 : 0);
				TestTrue(Case + TEXT(": lossless round trip"), bOk && Reader.AtEnd() && Read == Item);
				TestEqual(Case + TEXT(": bHasType costs exactly its 8 bits"), static_cast<int64>(FGSTN_MinionNetCodec::GetBitSize(Item, Format)),
					static_cast<int64>(Writer.GetNumBits()));
				TestTrue(Case + TEXT(": Teleport survives"), ((Read.Flags & GSTNMinion::NetFlags::Teleport) != 0) == bTeleport);
				TestEqual(Case + TEXT(": type survives"), static_cast<int32>(Read.TypeIndex), bHasType ? 255 : 0);

				for (int32 Axis = 0; Axis < 3; ++Axis)
				{
					TestTrue(Case + TEXT(": position inside the 19-bit range"), Read.PositionQ[Axis] <= static_cast<uint32>(GSTNMinionQuant::PositionMaxValue));
				}
				const FGSTN_MinionNetState Back = FGSTN_MinionNetCodec::Dequantize(Read, Quant, 4096);
				if (FMath::Abs(Offset) < Range - 1.f)
				{
					TestTrue(Case + TEXT(": in-range X within half a step"), FMath::Abs(Back.Position.X - State.Position.X) <= GSTNMinionQuant::PositionMaxError + 0.01f);
				}
				TestTrue(Case + TEXT(": speed clamps to the 6-bit top"), Read.SpeedQ == 63);
				TestEqual(Case + TEXT(": frame stamp rebuilt"), Back.FrameStamp, 4096);
				TestEqual(Case + TEXT(": move start rebuilt at the 10-bit edge"), Back.MoveStartFrame, 4096 - 1023);
			}
		}
	}

	FGSTN_MinionNetWireItem NoType;
	FGSTN_MinionNetWireItem WithType;
	WithType.bHasType = true;
	TestEqual(TEXT("bHasType adds the 8-bit type"), FGSTN_MinionNetCodec::GetBitSize(WithType, Format) - FGSTN_MinionNetCodec::GetBitSize(NoType, Format),
		GSTNMinion::NetBits::TypeIndex);
	TestTrue(TEXT("an item with type stays under the 20 B planning size with the 3 B FastArray header"),
		FGSTN_MinionNetCodec::GetBitSize(WithType, Format) + 24 <= GSTNMinionNetRate::PlanningBytesPerItem * 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionNetDirtyRuleTest, "GSTNMinion.Core.Net.DirtyRule", GSTNMinionTest::Flags)
bool FGSTN_MinionNetDirtyRuleTest::RunTest(const FString& Parameters)
{
	FGSTN_MinionNetWireItem Sent;
	Sent.PositionQ[0] = Sent.PositionQ[1] = Sent.PositionQ[2] = 1000;
	Sent.YawQ = 1;

	FGSTN_MinionNetWireItem Same = Sent;
	Same.FrameStampLow = 200;
	TestFalse(TEXT("a new frame stamp alone is not a change"), GSTNMinionNetRate::IsSignificantChange(Sent, Same));

	FGSTN_MinionNetWireItem Small = Sent;
	Small.PositionQ[0] += 2;
	Small.PositionQ[1] += 2;
	TestFalse(TEXT("2.8 cm is not a move"), GSTNMinionNetRate::IsSignificantChange(Sent, Small));
	Small.PositionQ[2] += 2;
	TestTrue(TEXT("3.5 cm is a move"), GSTNMinionNetRate::IsSignificantChange(Sent, Small));

	FGSTN_MinionNetWireItem Turned = Sent;
	Turned.YawQ = 255;
	TestFalse(TEXT("2 steps across the wrap is under 4 degrees"), GSTNMinionNetRate::IsSignificantChange(Sent, Turned));
	Turned.YawQ = 254;
	TestTrue(TEXT("3 steps across the wrap is over 4 degrees"), GSTNMinionNetRate::IsSignificantChange(Sent, Turned));

	FGSTN_MinionNetWireItem Hit = Sent;
	Hit.HitCounter = 1;
	TestTrue(TEXT("any other field is a change"), GSTNMinionNetRate::IsSignificantChange(Sent, Hit));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionNetRelevancyBudgetTest, "GSTNMinion.Core.Net.RelevancyBudgetRanking", GSTNMinionTest::Flags)
bool FGSTN_MinionNetRelevancyBudgetTest::RunTest(const FString& Parameters)
{
	FGSTN_MinionRelevancyParams Params;
	Params.MaxItems = 64;
	Params.BudgetBytesPerSecond = 1000000;

	// Six 16-item packs in range: only four fit 64 items. The attacking one and the hit one go first whatever their distance
	TArray<FGSTN_MinionPackCandidate> Candidates = {
		Candidate(0, 16, 500.f), Candidate(1, 16, 7000.f, true), Candidate(2, 16, 1000.f), Candidate(3, 16, 6000.f, false, true),
		Candidate(4, 16, 300.f), Candidate(5, 16, 9000.f) };
	FGSTN_MinionRelevancyState State;
	FGSTN_MinionRelevancyResult Result = FGSTN_MinionRelevancyMath::Evaluate(Params, Candidates, 100, State);
	TestEqual(TEXT("rank 0 is the attacking pack"), Candidates[0].PackId, 1);
	TestEqual(TEXT("rank 1 is the recently hit pack"), Candidates[1].PackId, 3);
	TestEqual(TEXT("then by distance"), Candidates[2].PackId, 4);
	TestEqual(TEXT("four packs granted (64 items)"), Result.Granted, 4);
	TestEqual(TEXT("64 items"), Result.GrantedItems, 64);
	TestTrue(TEXT("attacking granted"), State.IsGranted(1));
	TestTrue(TEXT("hit granted"), State.IsGranted(3));
	TestTrue(TEXT("nearest granted"), State.IsGranted(4) && State.IsGranted(0));
	TestFalse(TEXT("the fifth in range is denied by the item budget"), State.IsGranted(2));
	TestEqual(TEXT("one denied by budget"), Result.DeniedByBudget, 1);
	TestFalse(TEXT("beyond 8000 cm is never granted"), State.IsGranted(5));
	TestEqual(TEXT("one out of range"), Result.OutOfRange, 1);

	// The byte budget binds too: 300 B/s per T0 item, 10 items = 3000 B/s
	FGSTN_MinionRelevancyParams Bytes;
	Bytes.BudgetBytesPerSecond = 3000;
	TArray<FGSTN_MinionPackCandidate> Small = { Candidate(0, 8, 100.f), Candidate(1, 4, 200.f), Candidate(2, 2, 300.f) };
	FGSTN_MinionRelevancyState BytesState;
	const FGSTN_MinionRelevancyResult BytesResult = FGSTN_MinionRelevancyMath::Evaluate(Bytes, Small, 0, BytesState);
	TestTrue(TEXT("8 items fit"), BytesState.IsGranted(0));
	TestFalse(TEXT("4 more do not (3600 B/s)"), BytesState.IsGranted(1));
	TestTrue(TEXT("a smaller pack after it still fits (3000 B/s)"), BytesState.IsGranted(2));
	TestEqual(TEXT("granted bytes"), BytesResult.GrantedBytesPerSecond, 3000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionNetRelevancyHysteresisTest, "GSTNMinion.Core.Net.RelevancyHysteresis", GSTNMinionTest::Flags)
bool FGSTN_MinionNetRelevancyHysteresisTest::RunTest(const FString& Parameters)
{
	FGSTN_MinionRelevancyParams Params;
	FGSTN_MinionRelevancyState State;

	// A pack crossing the 8000 cm border back and forth every evaluation (6 frames, ~100 ms) for 10 s toggles at most once per second
	int32 Toggles = 0;
	int32 LastToggle = INDEX_NONE;
	bool bWasGranted = false;
	bool bSpacingOk = true;
	for (int32 Frame = 0; Frame < 625; Frame += 6)
	{
		TArray<FGSTN_MinionPackCandidate> Candidates = { Candidate(7, 4, (Frame / 6) % 2 == 0 ? 7900.f : 8100.f) };
		FGSTN_MinionRelevancyMath::Evaluate(Params, Candidates, Frame, State);
		const bool bGranted = State.IsGranted(7);
		if (bGranted != bWasGranted)
		{
			bSpacingOk &= LastToggle == INDEX_NONE || Frame - LastToggle >= Params.MinToggleFrames;
			LastToggle = Frame;
			++Toggles;
		}
		bWasGranted = bGranted;
	}
	TestTrue(TEXT("no toggle closer than 1 s to the previous one"), bSpacingOk);
	TestTrue(FString::Printf(TEXT("at most 10 toggles in 10 s (%d)"), Toggles), Toggles <= 10 && Toggles >= 2);

	// A held grant is paid first: a pack kept by the hysteresis takes budget a nearer newcomer would have used
	FGSTN_MinionRelevancyParams Tight;
	Tight.MaxItems = 16;
	FGSTN_MinionRelevancyState Held;
	TArray<FGSTN_MinionPackCandidate> First = { Candidate(1, 16, 5000.f) };
	FGSTN_MinionRelevancyMath::Evaluate(Tight, First, 0, Held);
	TArray<FGSTN_MinionPackCandidate> Second = { Candidate(1, 16, 5000.f), Candidate(2, 16, 100.f) };
	FGSTN_MinionRelevancyMath::Evaluate(Tight, Second, 30, Held);
	TestTrue(TEXT("the held pack stays granted inside its second"), Held.IsGranted(1));
	TestFalse(TEXT("the newcomer waits: the held pack used the budget"), Held.IsGranted(2));
	TArray<FGSTN_MinionPackCandidate> Third = { Candidate(1, 16, 5000.f), Candidate(2, 16, 100.f) };
	FGSTN_MinionRelevancyMath::Evaluate(Tight, Third, 100, Held);
	TestTrue(TEXT("once both are free the nearer pack wins"), Held.IsGranted(2) && !Held.IsGranted(1));

	// A pack that disappears is forgotten
	TArray<FGSTN_MinionPackCandidate> None;
	FGSTN_MinionRelevancyMath::Evaluate(Tight, None, 200, Held);
	TestEqual(TEXT("no grants left"), Held.Grants.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionNetRegistryHashTest, "GSTNMinion.Core.Net.RegistryHash", GSTNMinionTest::Flags)
bool FGSTN_MinionNetRegistryHashTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Server = { TEXT("/GSTNMinion/DA_Minion_Grunt.DA_Minion_Grunt"), TEXT("/GSTNMinion/DA_Minion_Heavy.DA_Minion_Heavy") };
	const TArray<FString> SameCase = { TEXT("/gstnminion/da_minion_grunt.da_minion_grunt"), TEXT("/GSTNMinion/DA_Minion_Heavy.DA_Minion_Heavy") };
	const TArray<FString> Swapped = { Server[1], Server[0] };
	const TArray<FString> Missing = { Server[0], FString() };
	const TArray<FString> Prefix = { Server[0] };
	const uint32 Hash = FGSTN_MinionRegistryHash::Compute(Server);
	TestTrue(TEXT("equal registries hash equal"), FGSTN_MinionRegistryHash::Compute(TArray<FString>(Server)) == Hash);
	TestTrue(TEXT("path case does not matter"), FGSTN_MinionRegistryHash::Compute(SameCase) == Hash);
	TestNotEqual(TEXT("a different order is a mismatch"), FGSTN_MinionRegistryHash::Compute(Swapped), Hash);
	TestNotEqual(TEXT("an unresolved type on the client is a mismatch"), FGSTN_MinionRegistryHash::Compute(Missing), Hash);
	TestNotEqual(TEXT("a shorter registry is a mismatch"), FGSTN_MinionRegistryHash::Compute(Prefix), Hash);
	TestNotEqual(TEXT("an empty registry differs from a filled one"), FGSTN_MinionRegistryHash::Compute(TArray<FString>()), Hash);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
