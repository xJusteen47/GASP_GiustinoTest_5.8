// Automation tests of the parts sync fragment (plan P6): its wire format against the byte numbers of the P0c spike and the spec table,
// the framing in the combat sync state with zero, one and two fragments, exact reconcile, and the unknown-id and malformed cases.
// They need no world: the state is plain data and the archives are bit archives, so the printed sizes are what goes on the wire.

#include "GSTN_PartsFragmentRegistration.h"
#include "GSTN_PartsNPTestTypes.h"
#include "Misc/AutomationTest.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "State/GSTN_PartsFragment.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogGSTNPartsNPTest, Log, All);

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Registers the test fragment for the life of a test, as its plugin's module would; the parts fragment is the module's own */
	struct FScopedFragments
	{
		FScopedFragments()
		{
			FGSTN_PartsFragmentRegistration::Register();
			FGSTN_SyncFragmentRegistry::Register<FGSTN_PartsTestFragment>(GSTNFeatureIds::TestA);
		}

		~FScopedFragments()
		{
			FGSTN_SyncFragmentRegistry::Unregister(GSTNFeatureIds::TestA);
			FGSTN_PartsFragmentRegistration::Register();
		}
	};

	/*
	 * The tests go through FGSTN_SyncFragmentList, which is exactly what the combat sync state's NetSerialize, ShouldReconcile, Interpolate
	 * and ToString delegate to for the fragments. FGSTN_CombatSyncState itself cannot be instantiated outside GSTNCombatNP (its hit
	 * sync member has no export macro, so the inline constructor does not link from another module); the state's own wire cost and the
	 * 1-bit empty list are pinned by GSTNCombat.NP.Fragments.* in the combat plugin.
	 */
	int64 BitsOfList(FGSTN_SyncFragmentArray& Fragments)
	{
		FBitWriter Writer(256, true);
		FGSTN_SyncFragmentList::NetSerialize(Writer, Fragments);
		return Writer.GetNumBits();
	}

	/** Writes the list and reads it back; bOutError is the reader's error or bytes left over */
	FGSTN_SyncFragmentArray ReadBack(FGSTN_SyncFragmentArray& Fragments, bool& bOutError)
	{
		FBitWriter Writer(256, true);
		FGSTN_SyncFragmentList::NetSerialize(Writer, Fragments);
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_SyncFragmentArray Loaded;
		FGSTN_SyncFragmentList::NetSerialize(Reader, Loaded);
		bOutError = Reader.IsError() || !Reader.AtEnd();
		return Loaded;
	}

	/** A fragment of n parts with a recognisable pattern: part i holds 10 + 15 i, parts 0 and n - 1 are broken (255) */
	FGSTN_PartsFragment MakeFragment(int32 PartCount, uint8 Serial = 3)
	{
		FGSTN_PartsState State;
		for (int32 Index = 0; Index < PartCount; ++Index)
		{
			State.PartDamage[Index] = static_cast<uint8>(10 + 15 * Index);
		}
		State.PartDamage[0] = 255;
		State.PartDamage[PartCount - 1] = 255;
		State.BrokenMask = static_cast<uint16>(1u | (1u << (PartCount - 1)));
		State.BreakSerial = Serial;
		FGSTN_PartsFragment Fragment;
		Fragment.SetState(State, PartCount);
		return Fragment;
	}

	/** Every part broken at the cap and the serial at its largest value */
	FGSTN_PartsFragment MakeFull(int32 PartCount)
	{
		FGSTN_PartsState State;
		for (int32 Index = 0; Index < PartCount; ++Index)
		{
			State.PartDamage[Index] = 255;
			State.BrokenMask |= static_cast<uint16>(1u << Index);
		}
		State.BreakSerial = FGSTN_PartsState::SerialMask;
		FGSTN_PartsFragment Fragment;
		Fragment.SetState(State, PartCount);
		return Fragment;
	}

	int32 PayloadBytesOf(const FGSTN_PartsFragment& Fragment)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		const_cast<FGSTN_PartsFragment&>(Fragment).NetSerialize(Writer);
		return Bytes.Num();
	}

	FGSTN_SyncFragmentArray ListOf(const FGSTN_PartsFragment& Fragment)
	{
		FGSTN_SyncFragmentArray List;
		FGSTN_SyncFragmentList::FindOrAdd<FGSTN_PartsFragment>(List) = Fragment;
		return List;
	}

	bool SameState(const FGSTN_PartsFragment& A, const FGSTN_PartsFragment& B)
	{
		return A.ToState() == B.ToState();
	}

	FString ToText(const FGSTN_SyncFragmentArray& Fragments)
	{
		TAnsiStringBuilder<512> Builder;
		FGSTN_SyncFragmentList::ToString(Fragments, Builder);
		return FString(ANSI_TO_TCHAR(Builder.ToString()));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsFragmentRoundTrip0Test, "GSTNParts.NP.Fragment.RoundTrip0", Flags)
bool FGSTN_PartsFragmentRoundTrip0Test::RunTest(const FString& Parameters)
{
	FScopedFragments Registered;
	TestTrue(TEXT("the module registered the fragment under its fixed id"),
		FGSTN_SyncFragmentRegistry::FindStruct(GSTNPartsFeatureIds::Parts) == FGSTN_PartsFragment::StaticStruct());

	// No fragments: exactly the one bit, which the plugin being loaded does not change
	FGSTN_SyncFragmentArray State;
	TestEqual(TEXT("zero fragments cost one bit"), BitsOfList(State), static_cast<int64>(1));
	bool bError = false;
	const FGSTN_SyncFragmentArray Loaded = ReadBack(State, bError);
	TestFalse(TEXT("reads back to the end without error"), bError);
	TestTrue(TEXT("no fragments"), Loaded.IsEmpty());
	TestFalse(TEXT("no reconcile against itself"), FGSTN_SyncFragmentList::ShouldReconcile(Loaded, State));
	TestTrue(TEXT("the text of a state without fragments is empty"), ToText(State).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsFragmentRoundTrip1Test, "GSTNParts.NP.Fragment.RoundTrip1", Flags)
bool FGSTN_PartsFragmentRoundTrip1Test::RunTest(const FString& Parameters)
{
	FScopedFragments Registered;
	FGSTN_SyncFragmentArray State;
	const FGSTN_PartsFragment Sent = MakeFragment(8);
	FGSTN_SyncFragmentList::FindOrAdd<FGSTN_PartsFragment>(State) = Sent;

	// 1 flag bit, packed count (8), type id (8), packed length (8), 3 + 8 payload bytes
	const int64 Bits = BitsOfList(State);
	UE_LOG(LogGSTNPartsNPTest, Display, TEXT("Fragment.RoundTrip1: %lld bits (%lld bytes) for the fragment list, N = 8 payload %d bytes"), Bits, (Bits + 7) / 8, PayloadBytesOf(Sent));
	TestEqual(TEXT("one fragment: flag, count, id, length and payload"), Bits, static_cast<int64>(1 + 8 + 8 + 8 + 8 * 11));

	bool bError = false;
	FGSTN_SyncFragmentArray Loaded = ReadBack(State, bError);
	TestFalse(TEXT("reads back to the end without error"), bError);
	const FGSTN_PartsFragment* Read = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Loaded);
	TestTrue(TEXT("the fragment is there"), Read != nullptr);
	if (!Read)
	{
		return false;
	}
	TestTrue(TEXT("mask, damage and serial survive"), SameState(*Read, Sent));
	TestEqual(TEXT("N is derived from the payload length"), static_cast<int32>(Read->PartCount), 8);
	TestFalse(TEXT("equal states do not reconcile"), FGSTN_SyncFragmentList::ShouldReconcile(Loaded, State));
	FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Loaded)->PartDamage[2] = 99;
	TestTrue(TEXT("a damage byte that differs does"), FGSTN_SyncFragmentList::ShouldReconcile(Loaded, State));
	TestTrue(TEXT("a fragment missing on one side does"), FGSTN_SyncFragmentList::ShouldReconcile(FGSTN_SyncFragmentArray(), State));

	TestTrue(TEXT("the fragment is described in the state text"), ToText(State).Contains(TEXT("Fragments: [16: Parts mask 0x0081 serial 3 n=8 [")));

	// The registry hands Interpolate a copy of the later sample: the result is that sample, never a blend
	FGSTN_SyncFragmentArray Older = State;
	FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Older)->PartDamage[1] = 1;
	FGSTN_SyncFragmentArray Interpolated;
	FGSTN_SyncFragmentList::Interpolate(Interpolated, &Older, State, 0.5f);
	const FGSTN_PartsFragment* Blend = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Interpolated);
	TestTrue(TEXT("interpolation keeps the later sample"), Blend && SameState(*Blend, Sent));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsFragmentRoundTrip2Test, "GSTNParts.NP.Fragment.RoundTrip2", Flags)
bool FGSTN_PartsFragmentRoundTrip2Test::RunTest(const FString& Parameters)
{
	FScopedFragments Registered;
	FGSTN_SyncFragmentArray State;

	// The test fragment is added first, but the list keeps type id order: parts (16) before it (240)
	FGSTN_PartsTestFragment& Other = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_PartsTestFragment>(State);
	Other.Frame = 9;
	Other.Count = 1;
	const FGSTN_PartsFragment Sent = MakeFragment(6, 7);
	FGSTN_SyncFragmentList::FindOrAdd<FGSTN_PartsFragment>(State) = Sent;
	TestEqual(TEXT("two fragments"), State.Num(), 2);
	TestEqual(TEXT("sorted by type id: parts first"), FGSTN_SyncFragmentRegistry::FindId(State[0].GetScriptStruct()), GSTNPartsFeatureIds::Parts);

	// 1 flag bit, packed count (8), then per fragment id (8), length (8), payload: 9 bytes for N = 6, 5 for the test fragment
	const int64 Bits = BitsOfList(State);
	UE_LOG(LogGSTNPartsNPTest, Display, TEXT("Fragment.RoundTrip2: %lld bits (%lld bytes) for the fragment list with parts N = 6 and the test fragment"), Bits, (Bits + 7) / 8);
	TestEqual(TEXT("two fragments: shared flag and count, then each fragment's id, length and payload"),
		Bits, static_cast<int64>(1 + 8 + (8 + 8 + 8 * 9) + (8 + 8 + 8 * 5)));

	bool bError = false;
	FGSTN_SyncFragmentArray Loaded = ReadBack(State, bError);
	TestFalse(TEXT("reads back to the end without error"), bError);
	TestEqual(TEXT("two fragments read back"), Loaded.Num(), 2);
	const FGSTN_PartsFragment* ReadParts = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Loaded);
	const FGSTN_PartsTestFragment* ReadOther = FGSTN_SyncFragmentList::Find<FGSTN_PartsTestFragment>(Loaded);
	TestTrue(TEXT("both are there"), ReadParts && ReadOther);
	if (ReadParts && ReadOther)
	{
		TestTrue(TEXT("the parts fragment survives"), SameState(*ReadParts, Sent));
		TestEqual(TEXT("the other fragment survives"), ReadOther->Frame, 9);
	}
	TestFalse(TEXT("equal"), FGSTN_SyncFragmentList::ShouldReconcile(Loaded, State));
	FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Loaded)->BreakSerial = 8;
	TestTrue(TEXT("the parts serial differs"), FGSTN_SyncFragmentList::ShouldReconcile(Loaded, State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsFragmentSizesTest, "GSTNParts.NP.Fragment.N1_8_16EmptyAndFull", Flags)
bool FGSTN_PartsFragmentSizesTest::RunTest(const FString& Parameters)
{
	FScopedFragments Registered;
	for (const int32 Count : { 1, 8, 16 })
	{
		// Empty: the payload is zero bytes, the framing is flag + count + id + length, and the fragment still arrives (empty)
		FGSTN_PartsFragment Empty;
		Empty.PartCount = static_cast<uint8>(Count);
		TestTrue(*FString::Printf(TEXT("N = %d pristine is empty"), Count), Empty.IsEmpty());
		TestEqual(*FString::Printf(TEXT("N = %d empty payload"), Count), PayloadBytesOf(Empty), 0);
		FGSTN_SyncFragmentArray EmptyList = ListOf(Empty);
		TestEqual(*FString::Printf(TEXT("N = %d empty framing"), Count), BitsOfList(EmptyList), static_cast<int64>(1 + 8 + 8 + 8));
		{
			FBitWriter Writer(256, true);
			FGSTN_SyncFragmentList::NetSerialize(Writer, EmptyList);
			FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
			FGSTN_SyncFragmentArray Read;
			FGSTN_SyncFragmentList::NetSerialize(Reader, Read);
			TestFalse(*FString::Printf(TEXT("N = %d empty reads cleanly"), Count), Reader.IsError() || !Reader.AtEnd());
			const FGSTN_PartsFragment* ReadEmpty = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Read);
			TestTrue(*FString::Printf(TEXT("N = %d empty fragment is present and empty"), Count), ReadEmpty && ReadEmpty->IsEmpty());
			TestFalse(*FString::Printf(TEXT("N = %d empty equals the original"), Count), FGSTN_SyncFragmentList::ShouldReconcile(Read, EmptyList));
		}

		// Pristine damage with only the serial moved is not empty: all N bytes travel, zeros included
		FGSTN_PartsFragment SerialOnly;
		SerialOnly.PartCount = static_cast<uint8>(Count);
		SerialOnly.BreakSerial = 1;
		TestEqual(*FString::Printf(TEXT("N = %d serial only payload"), Count), PayloadBytesOf(SerialOnly), FGSTN_PartsFragment::GetPayloadBytes(Count));

		// Full: every part broken at the cap, the serial at its largest
		const FGSTN_PartsFragment Full = MakeFull(Count);
		FGSTN_SyncFragmentArray FullList = ListOf(Full);
		TestEqual(*FString::Printf(TEXT("N = %d full payload"), Count), PayloadBytesOf(Full), 3 + Count);
		FBitWriter Writer(256, true);
		FGSTN_SyncFragmentList::NetSerialize(Writer, FullList);
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_SyncFragmentArray Read;
		FGSTN_SyncFragmentList::NetSerialize(Reader, Read);
		TestFalse(*FString::Printf(TEXT("N = %d full reads cleanly"), Count), Reader.IsError() || !Reader.AtEnd());
		const FGSTN_PartsFragment* ReadFull = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Read);
		TestTrue(*FString::Printf(TEXT("N = %d full survives"), Count), ReadFull && SameState(*ReadFull, Full) && ReadFull->PartCount == Count);
	}

	// A set of N parts with damage only on part 0: the highest part with damage decides nothing, PartCount does
	FGSTN_PartsFragment Sparse;
	Sparse.PartCount = 8;
	Sparse.PartDamage[0] = 5;
	TestEqual(TEXT("PartCount fixes the payload length"), PayloadBytesOf(Sparse), 11);
	// A fragment whose count was never set is never cut short: the bytes of the parts that hold damage are written
	FGSTN_PartsFragment NoCount;
	NoCount.PartDamage[4] = 7;
	TestEqual(TEXT("a damaged part beyond PartCount is still written"), PayloadBytesOf(NoCount), 3 + 5);
	NoCount.BrokenMask = 1u << 9;
	TestEqual(TEXT("so is a broken bit"), PayloadBytesOf(NoCount), 3 + 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsFragmentReconcileTest, "GSTNParts.NP.Fragment.ShouldReconcileExact", Flags)
bool FGSTN_PartsFragmentReconcileTest::RunTest(const FString& Parameters)
{
	FScopedFragments Registered;
	const FGSTN_PartsFragment Base = MakeFragment(16, 5);
	TestFalse(TEXT("a fragment does not reconcile against itself"), Base.ShouldReconcile(Base));

	int32 Checked = 0;
	// Every mask bit, every damage byte, the serial: one unit of difference in any of them is a reconcile, both ways round
	for (int32 Bit = 0; Bit < 16; ++Bit)
	{
		FGSTN_PartsFragment Other = Base;
		Other.BrokenMask ^= static_cast<uint16>(1u << Bit);
		TestTrue(*FString::Printf(TEXT("mask bit %d"), Bit), Base.ShouldReconcile(Other) && Other.ShouldReconcile(Base));
		++Checked;
	}
	for (int32 Index = 0; Index < 16; ++Index)
	{
		FGSTN_PartsFragment Other = Base;
		Other.PartDamage[Index] = static_cast<uint8>(Other.PartDamage[Index] + 1);
		TestTrue(*FString::Printf(TEXT("damage byte %d differs by one"), Index), Base.ShouldReconcile(Other) && Other.ShouldReconcile(Base));
		++Checked;
	}
	FGSTN_PartsFragment OtherSerial = Base;
	OtherSerial.BreakSerial = 6;
	TestTrue(TEXT("serial"), Base.ShouldReconcile(OtherSerial));
	TestEqual(TEXT("33 single-field differences checked"), Checked + 1, 33);

	// The part count is not state
	FGSTN_PartsFragment OtherCount = Base;
	OtherCount.PartCount = 3;
	TestFalse(TEXT("the part count alone does not reconcile"), Base.ShouldReconcile(OtherCount));

	// Through the list framing
	FGSTN_SyncFragmentArray A = ListOf(Base);
	FGSTN_SyncFragmentArray B = ListOf(Base);
	TestFalse(TEXT("equal lists"), FGSTN_SyncFragmentList::ShouldReconcile(A, B));
	FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(B)->PartDamage[15] ^= 1;
	TestTrue(TEXT("unequal lists"), FGSTN_SyncFragmentList::ShouldReconcile(A, B));
	TestTrue(TEXT("a list with the fragment against one without"), FGSTN_SyncFragmentList::ShouldReconcile(A, FGSTN_SyncFragmentArray()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsFragmentBytesTest, "GSTNParts.NP.Fragment.BytesMatchTable", Flags)
bool FGSTN_PartsFragmentBytesTest::RunTest(const FString& Parameters)
{
	FScopedFragments Registered;
	// Spec 5.1 counts bits: 1 flag + N mask bits + 8 N damage bits + 4 serial bits. The built byte encoding is the P0c one (3 + N): at
	// most two bytes above the bit count, and exactly the 19 bytes the spec lists for N = 16
	for (const int32 Count : { 6, 8, 16 })
	{
		const FGSTN_PartsFragment Fragment = MakeFragment(Count);
		const int32 Payload = PayloadBytesOf(Fragment);
		FGSTN_SyncFragmentArray List = ListOf(Fragment);
		const int64 ListBits = BitsOfList(List);
		const int32 SpecBits = 1 + Count + 8 * Count + 4;
		const int32 SpecBytes = (SpecBits + 7) / 8;
		UE_LOG(LogGSTNPartsNPTest, Display, TEXT("Fragment bytes: N = %2d  payload %2d B (spec table %d bits = %d B)  in the list %2lld B (%lld bits, 1 flag + count + id + length + payload)"),
			Count, Payload, SpecBits, SpecBytes, (ListBits + 7) / 8, ListBits);
		TestEqual(*FString::Printf(TEXT("N = %d payload is 3 + N"), Count), Payload, 3 + Count);
		TestTrue(*FString::Printf(TEXT("N = %d within two bytes of the spec bit count"), Count), Payload >= SpecBytes && Payload <= SpecBytes + 2);
		TestEqual(*FString::Printf(TEXT("N = %d framing is flag + count + id + length"), Count), ListBits, static_cast<int64>(1 + 8 + 8 + 8 + 8 * Payload));
	}
	TestEqual(TEXT("N = 16 is the 19 byte payload of the spec table"), PayloadBytesOf(MakeFragment(16)), 19);
	FGSTN_SyncFragmentArray Sixteen = ListOf(MakeFragment(16));
	TestEqual(TEXT("and 23 bytes in the list (P0c)"), (BitsOfList(Sixteen) + 7) / 8, static_cast<int64>(23));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsFragmentZeroTest, "GSTNParts.NP.Fragment.ZeroPartsZeroBytes", Flags)
bool FGSTN_PartsFragmentZeroTest::RunTest(const FString& Parameters)
{
	FScopedFragments Registered;

	// An entity without a part set has no fragment: the list is the flag bit and nothing else, and nothing is compared
	FGSTN_SyncFragmentArray None;
	TestEqual(TEXT("no fragments: one bit"), BitsOfList(None), static_cast<int64>(1));
	TestFalse(TEXT("two entities without parts never reconcile"), FGSTN_SyncFragmentList::ShouldReconcile(None, FGSTN_SyncFragmentArray()));

	// An entity whose parts are all pristine carries an empty fragment: zero payload bytes
	FGSTN_PartsFragment Pristine;
	Pristine.PartCount = 16;
	TestEqual(TEXT("a pristine fragment writes zero bytes"), PayloadBytesOf(Pristine), 0);
	TestEqual(TEXT("an unset fragment too"), PayloadBytesOf(FGSTN_PartsFragment()), 0);

	// And reading nothing gives the empty fragment back, whatever the part count was
	FGSTN_PartsFragment Read = MakeFull(4);
	TArray<uint8> NoBytes;
	FMemoryReader Reader(NoBytes);
	Read.NetSerialize(Reader);
	TestFalse(TEXT("an empty payload reads without error"), Reader.IsError());
	TestTrue(TEXT("and clears the old state"), Read.IsEmpty() && Read.PartCount == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsFragmentUnknownTest, "GSTNParts.NP.Fragment.UnknownTypeIdIgnored", Flags)
bool FGSTN_PartsFragmentUnknownTest::RunTest(const FString& Parameters)
{
	FScopedFragments Registered;
	FGSTN_SyncFragmentArray State;
	FGSTN_SyncFragmentList::FindOrAdd<FGSTN_PartsFragment>(State) = MakeFragment(8);
	FGSTN_PartsTestFragment& Other = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_PartsTestFragment>(State);
	Other.Frame = 123;
	Other.Count = 2;

	FBitWriter Writer(256, true);
	FGSTN_SyncFragmentList::NetSerialize(Writer, State);

	// A machine without the plugin: the parts id is not registered, its 11 bytes are skipped, and the fragment behind it still reads
	FGSTN_PartsFragmentRegistration::Unregister();
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_SyncFragmentArray Loaded;
	FGSTN_SyncFragmentList::NetSerialize(Reader, Loaded);
	FGSTN_PartsFragmentRegistration::Register();

	TestFalse(TEXT("the reader ends exactly where the writer did"), Reader.IsError() || !Reader.AtEnd());
	TestEqual(TEXT("only the fragment the machine knows is kept"), Loaded.Num(), 1);
	const FGSTN_PartsTestFragment* ReadOther = FGSTN_SyncFragmentList::Find<FGSTN_PartsTestFragment>(Loaded);
	TestTrue(TEXT("the fragment behind the unknown one reads intact"), ReadOther && ReadOther->Frame == 123 && ReadOther->Count == 2);
	TestTrue(TEXT("no parts fragment"), FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Loaded) == nullptr);

	// A payload of a size no part set can have, or a serial that does not fit 4 bits, is dropped without moving the reader
	AddExpectedMessage(TEXT("could not be read"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2);
	for (const TArray<uint8>& Bad : { TArray<uint8>({ 0x01, 0x00 }), TArray<uint8>({ 0x00, 0x00, 0x00, 16 }) })
	{
		FBitWriter BadWriter(256, true);
		uint8 bHas = 1;
		BadWriter.SerializeBits(&bHas, 1);
		uint32 FragmentCount = 1;
		BadWriter.SerializeIntPacked(FragmentCount);
		uint8 Id = GSTNPartsFeatureIds::Parts;
		BadWriter << Id;
		uint32 Length = static_cast<uint32>(Bad.Num());
		BadWriter.SerializeIntPacked(Length);
		BadWriter.Serialize(const_cast<uint8*>(Bad.GetData()), Length);
		FBitReader BadReader(BadWriter.GetData(), BadWriter.GetNumBits());
		FGSTN_SyncFragmentArray Read;
		FGSTN_SyncFragmentList::NetSerialize(BadReader, Read);
		TestTrue(TEXT("a malformed parts payload is dropped"), Read.IsEmpty());
		TestFalse(TEXT("and the reader is where the writer ended"), BadReader.IsError() || !BadReader.AtEnd());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsFragmentToStringTest, "GSTNParts.NP.Fragment.ToStringStable", Flags)
bool FGSTN_PartsFragmentToStringTest::RunTest(const FString& Parameters)
{
	FScopedFragments Registered;
	const auto Describe = [](const FGSTN_PartsFragment& Fragment)
	{
		TAnsiStringBuilder<256> Builder;
		Fragment.ToString(Builder);
		return FString(ANSI_TO_TCHAR(Builder.ToString()));
	};

	FGSTN_PartsFragment Fragment;
	Fragment.BrokenMask = 0x0005;
	Fragment.BreakSerial = 9;
	Fragment.PartCount = 4;
	Fragment.PartDamage[0] = 128;
	Fragment.PartDamage[1] = 255;
	Fragment.PartDamage[3] = 3;
	TestEqual(TEXT("mask in hex, serial, count and the percent of each part"), Describe(Fragment), FString(TEXT("Parts mask 0x0005 serial 9 n=4 [0:50% 1:100% 2:0% 3:1%]")));
	TestEqual(TEXT("the empty fragment"), Describe(FGSTN_PartsFragment()), FString(TEXT("Parts mask 0x0000 serial 0 n=0 []")));
	TestEqual(TEXT("printing twice gives the same text"), Describe(Fragment), Describe(Fragment));

	// Printing after a wire round trip gives the same text as before it
	FGSTN_SyncFragmentArray List = ListOf(Fragment);
	FBitWriter Writer(256, true);
	FGSTN_SyncFragmentList::NetSerialize(Writer, List);
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_SyncFragmentArray Read;
	FGSTN_SyncFragmentList::NetSerialize(Reader, Read);
	const FGSTN_PartsFragment* ReadFragment = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Read);
	TestTrue(TEXT("the fragment came back"), ReadFragment != nullptr);
	if (ReadFragment)
	{
		TestEqual(TEXT("same text after the wire"), Describe(*ReadFragment), Describe(Fragment));
	}

	TAnsiStringBuilder<256> Whole;
	FGSTN_SyncFragmentList::ToString(List, Whole);
	TestTrue(TEXT("the list text names the id and the fragment"), FString(ANSI_TO_TCHAR(Whole.ToString())).Contains(TEXT("Fragments: [16: Parts mask 0x0005 serial 9 n=4 [0:50% 1:100% 2:0% 3:1%]]")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
