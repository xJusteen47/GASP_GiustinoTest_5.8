// Automation tests of the defence sync fragment (M6 F7, spec 5.1 and 10): the idle fragment costs the list's framing and nothing else, an
// active one stays within the spec's byte table, every field reconciles exactly, interpolation keeps the later sample, and a forced
// rollback (the authority's bytes replace the local fragment) leaves a fragment equal to the authority's. No world: plain data and bit archives.

#include "GSTN_DefenceFragmentRegistration.h"
#include "GSTN_DefenceNPTestFixture.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogGSTNDefenceNPTest, Log, All);

using namespace GSTNDefenceNPTest;

namespace
{
	/** The module registers the fragment at startup; a test re-registers in case an earlier test unregistered it */
	struct FDefScopedFragment
	{
		FDefScopedFragment() { FGSTN_DefenceFragmentRegistration::Register(); }
		~FDefScopedFragment() { FGSTN_DefenceFragmentRegistration::Register(); }
	};

	int64 DefBitsOfList(FGSTN_SyncFragmentArray& Fragments)
	{
		FBitWriter Writer(1024, true);
		FGSTN_SyncFragmentList::NetSerialize(Writer, Fragments);
		return Writer.GetNumBits();
	}

	FGSTN_SyncFragmentArray DefReadBack(FGSTN_SyncFragmentArray& Fragments, bool& bOutError)
	{
		FBitWriter Writer(1024, true);
		FGSTN_SyncFragmentList::NetSerialize(Writer, Fragments);
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_SyncFragmentArray Loaded;
		FGSTN_SyncFragmentList::NetSerialize(Reader, Loaded);
		bOutError = Reader.IsError() || !Reader.AtEnd();
		return Loaded;
	}

	int32 DefPayloadBytes(const FGSTN_DefenceFragment& Fragment)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		const_cast<FGSTN_DefenceFragment&>(Fragment).NetSerialize(Writer);
		return Bytes.Num();
	}

	FGSTN_SyncFragmentArray DefListOf(const FGSTN_DefenceFragment& Fragment)
	{
		FGSTN_SyncFragmentArray List;
		SetFragment(List, Fragment);
		return List;
	}

	/** Every group live at once: the worst case of the wire */
	FGSTN_DefenceFragment DefMakeEverything()
	{
		FGSTN_DefenceFragment F = MakeRaised(100, 37.25);
		F.ShieldReleaseFrame = 130;
		F.ShieldFlags = GSTNShieldFlags::Breaking;
		F.MoveParryStartFrame = 140;
		F.MoveParryEndFrame = 150;
		F.MoveParryProfile = 1;
		F.MoveParryHurtboxMask = 0x8003;
		F.LastParryFrame = 145;
		F.ShieldStunEndFrame = 160;
		F.ParryRecoveryEndFrame = 161;
		F.BreakEndFrame = 300;
		F.PunishEndFrame = 170;
		F.PunishKind = 1;
		F.TechLockoutEndFrame = 180;
		F.LastTechPressFrame = 175;
		F.TechEndFrame = 190;
		F.TechType = 2;
		F.KnockdownEndFrame = 200;
		F.GrabImmunityEndFrame = 210;
		F.SetPushback(146, FVector(-300.0, 40.0, 0.0), 8);
		F.MirrorValueQ = F.ShieldValueQ;
		F.MirrorFrame = 144;
		return F;
	}

	FString DefText(const FGSTN_DefenceFragment& Fragment)
	{
		TAnsiStringBuilder<512> Builder;
		Fragment.ToString(Builder);
		return FString(ANSI_TO_TCHAR(Builder.ToString()));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceFragmentIdleTest, "GSTNDefence.NP.Fragment.RoundTripIdleOneBit", TestFlags)
bool FGSTN_DefenceFragmentIdleTest::RunTest(const FString& Parameters)
{
	FDefScopedFragment Registered;
	TestTrue(TEXT("the module registered the fragment under the defence feature id"),
		FGSTN_SyncFragmentRegistry::FindStruct(GSTNFeatureIds::Defence) == FGSTN_DefenceFragment::StaticStruct());

	// An entity that never defended has no fragment: the list is the one flag bit, as without the plugin
	FGSTN_SyncFragmentArray None;
	TestEqual(TEXT("no fragment: one bit"), DefBitsOfList(None), static_cast<int64>(1));

	// A fragment at its defaults writes zero payload bytes: only the list framing (flag, count, id, length) is paid
	const FGSTN_DefenceFragment Idle;
	TestTrue(TEXT("a fresh fragment is the default"), Idle.IsDefault());
	TestEqual(TEXT("the idle payload is empty"), DefPayloadBytes(Idle), 0);
	FGSTN_SyncFragmentArray IdleList = DefListOf(Idle);
	TestEqual(TEXT("idle framing is flag + count + id + length"), DefBitsOfList(IdleList), static_cast<int64>(1 + 8 + 8 + 8));

	bool bError = false;
	const FGSTN_SyncFragmentArray Loaded = DefReadBack(IdleList, bError);
	TestFalse(TEXT("reads back to the end without error"), bError);
	const FGSTN_DefenceFragment* Read = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Loaded);
	TestTrue(TEXT("the idle fragment arrives as the default"), Read && Read->IsDefault());
	TestFalse(TEXT("and does not reconcile against the original"), FGSTN_SyncFragmentList::ShouldReconcile(Loaded, IdleList));
	TestEqual(TEXT("the idle text names the fragment only"), DefText(Idle), FString(TEXT("Defence")));

	// Reading nothing into a live fragment clears it
	FGSTN_DefenceFragment Live = DefMakeEverything();
	TArray<uint8> NoBytes;
	FMemoryReader Reader(NoBytes);
	Live.NetSerialize(Reader);
	TestFalse(TEXT("an empty payload reads without error"), Reader.IsError());
	TestTrue(TEXT("and gives the default"), Live.IsDefault());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceFragmentBytesTest, "GSTNDefence.NP.Fragment.ActiveBytesWithinTable", TestFlags)
bool FGSTN_DefenceFragmentBytesTest::RunTest(const FString& Parameters)
{
	FDefScopedFragment Registered;

	// A raised shield: the mask and the shield group (value 2, stamp 4, flags 1, press 4, release 4): 17 bytes, against the spec's 15 raw + framing
	const FGSTN_DefenceFragment Raised = MakeRaised(100);
	const int32 RaisedBytes = DefPayloadBytes(Raised);
	UE_LOG(LogGSTNDefenceNPTest, Display, TEXT("Fragment bytes: raised shield %d B payload (spec 10 shield row 15 B raw + 2 B mask)"), RaisedBytes);
	TestEqual(TEXT("a raised shield is the mask and the shield group"), RaisedBytes, 2 + 15);

	// Everything live: 2 + 85 bytes, under the spec's ~100 B total for the defence rows (shield 15, stuns 17, tech + knockdown 17, move parry 13,
	// grab immunity 4, punish 5) plus this build's pushback (11) and mirror (6) groups
	const FGSTN_DefenceFragment Everything = DefMakeEverything();
	const int32 AllBytes = DefPayloadBytes(Everything);
	UE_LOG(LogGSTNDefenceNPTest, Display, TEXT("Fragment bytes: every group live %d B payload"), AllBytes);
	TestEqual(TEXT("every group live"), AllBytes, 2 + 85);
	TestTrue(TEXT("within the fragment list's payload limit"), AllBytes <= FGSTN_SyncFragmentList::MaxFragmentBytes);

	// Round trip of the worst case through the list framing: every field survives and nothing is left in the reader
	FGSTN_SyncFragmentArray List = DefListOf(Everything);
	TestEqual(TEXT("framing: flag, count, id, length, payload"), DefBitsOfList(List), static_cast<int64>(1 + 8 + 8 + 8 + 8 * AllBytes));
	bool bError = false;
	const FGSTN_SyncFragmentArray Loaded = DefReadBack(List, bError);
	TestFalse(TEXT("reads back to the end without error"), bError);
	const FGSTN_DefenceFragment* Read = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Loaded);
	TestTrue(TEXT("the fragment is there"), Read != nullptr);
	if (!Read)
	{
		return false;
	}
	TestTrue(TEXT("every field survives the wire"), *Read == Everything);
	TestEqual(TEXT("and prints the same"), DefText(*Read), DefText(Everything));
	TestFalse(TEXT("equal fragments do not reconcile"), FGSTN_SyncFragmentList::ShouldReconcile(Loaded, List));

	// Exact reconcile: one unit of difference in any field is a reconcile, both ways round
	int32 Checked = 0;
	const auto Differs = [&](const TCHAR* What, TFunctionRef<void(FGSTN_DefenceFragment&)> Change)
	{
		FGSTN_DefenceFragment Other = Everything;
		Change(Other);
		TestTrue(What, Everything.ShouldReconcile(Other) && Other.ShouldReconcile(Everything));
		++Checked;
	};
	Differs(TEXT("shield value"), [](FGSTN_DefenceFragment& F) { F.ShieldValueQ += 1; });
	Differs(TEXT("shield stamp"), [](FGSTN_DefenceFragment& F) { F.ShieldStampFrame += 1; });
	Differs(TEXT("shield flags"), [](FGSTN_DefenceFragment& F) { F.ShieldFlags ^= GSTNShieldFlags::Raised; });
	Differs(TEXT("press"), [](FGSTN_DefenceFragment& F) { F.ShieldPressFrame += 1; });
	Differs(TEXT("release"), [](FGSTN_DefenceFragment& F) { F.ShieldReleaseFrame += 1; });
	Differs(TEXT("move parry start"), [](FGSTN_DefenceFragment& F) { F.MoveParryStartFrame += 1; });
	Differs(TEXT("move parry end"), [](FGSTN_DefenceFragment& F) { F.MoveParryEndFrame += 1; });
	Differs(TEXT("move parry profile"), [](FGSTN_DefenceFragment& F) { F.MoveParryProfile += 1; });
	Differs(TEXT("move parry mask"), [](FGSTN_DefenceFragment& F) { F.MoveParryHurtboxMask ^= 1; });
	Differs(TEXT("last parry"), [](FGSTN_DefenceFragment& F) { F.LastParryFrame += 1; });
	Differs(TEXT("shield stun"), [](FGSTN_DefenceFragment& F) { F.ShieldStunEndFrame += 1; });
	Differs(TEXT("parry recovery"), [](FGSTN_DefenceFragment& F) { F.ParryRecoveryEndFrame += 1; });
	Differs(TEXT("break end"), [](FGSTN_DefenceFragment& F) { F.BreakEndFrame += 1; });
	Differs(TEXT("punish end"), [](FGSTN_DefenceFragment& F) { F.PunishEndFrame += 1; });
	Differs(TEXT("punish kind"), [](FGSTN_DefenceFragment& F) { F.PunishKind += 1; });
	Differs(TEXT("tech lockout"), [](FGSTN_DefenceFragment& F) { F.TechLockoutEndFrame += 1; });
	Differs(TEXT("tech press"), [](FGSTN_DefenceFragment& F) { F.LastTechPressFrame += 1; });
	Differs(TEXT("tech end"), [](FGSTN_DefenceFragment& F) { F.TechEndFrame += 1; });
	Differs(TEXT("tech type"), [](FGSTN_DefenceFragment& F) { F.TechType += 1; });
	Differs(TEXT("knockdown"), [](FGSTN_DefenceFragment& F) { F.KnockdownEndFrame += 1; });
	Differs(TEXT("grab immunity"), [](FGSTN_DefenceFragment& F) { F.GrabImmunityEndFrame += 1; });
	Differs(TEXT("pushback start"), [](FGSTN_DefenceFragment& F) { F.PushbackStartFrame += 1; });
	Differs(TEXT("pushback velocity"), [](FGSTN_DefenceFragment& F) { F.PushbackVelocityQ[1] += 1; });
	Differs(TEXT("pushback frames"), [](FGSTN_DefenceFragment& F) { F.PushbackFrames += 1; });
	Differs(TEXT("mirror value"), [](FGSTN_DefenceFragment& F) { F.MirrorValueQ += 1; });
	Differs(TEXT("mirror frame"), [](FGSTN_DefenceFragment& F) { F.MirrorFrame += 1; });
	TestEqual(TEXT("26 single-field differences checked"), Checked, 26);

	// Quantisation: quarter points, round half up, so two machines agree on the bit
	TestEqual(TEXT("37.25 is 149 quarters"), static_cast<int32>(FGSTN_DefenceFragment::QuantizeValue(37.25)), 149);
	TestEqual(TEXT("0.125 rounds up to one quarter"), static_cast<int32>(FGSTN_DefenceFragment::QuantizeValue(0.125)), 1);
	TestEqual(TEXT("and back"), FGSTN_DefenceFragment::DequantizeValue(149), 37.25);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceFragmentInterpolateTest, "GSTNDefence.NP.Fragment.Interpolate", TestFlags)
bool FGSTN_DefenceFragmentInterpolateTest::RunTest(const FString& Parameters)
{
	FDefScopedFragment Registered;

	// Discrete state: the registry hands Interpolate a copy of the later sample, and the result is that sample at any fraction
	FGSTN_SyncFragmentArray Older = DefListOf(MakeRaised(100, 100.0));
	const FGSTN_DefenceFragment Later = MakeRaised(100, 92.0);
	FGSTN_SyncFragmentArray Newer = DefListOf(Later);
	for (const float Pct : { 0.f, 0.25f, 0.5f, 0.99f, 1.f })
	{
		FGSTN_SyncFragmentArray Out;
		FGSTN_SyncFragmentList::Interpolate(Out, &Older, Newer, Pct);
		const FGSTN_DefenceFragment* Blend = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Out);
		TestTrue(*FString::Printf(TEXT("pct %.2f keeps the later sample"), Pct), Blend && *Blend == Later);
	}

	// No earlier sample: the later one, unchanged
	FGSTN_SyncFragmentArray Alone;
	FGSTN_SyncFragmentList::Interpolate(Alone, nullptr, Newer, 0.5f);
	const FGSTN_DefenceFragment* Only = FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Alone);
	TestTrue(TEXT("without an earlier sample the later one stands"), Only && *Only == Later);

	// A fragment present only in the later sample (the shield was raised between the two) appears whole; one present only in the earlier is gone
	FGSTN_SyncFragmentArray Empty;
	FGSTN_SyncFragmentArray Appeared;
	FGSTN_SyncFragmentList::Interpolate(Appeared, &Empty, Newer, 0.5f);
	TestTrue(TEXT("a fragment of the later sample only is taken whole"), FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(Appeared) != nullptr);
	FGSTN_SyncFragmentArray Gone;
	FGSTN_SyncFragmentList::Interpolate(Gone, &Newer, Empty, 0.5f);
	TestTrue(TEXT("a fragment of the earlier sample only is gone"), Gone.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceFragmentRollbackTest, "GSTNDefence.NP.Fragment.ForcedRollbackTakesAuthority", TestFlags)
bool FGSTN_DefenceFragmentRollbackTest::RunTest(const FString& Parameters)
{
	FDefScopedFragment Registered;

	// The client predicted a held shield; the authority's frame says the shield blocked a hit (lower value, re-stamped, a shield stun) and
	// nothing else differs. The two must reconcile, and after the authority's bytes replace the local fragment nothing is left to reconcile
	FGSTN_SyncFragmentArray Local = DefListOf(MakeRaised(100, 100.0));
	FGSTN_DefenceFragment Blocked = MakeRaised(100, 100.0);
	Blocked.ShieldValueQ = FGSTN_DefenceFragment::QuantizeValue(88.0);
	Blocked.ShieldStampFrame = 110;
	Blocked.ShieldStunEndFrame = 125;
	Blocked.SetPushback(118, FVector(-250.0, 0.0, 0.0), 8);
	FGSTN_SyncFragmentArray Authority = DefListOf(Blocked);
	TestTrue(TEXT("the predicted and the authoritative shield differ: a rollback"), FGSTN_SyncFragmentList::ShouldReconcile(Local, Authority));

	FBitWriter Writer(1024, true);
	FGSTN_SyncFragmentList::NetSerialize(Writer, Authority);
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_SyncFragmentList::NetSerialize(Reader, Local);
	TestFalse(TEXT("the authority's bytes read cleanly"), Reader.IsError() || !Reader.AtEnd());
	TestTrue(TEXT("the local fragment is now the authority's"), ReadFragment(Local) == Blocked);
	TestFalse(TEXT("and nothing is left to reconcile"), FGSTN_SyncFragmentList::ShouldReconcile(Local, Authority));

	// A rollback to a frame BEFORE the shield existed: the authority has no fragment at all, and the local one must go with the list
	FGSTN_SyncFragmentArray NoFragment;
	FBitWriter EmptyWriter(64, true);
	FGSTN_SyncFragmentList::NetSerialize(EmptyWriter, NoFragment);
	FBitReader EmptyReader(EmptyWriter.GetData(), EmptyWriter.GetNumBits());
	FGSTN_SyncFragmentList::NetSerialize(EmptyReader, Local);
	TestTrue(TEXT("a restored state without the fragment has none"), Local.IsEmpty());
	TestFalse(TEXT("two empty lists never reconcile"), FGSTN_SyncFragmentList::ShouldReconcile(Local, NoFragment));

	// A machine without the plugin skips the defence bytes and keeps reading
	FBitReader Foreign(Writer.GetData(), Writer.GetNumBits());
	FGSTN_DefenceFragmentRegistration::Unregister();
	FGSTN_SyncFragmentArray Skipped;
	FGSTN_SyncFragmentList::NetSerialize(Foreign, Skipped);
	FGSTN_DefenceFragmentRegistration::Register();
	TestFalse(TEXT("the unknown id is skipped to the end"), Foreign.IsError() || !Foreign.AtEnd());
	TestTrue(TEXT("and nothing is kept"), Skipped.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
