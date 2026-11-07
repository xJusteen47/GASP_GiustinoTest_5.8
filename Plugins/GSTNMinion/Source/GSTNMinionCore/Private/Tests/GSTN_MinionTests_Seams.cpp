// Automation tests of the minion Core side of the bridge seams (M8 G10): the store columns and the ExtByte format built from a definition.

#include "Data/GSTN_MinionDefinition.h"
#include "GSTN_MinionTestUtils.h"
#include "Net/GSTN_MinionNetCodec.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Store/GSTN_MinionStore.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Writes the item to a packed stream and reads it back; returns the bit count */
	int64 SeamRoundTrip(const FGSTN_MinionNetWireItem& In, const FGSTN_MinionNetFormat& Format, FGSTN_MinionNetWireItem& Out, bool& bOutOk)
	{
		FBitWriter Writer(512, true);
		FGSTN_MinionNetWireItem Copy = In;
		bOutOk = FGSTN_MinionNetCodec::Serialize(Writer, Copy, Format);

		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		bOutOk &= FGSTN_MinionNetCodec::Serialize(Reader, Out, Format);
		bOutOk &= Reader.AtEnd();
		return Writer.GetNumBits();
	}

	FGSTN_MinionNetWireItem MakeItem(uint8 TypeIndex, uint8 ExtByte)
	{
		FGSTN_MinionNetState State;
		State.Position = FVector3f(120.f, -340.f, 15.f);
		State.YawDegrees = 90.f;
		State.FrameStamp = 1000;
		State.HealthFraction = 1.f;

		FGSTN_MinionNetWireItem Item;
		FGSTN_MinionNetCodec::Quantize(State, FGSTN_MinionNetQuantization(), Item);
		Item.Id = 3;
		Item.bHasType = true;
		Item.TypeIndex = TypeIndex;
		Item.ExtByte = ExtByte;
		return Item;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionSeamsStoreColumnsTest, "GSTNMinion.Core.Seams.StoreColumns", GSTNMinionTest::Flags)
bool FGSTN_MinionSeamsStoreColumnsTest::RunTest(const FString& Parameters)
{
	FGSTN_MinionStore Store;
	Store.Reset(3);
	const FGSTN_MinionHandle A = Store.Allocate();
	TestEqual(TEXT("no bridge flag by default"), static_cast<int32>(Store.ExtFlags[A.Index]), 0);
	TestEqual(TEXT("no LOD ceiling by default (the farthest tier)"), static_cast<int32>(Store.MaxLODTier[A.Index]), static_cast<int32>(GSTNMinion::NoLODCeiling));
	TestEqual(TEXT("the default ceiling is Dormant"), static_cast<int32>(GSTNMinion::NoLODCeiling), static_cast<int32>(EGSTN_MinionLODTier::Dormant));
	TestEqual(TEXT("no ext byte by default"), static_cast<int32>(Store.ExtByte[A.Index]), 0);

	Store.ExtFlags[A.Index] = GSTNMinion::ExtFlag::PlatformActor | GSTNMinion::ExtFlag::ExemptFromCombatCap;
	Store.MaxLODTier[A.Index] = static_cast<uint8>(EGSTN_MinionLODTier::Near);
	Store.ExtByte[A.Index] = 0xA5;
	TestTrue(TEXT("free"), Store.Free(A));
	const FGSTN_MinionHandle B = Store.Allocate();
	TestEqual(TEXT("the slot is recycled"), static_cast<int32>(B.Index), static_cast<int32>(A.Index));
	TestEqual(TEXT("flags reset with the slot"), static_cast<int32>(Store.ExtFlags[B.Index]), 0);
	TestEqual(TEXT("ceiling reset with the slot"), static_cast<int32>(Store.MaxLODTier[B.Index]), static_cast<int32>(GSTNMinion::NoLODCeiling));
	TestEqual(TEXT("ext byte reset with the slot"), static_cast<int32>(Store.ExtByte[B.Index]), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionSeamsExtByteTest, "GSTNMinion.Core.Seams.ExtByteFormatFromDefinition", GSTNMinionTest::Flags)
bool FGSTN_MinionSeamsExtByteTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_MinionDefinition> WithExt(NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
	WithExt->bHasNetExt = true;
	TStrongObjectPtr<UGSTN_MinionDefinition> WithoutExt(NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
	TestFalse(TEXT("a type has no ext byte by default"), WithoutExt->bHasNetExt);

	const FGSTN_MinionNetFormat Plain = WithoutExt->MakeNetFormat();
	const FGSTN_MinionNetFormat Extended = WithExt->MakeNetFormat();
	TestFalse(TEXT("the plain format has no ext"), Plain.bTypeHasExt);
	TestTrue(TEXT("the extended format has ext"), Extended.bTypeHasExt);

	// The type without the byte costs exactly what the v1 layout cost, whatever the item holds
	const FGSTN_MinionNetFormat V1;
	const FGSTN_MinionNetWireItem Dirty = MakeItem(0, 0xFF);
	TestEqual(TEXT("0 extra bits without bHasNetExt"), FGSTN_MinionNetCodec::GetBitSize(Dirty, Plain), FGSTN_MinionNetCodec::GetBitSize(Dirty, V1));
	FGSTN_MinionNetWireItem Read;
	bool bOk = false;
	const int64 PlainBits = SeamRoundTrip(Dirty, Plain, Read, bOk);
	TestTrue(TEXT("plain round trips"), bOk);
	TestEqual(TEXT("plain stream bits equal the v1 size"), PlainBits, static_cast<int64>(FGSTN_MinionNetCodec::GetBitSize(Dirty, V1)));
	TestEqual(TEXT("the byte is not carried without bHasNetExt"), static_cast<int32>(Read.ExtByte), 0);

	// The type with the byte round-trips it, for 8 more bits
	const FGSTN_MinionNetWireItem WithByte = MakeItem(0, 0xA7);
	FGSTN_MinionNetWireItem ReadExt;
	const int64 ExtBits = SeamRoundTrip(WithByte, Extended, ReadExt, bOk);
	TestTrue(TEXT("extended round trips and consumes every bit"), bOk);
	TestEqual(TEXT("the byte survives"), static_cast<int32>(ReadExt.ExtByte), 0xA7);
	TestTrue(TEXT("the item is identical"), ReadExt == WithByte);
	TestEqual(TEXT("8 more bits"), ExtBits, PlainBits + GSTNMinion::NetBits::ExtByte);
	TestEqual(TEXT("bit size matches the stream"), static_cast<int64>(FGSTN_MinionNetCodec::GetBitSize(WithByte, Extended)), ExtBits);

	// A mixed pack: the table says which type indices own the byte, the type is read before it
	TBitArray<> ExtTypes(false, 3);
	ExtTypes[1] = true;
	FGSTN_MinionNetFormat Mixed;
	Mixed.ExtTypes = &ExtTypes;
	const FGSTN_MinionNetWireItem OfType0 = MakeItem(0, 0x33);
	const FGSTN_MinionNetWireItem OfType1 = MakeItem(1, 0x33);
	const FGSTN_MinionNetWireItem OfType2 = MakeItem(2, 0x33);
	FGSTN_MinionNetWireItem Read0;
	FGSTN_MinionNetWireItem Read1;
	FGSTN_MinionNetWireItem Read2;
	const int64 Bits0 = SeamRoundTrip(OfType0, Mixed, Read0, bOk);
	TestTrue(TEXT("type 0 round trips"), bOk);
	const int64 Bits1 = SeamRoundTrip(OfType1, Mixed, Read1, bOk);
	TestTrue(TEXT("type 1 round trips"), bOk);
	const int64 Bits2 = SeamRoundTrip(OfType2, Mixed, Read2, bOk);
	TestTrue(TEXT("type 2 round trips"), bOk);
	TestEqual(TEXT("type 0 (no ext) reads 0"), static_cast<int32>(Read0.ExtByte), 0);
	TestEqual(TEXT("type 1 (ext) keeps the byte"), static_cast<int32>(Read1.ExtByte), 0x33);
	TestEqual(TEXT("type 2 (past the table) reads 0"), static_cast<int32>(Read2.ExtByte), 0);
	TestEqual(TEXT("only the ext type pays the 8 bits"), Bits1, Bits0 + GSTNMinion::NetBits::ExtByte);
	TestEqual(TEXT("a type past the table pays nothing"), Bits2, Bits0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
