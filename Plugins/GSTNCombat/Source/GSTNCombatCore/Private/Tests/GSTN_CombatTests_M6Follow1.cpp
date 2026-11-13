// Automation tests of the M6 follow-up 1 data seams (F1): the entity reference and the new hit data fields. Pure, no world.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitData.h"
#include "Rules/GSTN_CombatRules.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Sync/GSTN_EntityRef.h"
#include "Sync/GSTN_SyncFragment.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<uint8> SerializeRef(FGSTN_EntityRef Ref)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		bool bSuccess = false;
		Ref.NetSerialize(Writer, nullptr, bSuccess);
		return Bytes;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EntityRefRoundTripTest, "GSTNCombat.Core.EntityRef.RoundTrip", GSTN_CORE_TEST_FLAGS)
bool FGSTN_EntityRefRoundTripTest::RunTest(const FString& Parameters)
{
	const FGSTN_EntityRef Samples[] =
	{
		FGSTN_EntityRef(GSTNEntityKinds::Fighter, 0, 0),
		FGSTN_EntityRef(GSTNEntityKinds::Fighter, 12, 3),
		FGSTN_EntityRef(GSTNEntityKinds::Minion, 65535, 255),
		FGSTN_EntityRef(GSTNEntityKinds::Object, 300, 7),
		FGSTN_EntityRef(GSTNEntityKinds::FirstGame, 1, 1),
		FGSTN_EntityRef::Null()
	};

	for (const FGSTN_EntityRef& Sample : Samples)
	{
		const TArray<uint8> Bytes = SerializeRef(Sample);
		TestEqual(*FString::Printf(TEXT("%s takes 32 bits"), *Sample.ToString()), Bytes.Num(), 4);

		FMemoryReader Reader(Bytes);
		FGSTN_EntityRef Read(GSTNEntityKinds::Object, 999, 99);
		bool bSuccess = false;
		Read.NetSerialize(Reader, nullptr, bSuccess);
		TestTrue(TEXT("the read succeeds"), bSuccess);
		TestTrue(*FString::Printf(TEXT("%s comes back"), *Sample.ToString()), Read == Sample);
		TestEqual(TEXT("the same text"), Read.ToString(), Sample.ToString());
	}

	// Equal references have equal bytes, so the sync state compares them bit for bit: a null with leftover fields is written as the null
	FGSTN_EntityRef Dirty(GSTNEntityKinds::None, 77, 5);
	TestTrue(TEXT("a null with leftovers writes the null bytes"), SerializeRef(Dirty) == SerializeRef(FGSTN_EntityRef::Null()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EntityRefNullAndEqualityTest, "GSTNCombat.Core.EntityRef.NullAndEquality", GSTN_CORE_TEST_FLAGS)
bool FGSTN_EntityRefNullAndEqualityTest::RunTest(const FString& Parameters)
{
	const FGSTN_EntityRef Default;
	TestTrue(TEXT("the default is the null reference"), Default.IsNull());
	TestTrue(TEXT("Null() is null"), FGSTN_EntityRef::Null().IsNull());
	TestEqual(TEXT("null prints as None"), Default.ToString(), FString(TEXT("None")));

	// The fighter is kind 0: a fighter with id 0 must not be mistaken for the null reference
	const FGSTN_EntityRef FighterZero(GSTNEntityKinds::Fighter, 0, 0);
	TestFalse(TEXT("fighter 0 is not null"), FighterZero.IsNull());
	TestTrue(TEXT("and differs from null"), FighterZero != Default);
	TestEqual(TEXT("fighter text"), FighterZero.ToString(), FString(TEXT("Fighter:0#0")));

	const FGSTN_EntityRef Other(GSTNEntityKinds::None, 9, 9);
	TestTrue(TEXT("every null equals every other null"), Other == Default);
	TestEqual(TEXT("and hashes alike"), GetTypeHash(Other), GetTypeHash(Default));

	const FGSTN_EntityRef A(GSTNEntityKinds::Fighter, 4, 1);
	TestTrue(TEXT("same three fields are equal"), A == FGSTN_EntityRef(GSTNEntityKinds::Fighter, 4, 1));
	TestEqual(TEXT("with the same hash"), GetTypeHash(A), GetTypeHash(FGSTN_EntityRef(GSTNEntityKinds::Fighter, 4, 1)));
	TestTrue(TEXT("a different id differs"), A != FGSTN_EntityRef(GSTNEntityKinds::Fighter, 5, 1));
	TestTrue(TEXT("a different serial differs: a reused id is another entity"), A != FGSTN_EntityRef(GSTNEntityKinds::Fighter, 4, 2));
	TestTrue(TEXT("a different kind differs"), A != FGSTN_EntityRef(GSTNEntityKinds::Minion, 4, 1));

	TSet<FGSTN_EntityRef> Set;
	Set.Add(A);
	Set.Add(FGSTN_EntityRef(GSTNEntityKinds::Fighter, 4, 1));
	Set.Add(Default);
	Set.Add(Other);
	TestEqual(TEXT("a set keeps one of each"), Set.Num(), 2);

	// The ids are fixed numbers: they would go on a wire
	TestEqual(TEXT("Fighter id"), static_cast<int32>(GSTNEntityKinds::Fighter), 0);
	TestEqual(TEXT("Defence feature id"), static_cast<int32>(GSTNFeatureIds::Defence), 17);
	TestEqual(TEXT("Link feature id"), static_cast<int32>(GSTNFeatureIds::Link), 18);
	TestEqual(TEXT("Hit inbox kind id"), static_cast<int32>(GSTNInboxKinds::Hit), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitDataDefaultsKeepM4ContentTest, "GSTNCombat.Core.HitData.DefaultsKeepM4Content", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitDataDefaultsKeepM4ContentTest::RunTest(const FString& Parameters)
{
	const FGSTN_HitData Hit;
	TestTrue(TEXT("a parry may stop a hit by default"), Hit.bParryable);
	TestFalse(TEXT("DI is on by default"), Hit.bDisableDI);
	TestTrue(TEXT("the link filter is Any"), Hit.LinkFilter == EGSTN_LinkFilter::Any);
	TestFalse(TEXT("no link type"), Hit.LinkType.IsValid());

	// A variant that overrides nothing returns the new fields untouched
	FGSTN_HitData Authored;
	Authored.Damage = 12.f;
	Authored.bParryable = false;
	Authored.bDisableDI = true;
	Authored.LinkFilter = EGSTN_LinkFilter::ExcludeLinkedPartner;
	const FGSTN_HitData Result = UGSTN_HitDataUtils::ApplyVariant(Authored, FGSTN_HitVariant());
	TestFalse(TEXT("bParryable passes through ApplyVariant"), Result.bParryable);
	TestTrue(TEXT("bDisableDI passes through"), Result.bDisableDI);
	TestTrue(TEXT("LinkFilter passes through"), Result.LinkFilter == EGSTN_LinkFilter::ExcludeLinkedPartner);
	TestEqual(TEXT("and the M4 fields are as before"), Result.Damage, 12.f);

	// The rules default keeps today's meaning of every hit: only Unblockable is out of reach of a block
	const UGSTN_CombatRules* Rules = GetDefault<UGSTN_CombatRules>();
	TestEqual(TEXT("one non-blockable type by default"), Rules->NonBlockableHitTypes.Num(), 1);
	TestTrue(TEXT("and it is Unblockable"), Rules->NonBlockableHitTypes.Contains(GSTNCombatHitTags::HitType_Unblockable.GetTag()));
	TestTrue(TEXT("a default hit is blockable"), Hit.IsBlockable(*Rules));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitDataBlockableDerivedFromHitTypeTest, "GSTNCombat.Core.HitData.BlockableDerivedFromHitType", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HitDataBlockableDerivedFromHitTypeTest::RunTest(const FString& Parameters)
{
	UGSTN_CombatRules* Rules = NewObject<UGSTN_CombatRules>(GetTransientPackage());

	FGSTN_HitData Hit;
	TestTrue(TEXT("an unset type is blockable"), Hit.IsBlockable(*Rules));
	Hit.HitType = GSTNCombatHitTags::HitType_Normal.GetTag();
	TestTrue(TEXT("Normal is blockable"), Hit.IsBlockable(*Rules));
	Hit.HitType = GSTNCombatHitTags::HitType_Projectile.GetTag();
	TestTrue(TEXT("a projectile is blockable"), Hit.IsBlockable(*Rules));
	Hit.HitType = GSTNCombatHitTags::HitType_Grab.GetTag();
	TestTrue(TEXT("a grab is blockable until the rules say otherwise"), Hit.IsBlockable(*Rules));
	Hit.HitType = GSTNCombatHitTags::HitType_Unblockable.GetTag();
	TestFalse(TEXT("Unblockable is not"), Hit.IsBlockable(*Rules));

	// The link plugin adds Grab by data: the same hit data now reads as unblockable
	Rules->NonBlockableHitTypes.Add(GSTNCombatHitTags::HitType_Grab.GetTag());
	Hit.HitType = GSTNCombatHitTags::HitType_Grab.GetTag();
	TestFalse(TEXT("a grab listed by the rules is not blockable"), Hit.IsBlockable(*Rules));

	// An unset type is a Normal hit, so listing Normal covers it too
	Rules->NonBlockableHitTypes.Add(GSTNCombatHitTags::HitType_Normal.GetTag());
	FGSTN_HitData Unset;
	TestFalse(TEXT("an unset type follows Normal"), Unset.IsBlockable(*Rules));

	Rules->NonBlockableHitTypes.Reset();
	Hit.HitType = GSTNCombatHitTags::HitType_Unblockable.GetTag();
	TestTrue(TEXT("with an empty list everything is blockable"), Hit.IsBlockable(*Rules));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
