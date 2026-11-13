// Automation tests of the combat sync state's wire format and its sync fragments. They need no world: the state is plain data
// and the archives are bit archives, so the printed sizes are what goes on the wire.

#include "GSTN_CombatNPTestTypes.h"
#include "Misc/AutomationTest.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Simulation/GSTN_CombatSimTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCombatNPTest, Log, All);

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** Registers the test fragments for the life of a test, like their plugin's module would */
	struct FScopedTestFragments
	{
		FScopedTestFragments()
		{
			FGSTN_SyncFragmentRegistry::Register<FGSTN_TestFragmentA>(GSTNFeatureIds::TestA);
			FGSTN_SyncFragmentRegistry::Register<FGSTN_TestFragmentB>(GSTNFeatureIds::TestB);
		}

		~FScopedTestFragments()
		{
			FGSTN_SyncFragmentRegistry::Unregister(GSTNFeatureIds::TestA);
			FGSTN_SyncFragmentRegistry::Unregister(GSTNFeatureIds::TestB);
		}
	};

	FGSTN_CombatSyncState MakeState()
	{
		FGSTN_CombatSyncState State;
		State.Core.Input.RequestedBinding = 3;
		State.Core.Input.RequestFrame = 42;
		State.Core.Input.LastActivatedFrame = 41;
		State.Core.HitstopStartFrame = 10;
		State.Core.HitstopEndFrame = 18;
		return State;
	}

	/** Bits the state takes on the wire */
	int64 BitsOf(FGSTN_CombatSyncState& State, TArray<uint8>* OutBytes = nullptr)
	{
		FBitWriter Writer(256, true);
		State.NetSerialize(FNetSerializeParams(Writer));
		if (OutBytes)
		{
			OutBytes->Append(Writer.GetData(), Writer.GetNumBytes());
		}
		return Writer.GetNumBits();
	}

	int64 BitsOfCore(FGSTN_CombatSyncState& State)
	{
		FBitWriter Writer(256, true);
		State.Core.NetSerialize(Writer);
		return Writer.GetNumBits();
	}

	int64 BitsOfFragments(FGSTN_SyncFragmentArray& Fragments)
	{
		FBitWriter Writer(256, true);
		FGSTN_SyncFragmentList::NetSerialize(Writer, Fragments);
		return Writer.GetNumBits();
	}

	/** Reads a state written by BitsOf; Error says whether the reader failed */
	FGSTN_CombatSyncState ReadBack(FGSTN_CombatSyncState& State, bool& bOutError)
	{
		FBitWriter Writer(256, true);
		State.NetSerialize(FNetSerializeParams(Writer));
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_CombatSyncState Loaded;
		Loaded.NetSerialize(FNetSerializeParams(Reader));
		bOutError = Reader.IsError() || !Reader.AtEnd();
		return Loaded;
	}

	bool SameCore(const FGSTN_CombatSyncState& A, const FGSTN_CombatSyncState& B)
	{
		return A.Core.Input.RequestedBinding == B.Core.Input.RequestedBinding && A.Core.Input.RequestFrame == B.Core.Input.RequestFrame
			&& A.Core.Input.LastActivatedFrame == B.Core.Input.LastActivatedFrame && A.Core.HitstopStartFrame == B.Core.HitstopStartFrame
			&& A.Core.HitstopEndFrame == B.Core.HitstopEndFrame;
	}

	FString ToText(const FGSTN_CombatSyncState& State)
	{
		TAnsiStringBuilder<512> Builder;
		State.ToString(Builder);
		return FString(ANSI_TO_TCHAR(Builder.ToString()));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_FragmentsRoundTrip0Test, "GSTNCombat.NP.Fragments.RoundTrip0", GSTN_NP_TEST_FLAGS)

bool FGSTN_FragmentsRoundTrip0Test::RunTest(const FString& Parameters)
{
	FScopedTestFragments Registered;
	FGSTN_CombatSyncState State = MakeState();

	// No fragments: exactly one bit on top of the core, which is the same bytes it always was
	const int64 CoreBits = BitsOfCore(State);
	const int64 Bits = BitsOf(State);
	UE_LOG(LogGSTNCombatNPTest, Display, TEXT("Fragments.RoundTrip0: %lld bits (%lld bytes) for the whole state, %lld of them the core"), Bits, (Bits + 7) / 8, CoreBits);
	TestEqual(TEXT("zero fragments cost one bit"), Bits, CoreBits + 1);
	FGSTN_SyncFragmentArray None;
	TestEqual(TEXT("the empty list alone is one bit"), BitsOfFragments(None), static_cast<int64>(1));

	bool bError = false;
	const FGSTN_CombatSyncState Loaded = ReadBack(State, bError);
	TestFalse(TEXT("reads back to the end without error"), bError);
	TestTrue(TEXT("core"), SameCore(Loaded, State));
	TestTrue(TEXT("no fragments"), Loaded.Fragments.IsEmpty());
	TestFalse(TEXT("no reconcile against itself"), Loaded.ShouldReconcile(State));

	// The text of a state without fragments is what it was before fragments existed
	TestFalse(TEXT("no fragments line"), ToText(State).Contains(TEXT("Fragments")));
	TestTrue(TEXT("hitstop is still described"), ToText(State).Contains(TEXT("Hitstop: [10, 18)")));

	// Interpolation takes the newer core, as before
	FGSTN_CombatSyncState Older = MakeState();
	Older.Core.Input.RequestFrame = 1;
	FGSTN_CombatSyncState Interpolated;
	Interpolated.Interpolate(&Older, &State, 0.5f);
	TestTrue(TEXT("interpolation takes the newer state"), SameCore(Interpolated, State));

	// The core reconciles on the hitstop window and on the input state, which is authoritative like the rest
	FGSTN_CombatSyncState Authority = State;
	TestFalse(TEXT("equal"), State.ShouldReconcile(Authority));
	Authority.Core.HitstopEndFrame = 19;
	TestTrue(TEXT("hitstop mismatch"), State.ShouldReconcile(Authority));
	Authority = State;
	Authority.Core.Input.RequestFrame = 7;
	TestTrue(TEXT("an input state mismatch asks for a reconcile"), State.ShouldReconcile(Authority));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_FragmentsRoundTrip1Test, "GSTNCombat.NP.Fragments.RoundTrip1", GSTN_NP_TEST_FLAGS)

bool FGSTN_FragmentsRoundTrip1Test::RunTest(const FString& Parameters)
{
	FScopedTestFragments Registered;
	FGSTN_CombatSyncState State = MakeState();
	FGSTN_TestFragmentA& A = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_TestFragmentA>(State.Fragments);
	A.Frame = 77;
	A.Count = 3;

	// 1 flag bit, packed count (8), type id (8), packed length (8), 5 payload bytes
	const int64 Bits = BitsOf(State);
	UE_LOG(LogGSTNCombatNPTest, Display, TEXT("Fragments.RoundTrip1: %lld bits (%lld bytes) for the whole state, %lld bits for the fragment list"), Bits, (Bits + 7) / 8, BitsOfFragments(State.Fragments));
	TestEqual(TEXT("one fragment: flag, count, id, length and payload"), Bits, BitsOfCore(State) + 1 + 8 + 8 + 8 + 8 * 5);

	bool bError = false;
	FGSTN_CombatSyncState Loaded = ReadBack(State, bError);
	TestFalse(TEXT("reads back to the end without error"), bError);
	TestTrue(TEXT("core"), SameCore(Loaded, State));
	TestEqual(TEXT("one fragment"), Loaded.Fragments.Num(), 1);
	const FGSTN_TestFragmentA* LoadedA = FGSTN_SyncFragmentList::Find<FGSTN_TestFragmentA>(Loaded.Fragments);
	TestTrue(TEXT("it is an A"), LoadedA != nullptr);
	if (LoadedA)
	{
		TestEqual(TEXT("frame"), LoadedA->Frame, 77);
		TestEqual(TEXT("count"), static_cast<int32>(LoadedA->Count), 3);
	}

	// Reconcile: equal, then a fragment mismatch, then a missing fragment
	TestFalse(TEXT("equal states"), Loaded.ShouldReconcile(State));
	FGSTN_SyncFragmentList::Find<FGSTN_TestFragmentA>(Loaded.Fragments)->Count = 4;
	TestTrue(TEXT("a fragment value differs"), Loaded.ShouldReconcile(State));
	TestTrue(TEXT("a fragment is missing on one side"), FGSTN_CombatSyncState().ShouldReconcile(State));
	TestTrue(TEXT("and on the other"), State.ShouldReconcile(FGSTN_CombatSyncState()));

	// ToString and Interpolate (a discrete fragment takes the newer one)
	TestTrue(TEXT("the fragment is described"), ToText(State).Contains(TEXT("Fragments: [240: A frame 77 count 3]")));
	FGSTN_CombatSyncState Older = State;
	FGSTN_SyncFragmentList::Find<FGSTN_TestFragmentA>(Older.Fragments)->Frame = 5;
	FGSTN_CombatSyncState Interpolated;
	Interpolated.Interpolate(&Older, &State, 0.5f);
	TestEqual(TEXT("interpolated fragment frame"), FGSTN_SyncFragmentList::Find<FGSTN_TestFragmentA>(Interpolated.Fragments)->Frame, 77);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_FragmentsRoundTrip2Test, "GSTNCombat.NP.Fragments.RoundTrip2", GSTN_NP_TEST_FLAGS)

bool FGSTN_FragmentsRoundTrip2Test::RunTest(const FString& Parameters)
{
	FScopedTestFragments Registered;

	// B is added first, but the list keeps type id order
	FGSTN_CombatSyncState State = MakeState();
	FGSTN_SyncFragmentList::FindOrAdd<FGSTN_TestFragmentB>(State.Fragments).Value = 100;
	FGSTN_TestFragmentA& A = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_TestFragmentA>(State.Fragments);
	A.Frame = 9;
	A.Count = 1;
	TestEqual(TEXT("two fragments"), State.Fragments.Num(), 2);
	TestEqual(TEXT("sorted by type id: A first"), FGSTN_SyncFragmentRegistry::FindId(State.Fragments[0].GetScriptStruct()), GSTNFeatureIds::TestA);
	TestEqual(TEXT("then B"), FGSTN_SyncFragmentRegistry::FindId(State.Fragments[1].GetScriptStruct()), GSTNFeatureIds::TestB);

	// 1 flag bit, packed count, then per fragment id + length + payload: A has 5 bytes, B has 4
	TArray<uint8> SortedBytes;
	const int64 Bits = BitsOf(State, &SortedBytes);
	UE_LOG(LogGSTNCombatNPTest, Display, TEXT("Fragments.RoundTrip2: %lld bits (%lld bytes) for the whole state"), Bits, (Bits + 7) / 8);
	TestEqual(TEXT("two fragments"), Bits, BitsOfCore(State) + 1 + 8 + (8 + 8 + 8 * 5) + (8 + 8 + 8 * 4));

	bool bError = false;
	FGSTN_CombatSyncState Loaded = ReadBack(State, bError);
	TestFalse(TEXT("reads back to the end without error"), bError);
	TestEqual(TEXT("two fragments"), Loaded.Fragments.Num(), 2);
	TestEqual(TEXT("A frame"), FGSTN_SyncFragmentList::Find<FGSTN_TestFragmentA>(Loaded.Fragments)->Frame, 9);
	TestEqual(TEXT("B value"), FGSTN_SyncFragmentList::Find<FGSTN_TestFragmentB>(Loaded.Fragments)->Value, 100);
	TestFalse(TEXT("equal states"), Loaded.ShouldReconcile(State));

	// The wire does not depend on the order of the array
	FGSTN_CombatSyncState Unordered = State;
	Unordered.Fragments.Swap(0, 1);
	TArray<uint8> UnorderedBytes;
	BitsOf(Unordered, &UnorderedBytes);
	TestTrue(TEXT("same bytes whatever the array order"), SortedBytes == UnorderedBytes);
	TestFalse(TEXT("and the same state for the reconcile check"), Unordered.ShouldReconcile(State));

	// Interpolation: B blends, A takes the newer
	FGSTN_CombatSyncState Older = State;
	FGSTN_SyncFragmentList::Find<FGSTN_TestFragmentB>(Older.Fragments)->Value = 0;
	FGSTN_SyncFragmentList::Find<FGSTN_TestFragmentA>(Older.Fragments)->Frame = 1;
	FGSTN_CombatSyncState Interpolated;
	Interpolated.Interpolate(&Older, &State, 0.25f);
	TestEqual(TEXT("B blended a quarter of the way"), FGSTN_SyncFragmentList::Find<FGSTN_TestFragmentB>(Interpolated.Fragments)->Value, 25);
	TestEqual(TEXT("A is the newer"), FGSTN_SyncFragmentList::Find<FGSTN_TestFragmentA>(Interpolated.Fragments)->Frame, 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_FragmentsUnsendableTest, "GSTNCombat.NP.Fragments.UnsendableDoesNotReconcile", GSTN_NP_TEST_FLAGS)

bool FGSTN_FragmentsUnsendableTest::RunTest(const FString& Parameters)
{
	// A fragment that cannot be sent (here: its type was unregistered after it was added) never reaches the authority's copy,
	// so it must not take part in the comparison: otherwise the client would ask for a reconcile on every frame
	FGSTN_CombatSyncState Local = MakeState();
	{
		FScopedTestFragments Registered;
		FGSTN_SyncFragmentList::FindOrAdd<FGSTN_TestFragmentA>(Local.Fragments).Frame = 12;
		FGSTN_SyncFragmentList::FindOrAdd<FGSTN_TestFragmentB>(Local.Fragments).Value = 34;
	}
	FGSTN_SyncFragmentRegistry::Register<FGSTN_TestFragmentA>(GSTNFeatureIds::TestA);

	FGSTN_CombatSyncState Authority = MakeState();
	FGSTN_SyncFragmentList::FindOrAdd<FGSTN_TestFragmentA>(Authority.Fragments).Frame = 12;

	TestFalse(TEXT("the unregistered fragment of the local state is ignored"), Local.ShouldReconcile(Authority));
	FGSTN_SyncFragmentList::Find<FGSTN_TestFragmentA>(Authority.Fragments)->Frame = 13;
	TestTrue(TEXT("a sendable fragment still reconciles"), Local.ShouldReconcile(Authority));

	FGSTN_SyncFragmentRegistry::Unregister(GSTNFeatureIds::TestA);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_FragmentsUnknownTypeIdTest, "GSTNCombat.NP.Fragments.UnknownTypeId", GSTN_NP_TEST_FLAGS)

bool FGSTN_FragmentsUnknownTypeIdTest::RunTest(const FString& Parameters)
{
	constexpr int32 Sentinel = 0x5EED5EED;

	// The writer knows A and B; the reader is a machine whose plugin for B is not loaded
	FBitWriter Writer(256, true);
	{
		FScopedTestFragments Registered;
		FGSTN_CombatSyncState State = MakeState();
		FGSTN_SyncFragmentList::FindOrAdd<FGSTN_TestFragmentA>(State.Fragments).Frame = 12;
		FGSTN_SyncFragmentList::FindOrAdd<FGSTN_TestFragmentB>(State.Fragments).Value = 34;
		State.NetSerialize(FNetSerializeParams(Writer));
		int32 Value = Sentinel;
		Writer << Value;
	}

	{
		FGSTN_SyncFragmentRegistry::Register<FGSTN_TestFragmentA>(GSTNFeatureIds::TestA);
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_CombatSyncState Loaded;
		Loaded.NetSerialize(FNetSerializeParams(Reader));
		int32 After = 0;
		Reader << After;

		TestFalse(TEXT("an unknown id is not an error"), Reader.IsError());
		TestEqual(TEXT("the reader is exactly after the fragment list: the next field reads back"), After, Sentinel);
		TestTrue(TEXT("it reached the end"), Reader.AtEnd());
		TestEqual(TEXT("only the known fragment is kept"), Loaded.Fragments.Num(), 1);
		const FGSTN_TestFragmentA* A = FGSTN_SyncFragmentList::Find<FGSTN_TestFragmentA>(Loaded.Fragments);
		TestTrue(TEXT("it is A"), A != nullptr);
		if (A)
		{
			TestEqual(TEXT("with its value"), A->Frame, 12);
		}
		FGSTN_SyncFragmentRegistry::Unregister(GSTNFeatureIds::TestA);
	}

	// With neither registered every fragment is skipped and the state is the core
	{
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_CombatSyncState Loaded;
		Loaded.NetSerialize(FNetSerializeParams(Reader));
		int32 After = 0;
		Reader << After;
		TestFalse(TEXT("no error"), Reader.IsError());
		TestEqual(TEXT("sentinel"), After, Sentinel);
		TestTrue(TEXT("no fragments"), Loaded.Fragments.IsEmpty());
		TestEqual(TEXT("core request frame"), Loaded.Core.Input.RequestFrame, 42);
	}

	// A malformed list (a count over the limit) is an error, not a crash or a huge allocation
	{
		FBitWriter Bad(256, true);
		FGSTN_CombatSyncCore Core;
		Core.NetSerialize(Bad);
		uint8 bHas = 1;
		Bad.SerializeBits(&bHas, 1);
		uint32 Count = 200;
		Bad.SerializeIntPacked(Count);

		FBitReader Reader(Bad.GetData(), Bad.GetNumBits());
		FGSTN_CombatSyncState Loaded;
		Loaded.NetSerialize(FNetSerializeParams(Reader));
		TestTrue(TEXT("a count over the limit fails the archive"), Reader.IsError());
		TestTrue(TEXT("and leaves no fragments"), Loaded.Fragments.IsEmpty());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
