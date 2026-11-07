// Automation tests of the minion Core: net item codec, quantization bounds, frame stamp wrap.

#include "GSTN_MinionTestUtils.h"
#include "Net/GSTN_MinionNetCodec.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_MinionNetState RandomState(FRandomStream& Random, const FVector3f& Origin)
	{
		FGSTN_MinionNetState State;
		State.Position = Origin + FVector3f(Random.FRandRange(-2500.f, 2500.f), Random.FRandRange(-2500.f, 2500.f), Random.FRandRange(-500.f, 500.f));
		State.YawDegrees = Random.FRandRange(-720.f, 720.f);
		State.ActionState = static_cast<uint8>(Random.RandRange(0, 11));
		State.MoveId = static_cast<uint8>(Random.RandRange(0, 31));
		State.FrameStamp = Random.RandRange(100000, 200000);
		State.MoveStartFrame = State.MoveId == 0 ? INDEX_NONE : State.FrameStamp - Random.RandRange(0, 1000);
		State.HitCounter = static_cast<uint8>(Random.RandRange(0, 7));
		State.HealthFraction = Random.FRand();
		State.Speed = Random.FRandRange(0.f, 450.f);
		State.Flags = static_cast<uint8>(Random.RandRange(0, 7));
		return State;
	}

	/** Writes the item to a packed stream and reads it back; returns the bit count */
	int64 RoundTrip(const FGSTN_MinionNetItem& In, const FGSTN_MinionNetFormat& Format, FGSTN_MinionNetItem& Out, bool& bOutOk)
	{
		FBitWriter Writer(512, true);
		FGSTN_MinionNetItem Copy = In;
		bOutOk = FGSTN_MinionNetCodec::Serialize(Writer, Copy, Format);

		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		bOutOk &= FGSTN_MinionNetCodec::Serialize(Reader, Out, Format);
		bOutOk &= Reader.AtEnd();
		return Writer.GetNumBits();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionCodecBoundsTest, "GSTNMinion.Core.Net.RoundTripQuantizationBounds", GSTNMinionTest::Flags)
bool FGSTN_MinionCodecBoundsTest::RunTest(const FString& Parameters)
{
	FGSTN_MinionNetQuantization Quant;
	Quant.WorldOrigin = FVector3f(1200.f, -800.f, 150.f);
	Quant.MaxSpeed = 450.f;
	const FGSTN_MinionNetFormat Format;

	FRandomStream Random(77);
	float WorstPosition = 0.f;
	float WorstYaw = 0.f;
	for (int32 Index = 0; Index < 3000; ++Index)
	{
		const FGSTN_MinionNetState State = RandomState(Random, Quant.WorldOrigin);
		FGSTN_MinionNetItem Item;
		Item.Id = static_cast<uint8>(Index & 0xFF);
		FGSTN_MinionNetCodec::Quantize(State, Quant, Item);

		FGSTN_MinionNetItem Read;
		bool bOk = false;
		RoundTrip(Item, Format, Read, bOk);
		if (!bOk || Read != Item)
		{
			AddError(TEXT("The serialized item differs from the quantized one"));
			return true;
		}

		const FGSTN_MinionNetState Back = FGSTN_MinionNetCodec::Dequantize(Read, Quant, State.FrameStamp + (Index % 5) - 2);
		WorstPosition = FMath::Max(WorstPosition, (Back.Position - State.Position).GetAbsMax());
		WorstYaw = FMath::Max(WorstYaw, FMath::Abs(FRotator::NormalizeAxis(Back.YawDegrees - State.YawDegrees)));

		if (Back.ActionState != State.ActionState || Back.MoveId != State.MoveId || Back.HitCounter != State.HitCounter || Back.Flags != State.Flags)
		{
			AddError(TEXT("A small integer field changed on the wire"));
			return true;
		}
		if (Back.FrameStamp != State.FrameStamp || Back.MoveStartFrame != State.MoveStartFrame)
		{
			AddError(FString::Printf(TEXT("Frames not rebuilt: stamp %d -> %d, move start %d -> %d"), State.FrameStamp, Back.FrameStamp, State.MoveStartFrame, Back.MoveStartFrame));
			return true;
		}
		if (State.HealthFraction > 0.f && Back.HealthFraction <= 0.f)
		{
			AddError(TEXT("A living minion read as dead"));
			return true;
		}
		if (FMath::Abs(Back.Speed - State.Speed) > Quant.MaxSpeed / 63.f * 0.5f + 0.01f)
		{
			AddError(TEXT("Speed error above half a step"));
			return true;
		}
	}
	TestTrue(FString::Printf(TEXT("Position error %.4f cm is at most 0.5 cm"), WorstPosition), WorstPosition <= 0.5f + 1.0e-3f);
	TestTrue(FString::Printf(TEXT("Yaw error %.4f deg is at most 0.8 deg"), WorstYaw), WorstYaw <= 0.8f);

	// Quantized values are fixed points of the codec: dequantize then quantize again gives the same item
	FGSTN_MinionNetState State = RandomState(Random, Quant.WorldOrigin);
	FGSTN_MinionNetItem First;
	FGSTN_MinionNetCodec::Quantize(State, Quant, First);
	const FGSTN_MinionNetState Dequantized = FGSTN_MinionNetCodec::Dequantize(First, Quant, State.FrameStamp);
	FGSTN_MinionNetItem Second;
	FGSTN_MinionNetCodec::Quantize(Dequantized, Quant, Second);
	TestTrue(TEXT("Quantize(Dequantize(x)) == x"), First == Second);

	// Positions outside the 19-bit range clamp instead of wrapping
	FGSTN_MinionNetState Far;
	Far.Position = Quant.WorldOrigin + FVector3f(1.0e7f, -1.0e7f, 0.f);
	FGSTN_MinionNetItem FarItem;
	FGSTN_MinionNetCodec::Quantize(Far, Quant, FarItem);
	const FGSTN_MinionNetState FarBack = FGSTN_MinionNetCodec::Dequantize(FarItem, Quant, 0);
	TestTrue(TEXT("Clamped to the edge of the range"), FarBack.Position.X > Quant.WorldOrigin.X + 2000.f && FarBack.Position.Y < Quant.WorldOrigin.Y - 2000.f);

	// Health: 0 only for 0, full is 31
	TestEqual(TEXT("Zero health"), static_cast<int32>(GSTNMinionQuant::QuantizeHealth(0.f)), 0);
	TestEqual(TEXT("A sliver of health is not zero"), static_cast<int32>(GSTNMinionQuant::QuantizeHealth(0.001f)), 1);
	TestEqual(TEXT("Full health"), static_cast<int32>(GSTNMinionQuant::QuantizeHealth(1.f)), 31);
	TestEqual(TEXT("Full health decodes to 1"), GSTNMinionQuant::DequantizeHealth(31), 1.f);

	// Yaw wraps both ways
	TestEqual(TEXT("360 degrees is step 0"), static_cast<int32>(GSTNMinionQuant::QuantizeYaw(360.f)), 0);
	TestEqual(TEXT("-1.4 degrees is the last step"), static_cast<int32>(GSTNMinionQuant::QuantizeYaw(-1.4f)), 255);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionCodecLayoutTest, "GSTNMinion.Core.Net.LayoutAndOptionalFields", GSTNMinionTest::Flags)
bool FGSTN_MinionCodecLayoutTest::RunTest(const FString& Parameters)
{
	FGSTN_MinionNetQuantization Quant;
	FRandomStream Random(5);
	FGSTN_MinionNetState State = RandomState(Random, FVector3f::ZeroVector);

	for (int32 Mask = 0; Mask < 16; ++Mask)
	{
		FGSTN_MinionNetFormat Format;
		Format.bIdFieldActive = (Mask & 1) != 0;
		Format.bTypeHasExt = (Mask & 2) != 0;

		FGSTN_MinionNetItem Item;
		FGSTN_MinionNetCodec::Quantize(State, Quant, Item);
		Item.Id = 0xA5;
		Item.bHasType = (Mask & 4) != 0;
		Item.TypeIndex = Item.bHasType ? 0x5A : 0;
		Item.bHasGlobalId = Format.bIdFieldActive && (Mask & 8) != 0;
		Item.GlobalId = Item.bHasGlobalId ? 0xC3 : 0;
		Item.ExtByte = Format.bTypeHasExt ? 0x81 : 0;

		FGSTN_MinionNetItem Read;
		bool bOk = false;
		const int64 Bits = RoundTrip(Item, Format, Read, bOk);
		TestTrue(FString::Printf(TEXT("Format %d round trips without error and consumes every bit"), Mask), bOk);
		TestTrue(FString::Printf(TEXT("Format %d reads back the same item"), Mask), Read == Item);
		TestEqual(FString::Printf(TEXT("Format %d bit size matches the stream"), Mask), static_cast<int64>(FGSTN_MinionNetCodec::GetBitSize(Item, Format)), Bits);
	}

	// The smallest item (no type, no id field, no extension) is within the spec's ~121 bits
	const FGSTN_MinionNetFormat Minimal;
	FGSTN_MinionNetItem Smallest;
	TestTrue(TEXT("The minimal item is at most 121 bits"), FGSTN_MinionNetCodec::GetBitSize(Smallest, Minimal) <= 121);
	TestTrue(TEXT("and above 110"), FGSTN_MinionNetCodec::GetBitSize(Smallest, Minimal) > 110);

	// An item with no type does not carry stale type bits across reads
	FGSTN_MinionNetItem Dirty;
	Dirty.TypeIndex = 77;
	Dirty.bHasType = false;
	bool bOk = false;
	FGSTN_MinionNetItem Read;
	Read.TypeIndex = 99;
	RoundTrip(Dirty, Minimal, Read, bOk);
	TestEqual(TEXT("Absent type reads as 0"), static_cast<int32>(Read.TypeIndex), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionFrameWrapTest, "GSTNMinion.Core.Net.FrameStampWrap", GSTNMinionTest::Flags)
bool FGSTN_MinionFrameWrapTest::RunTest(const FString& Parameters)
{
	for (const int32 Bits : { 8, 10 })
	{
		const int32 Period = 1 << Bits;
		const int32 Half = Period / 2;
		const int32 References[] = {
			0, 1, Half - 1, Half, Period - 1, Period, Period + 1, 3 * Period - 1, 3 * Period, 3 * Period + Half,
			100000, 100000 + Period - 1, (1 << 20) - 1, (1 << 20), (1 << 20) + 1, MAX_int32 - Period,
		};

		for (const int32 Reference : References)
		{
			// Nearest: every frame within [Ref - Half, Ref + Half - 1] is rebuilt exactly
			for (int32 Offset = -Half; Offset < Half; ++Offset)
			{
				const int32 Frame = Reference + Offset;
				if (Frame < 0)
				{
					continue;
				}
				const int32 Got = GSTNMinionFrame::ReconstructNearest(Reference, GSTNMinionFrame::Truncate(Frame, Bits), Bits);
				if (Got != Frame)
				{
					AddError(FString::Printf(TEXT("%d bits: nearest(ref %d, frame %d) gave %d"), Bits, Reference, Frame, Got));
					return true;
				}
			}
			// Past: every frame in (Ref - Period, Ref] is rebuilt exactly
			for (int32 Back = 0; Back < Period; ++Back)
			{
				const int32 Frame = Reference - Back;
				if (Frame < 0)
				{
					continue;
				}
				const int32 Got = GSTNMinionFrame::ReconstructPast(Reference, GSTNMinionFrame::Truncate(Frame, Bits), Bits);
				if (Got != Frame)
				{
					AddError(FString::Printf(TEXT("%d bits: past(ref %d, frame %d) gave %d"), Bits, Reference, Frame, Got));
					return true;
				}
			}
		}
	}

	// The wrap seen from the wire: a stamp of 255 then 0 are consecutive frames for a receiver at 256
	TestEqual(TEXT("8-bit: low 255 near ref 256 is 255"), GSTNMinionFrame::ReconstructNearest(256, 255, 8), 255);
	TestEqual(TEXT("8-bit: low 0 near ref 255 is 256"), GSTNMinionFrame::ReconstructNearest(255, 0, 8), 256);
	TestEqual(TEXT("10-bit: low 1023 near ref 1024 is 1023"), GSTNMinionFrame::ReconstructNearest(1024, 1023, 10), 1023);
	TestEqual(TEXT("10-bit past: low 1020 at ref 1026 is 1020"), GSTNMinionFrame::ReconstructPast(1026, 1020, 10), 1020);
	TestEqual(TEXT("10-bit past: low 3 at ref 1026 is 3 (1027 would be in the future)"), GSTNMinionFrame::ReconstructPast(1026, 3, 10), 3);
	TestEqual(TEXT("Truncate takes the low bits"), static_cast<int32>(GSTNMinionFrame::Truncate(1030, 8)), 6);

	// The codec ties them together across a wrap: sample frame 1030, move started at 1020 (both sides of a 10-bit wrap at 1024)
	FGSTN_MinionNetQuantization Quant;
	FGSTN_MinionNetState State;
	State.FrameStamp = 1030;
	State.MoveId = 3;
	State.MoveStartFrame = 1020;
	FGSTN_MinionNetItem Item;
	FGSTN_MinionNetCodec::Quantize(State, Quant, Item);
	const FGSTN_MinionNetState Back = FGSTN_MinionNetCodec::Dequantize(Item, Quant, 1000);
	TestEqual(TEXT("Stamp rebuilt from a reference 30 frames behind"), Back.FrameStamp, 1030);
	TestEqual(TEXT("Move start rebuilt across the 10-bit wrap"), Back.MoveStartFrame, 1020);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
