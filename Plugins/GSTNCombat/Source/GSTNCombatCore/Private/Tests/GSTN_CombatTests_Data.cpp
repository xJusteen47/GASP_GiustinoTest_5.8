#include "Misc/AutomationTest.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Kits/GSTN_CombatAssetTypes.h"
#include "Kits/GSTN_CommandSet.h"
#include "Kits/GSTN_DesignerIdentity.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Tests/GSTN_CombatTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_InputSlotEntry MakeSlot(const FGameplayTag& Command, int32 PriorityOffset = 0)
	{
		FGSTN_InputSlotEntry Entry;
		Entry.Inline.Command = Command;
		Entry.Inline.PriorityOffset = PriorityOffset;
		return Entry;
	}

	TArray<FGameplayTag> CommandsOf(const FGSTN_CommandSet& Set, const UGSTN_InputSlotSet* Base)
	{
		TArray<FGameplayTag> Commands;
		Set.GetCommands(Base, Commands);
		return Commands;
	}

	FGSTN_LoadoutValue MakeValue(float Value)
	{
		FGSTN_LoadoutValue Result;
		Result.Value = Value;
		return Result;
	}

	FGSTN_LoadoutData MakeLoadout()
	{
		FGSTN_LoadoutData Data;
		Data.PrimaryKit = FPrimaryAssetId(GSTNCombatAssetTypes::PrimaryKit(), TEXT("KitA"));
		Data.AdditionalWeapons = { FPrimaryAssetId(GSTNCombatAssetTypes::Weapon(), TEXT("WeaponA")), FPrimaryAssetId(GSTNCombatAssetTypes::Weapon(), TEXT("WeaponB")) };
		Data.Supers = { FPrimaryAssetId(GSTNCombatAssetTypes::Super(), TEXT("SuperA")), FPrimaryAssetId() };
		FGSTN_SlotCustomisation Customisation;
		Customisation.WeaponIndex = 1;
		Customisation.Command = GSTNCommandTags::Attack_Neutral.GetTag();
		Customisation.AlternativeAbility = TSoftClassPtr<UObject>(UObject::StaticClass());
		Data.SlotCustomisations.Add(Customisation);
		Data.DesignerValues.Add(GSTNCommandTags::Special_Neutral.GetTag(), MakeValue(2.5f));
		Data.DesignerValues.Add(GSTNCommandTags::Attack_Directional.GetTag(), MakeValue(-1.f));
		Data.Skin = FPrimaryAssetId(GSTNCombatAssetTypes::Skin(), TEXT("SkinA"));
		return Data;
	}

	TArray<uint8> WriteNet(FGSTN_LoadoutData& Data, int64& OutBits)
	{
		FBitWriter Writer(0, true);
		bool bSuccess = false;
		Data.NetSerialize(Writer, nullptr, bSuccess);
		OutBits = Writer.GetNumBits();
		return TArray<uint8>(Writer.GetData(), Writer.GetNumBytes());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CoreDataCommandSetTest, "GSTNCombat.Core.Data.CommandSetComposition", GSTN_CORE_TEST_FLAGS)
bool FGSTN_CoreDataCommandSetTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Neutral = GSTNCommandTags::Attack_Neutral.GetTag();
	const FGameplayTag Directional = GSTNCommandTags::Attack_Directional.GetTag();
	const FGameplayTag Special = GSTNCommandTags::Special_Neutral.GetTag();
	const FGameplayTag Combo = GSTNCommandTags::Combo_AttackSpecial_Neutral.GetTag();

	TStrongObjectPtr<UGSTN_InputSlotSet> Base(NewObject<UGSTN_InputSlotSet>(GetTransientPackage()));
	Base->Slots = { MakeSlot(Neutral), MakeSlot(Directional), MakeSlot(Special) };

	// Nothing removed or added: the base set
	FGSTN_CommandSet Set;
	TestTrue(TEXT("an untouched set is its base"), CommandsOf(Set, Base.Get()) == TArray<FGameplayTag>({ Neutral, Directional, Special }));

	// Base minus removed plus added, base order kept
	Set.RemovedCommands.AddTag(Directional);
	Set.AddedSlots = { MakeSlot(Combo) };
	TestTrue(TEXT("removed commands go, added ones follow the base"), CommandsOf(Set, Base.Get()) == TArray<FGameplayTag>({ Neutral, Special, Combo }));

	// An added slot with a command already present replaces it in its place
	Set.AddedSlots = { MakeSlot(Special, 9), MakeSlot(Combo) };
	TArray<const FGSTN_InputSlotSpec*> Specs;
	Set.Compose(Base.Get(), Specs);
	if (TestEqual(TEXT("a replaced slot adds no entry"), Specs.Num(), 3))
	{
		TestTrue(TEXT("the replaced slot keeps its place"), Specs[1]->Command == Special);
		TestEqual(TEXT("and carries the kit's version"), Specs[1]->PriorityOffset, 9);
		TestTrue(TEXT("the new slot is last"), Specs[2]->Command == Combo);
	}

	// Removal is exact: a root does not remove its children
	FGSTN_CommandSet RootRemoval;
	RootRemoval.RemovedCommands.AddTag(GSTNCommandTags::Attack);
	TestEqual(TEXT("removing a command root does not remove its leaves"), CommandsOf(RootRemoval, Base.Get()).Num(), 3);

	// A removed command that is not in the base changes nothing
	FGSTN_CommandSet Unrelated;
	Unrelated.RemovedCommands.AddTag(Combo);
	TestEqual(TEXT("removing an absent command is harmless"), CommandsOf(Unrelated, Base.Get()).Num(), 3);

	// No base: only what the kit adds; an invalid added command is kept out of the replace logic
	TestTrue(TEXT("without a base the set is the added slots"), CommandsOf(Set, nullptr) == TArray<FGameplayTag>({ Special, Combo }));

	// The composition does not change the base asset
	TestEqual(TEXT("the base set is untouched"), Base->Slots.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CoreDataLoadoutNetTest, "GSTNCombat.Core.Data.LoadoutNetSerialize", GSTN_CORE_TEST_FLAGS)
bool FGSTN_CoreDataLoadoutNetTest::RunTest(const FString& Parameters)
{
	FGSTN_LoadoutData Original = MakeLoadout();

	int64 Bits = 0;
	const TArray<uint8> Bytes = WriteNet(Original, Bits);
	AddInfo(FString::Printf(TEXT("a loadout with 3 weapon/super ids, 1 customisation and 2 values takes %lld bits (%d bytes) on the wire"), Bits, Bytes.Num()));

	FGSTN_LoadoutData Copy;
	{
		FBitReader Reader(Bytes.GetData(), Bits);
		bool bSuccess = false;
		Copy.NetSerialize(Reader, nullptr, bSuccess);
		TestTrue(TEXT("the loadout reads back"), bSuccess && !Reader.IsError());
		TestTrue(TEXT("every bit is consumed"), Reader.AtEnd());
	}
	TestTrue(TEXT("the copy equals the original"), Copy == Original);
	TestEqual(TEXT("the empty super slot survives"), Copy.Supers.Num(), 2);
	TestFalse(TEXT("an empty super slot stays empty"), Copy.Supers.Num() == 2 && Copy.Supers[1].IsValid());

	// The same loadout is the same bytes whatever the order its map was filled in
	FGSTN_LoadoutData Reordered = MakeLoadout();
	Reordered.DesignerValues.Empty();
	Reordered.DesignerValues.Add(GSTNCommandTags::Attack_Directional.GetTag(), MakeValue(-1.f));
	Reordered.DesignerValues.Add(GSTNCommandTags::Special_Neutral.GetTag(), MakeValue(2.5f));
	int64 ReorderedBits = 0;
	TestTrue(TEXT("map insertion order does not change the bytes"), WriteNet(Reordered, ReorderedBits) == Bytes && ReorderedBits == Bits);

	// An empty loadout is small and round-trips
	FGSTN_LoadoutData Empty;
	int64 EmptyBits = 0;
	const TArray<uint8> EmptyBytes = WriteNet(Empty, EmptyBits);
	FGSTN_LoadoutData EmptyCopy = MakeLoadout();
	{
		FBitReader Reader(EmptyBytes.GetData(), EmptyBits);
		bool bSuccess = false;
		EmptyCopy.NetSerialize(Reader, nullptr, bSuccess);
		TestTrue(TEXT("an empty loadout reads back"), bSuccess);
	}
	TestTrue(TEXT("reading replaces what the target held"), EmptyCopy == Empty);

	// A count above the bound is a malformed stream, never an allocation
	{
		FBitWriter Writer(0, true);
		FString NoKit;
		Writer << NoKit;
		uint32 Huge = 100000;
		Writer.SerializeIntPacked(Huge);
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_LoadoutData Hostile;
		bool bSuccess = true;
		Hostile.NetSerialize(Reader, nullptr, bSuccess);
		TestFalse(TEXT("an oversized list is rejected"), bSuccess);
		TestEqual(TEXT("and nothing is allocated for it"), Hostile.AdditionalWeapons.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CoreDataLoadoutWriteBoundsTest, "GSTNCombat.Core.Data.LoadoutNetWriteBounds", GSTN_CORE_TEST_FLAGS)
bool FGSTN_CoreDataLoadoutWriteBoundsTest::RunTest(const FString& Parameters)
{
	// Writing must not change the object, and a list over the bound must produce a stream the reader accepts
	FGSTN_LoadoutData Original = MakeLoadout();
	Original.SlotCustomisations[0].WeaponIndex = 1000;
	for (int32 Index = 0; Index < FGSTN_LoadoutData::MaxEntries + 10; ++Index)
	{
		Original.AdditionalWeapons.Add(FPrimaryAssetId(GSTNCombatAssetTypes::Weapon(), *FString::Printf(TEXT("W%d"), Index)));
	}
	const FGSTN_LoadoutData Before = Original;

	int64 Bits = 0;
	const TArray<uint8> Bytes = WriteNet(Original, Bits);
	TestTrue(TEXT("writing leaves the loadout as it was"), Original == Before);
	TestEqual(TEXT("including the out of range weapon index"), Original.SlotCustomisations[0].WeaponIndex, 1000);

	FGSTN_LoadoutData Copy;
	FBitReader Reader(Bytes.GetData(), Bits);
	bool bSuccess = false;
	Copy.NetSerialize(Reader, nullptr, bSuccess);
	TestTrue(TEXT("the reader accepts what the writer wrote"), bSuccess && !Reader.IsError());
	TestTrue(TEXT("every bit is consumed"), Reader.AtEnd());
	TestEqual(TEXT("the list was cut to the bound"), Copy.AdditionalWeapons.Num(), FGSTN_LoadoutData::MaxEntries);
	TestEqual(TEXT("the weapon index went out clamped"), Copy.SlotCustomisations.Num() == 1 ? Copy.SlotCustomisations[0].WeaponIndex : -1, FGSTN_LoadoutData::MaxEntries);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CoreDataLoadoutSaveTest, "GSTNCombat.Core.Data.LoadoutSaveSerialize", GSTN_CORE_TEST_FLAGS)
bool FGSTN_CoreDataLoadoutSaveTest::RunTest(const FString& Parameters)
{
	FGSTN_LoadoutData Original = MakeLoadout();

	TArray<uint8> Bytes;
	{
		FMemoryWriter Writer(Bytes);
		Original.Serialize(Writer);
		TestFalse(TEXT("writing raises no error"), Writer.IsError());
	}
	FGSTN_LoadoutData Copy;
	{
		FMemoryReader Reader(Bytes);
		Copy.Serialize(Reader);
		TestFalse(TEXT("reading raises no error"), Reader.IsError());
	}
	TestTrue(TEXT("the saved loadout reads back equal"), Copy == Original);

	// The same loadout saves to the same bytes
	FGSTN_LoadoutData Reordered = MakeLoadout();
	Reordered.DesignerValues.Empty();
	Reordered.DesignerValues.Add(GSTNCommandTags::Attack_Directional.GetTag(), MakeValue(-1.f));
	Reordered.DesignerValues.Add(GSTNCommandTags::Special_Neutral.GetTag(), MakeValue(2.5f));
	TArray<uint8> ReorderedBytes;
	{
		FMemoryWriter Writer(ReorderedBytes);
		Reordered.Serialize(Writer);
	}
	TestTrue(TEXT("the save bytes do not depend on map order"), ReorderedBytes == Bytes);

	// Tags that no longer exist are dropped, the rest of the loadout is kept
	{
		TArray<uint8> Stale;
		FMemoryWriter Writer(Stale);
		int32 Version = FGSTN_LoadoutData::SaveVersion;
		FString Kit = FPrimaryAssetId(GSTNCombatAssetTypes::PrimaryKit(), TEXT("KitA")).ToString();
		FString NoWeapon;
		FString GoneTag = TEXT("Gone.Tag.NotRegistered");
		FString NoClass;
		uint32 Zero = 0;
		uint32 One = 1;
		uint32 WeaponIndex = 0;
		float Number = 3.f;
		Writer << Version << Kit;
		Writer.SerializeIntPacked(Zero);
		Writer.SerializeIntPacked(Zero);
		Writer.SerializeIntPacked(One);
		Writer.SerializeIntPacked(WeaponIndex);
		Writer << GoneTag << NoClass;
		Writer.SerializeIntPacked(One);
		Writer << GoneTag << Number << NoWeapon;

		FGSTN_LoadoutData Loaded;
		FMemoryReader Reader(Stale);
		Loaded.Serialize(Reader);
		TestFalse(TEXT("a stale tag is not an error"), Reader.IsError());
		TestEqual(TEXT("the kit survives"), Loaded.PrimaryKit.PrimaryAssetName, FName(TEXT("KitA")));
		TestEqual(TEXT("a customisation of a removed command is dropped"), Loaded.SlotCustomisations.Num(), 0);
		TestEqual(TEXT("a value of a removed tag is dropped"), Loaded.DesignerValues.Num(), 0);
	}

	// A save from a newer build cannot be read: empty data and an error, not garbage
	{
		TArray<uint8> Future;
		FMemoryWriter Writer(Future);
		int32 Version = FGSTN_LoadoutData::SaveVersion + 1;
		Writer << Version;
		FGSTN_LoadoutData Loaded = MakeLoadout();
		FMemoryReader Reader(Future);
		Loaded.Serialize(Reader);
		TestTrue(TEXT("a newer version flags the archive"), Reader.IsError());
		TestTrue(TEXT("and leaves the data empty"), Loaded == FGSTN_LoadoutData());
	}

	// As a property: the struct serializer is the one the property system uses (a save game, a preset asset)
	{
		TArray<uint8> Property;
		FMemoryWriter Writer(Property);
		FGSTN_LoadoutData::StaticStruct()->SerializeItem(Writer, &Original, nullptr);
		FGSTN_LoadoutData Loaded;
		FMemoryReader Reader(Property);
		FGSTN_LoadoutData::StaticStruct()->SerializeItem(Reader, &Loaded, nullptr);
		TestTrue(TEXT("the struct reads back through the property system"), Loaded == Original);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CoreDataIdentityOneTest, "GSTNCombat.Core.Data.IdentityExactlyOne", GSTN_CORE_TEST_FLAGS)
bool FGSTN_CoreDataIdentityOneTest::RunTest(const FString& Parameters)
{
	// The command roots stand in for a designer root: the rule takes the root as a parameter and names no leaf
	const FGameplayTag Root = GSTNCommandTags::Attack.GetTag();
	const FGameplayTag LeafA = GSTNCommandTags::Attack_Neutral.GetTag();
	const FGameplayTag LeafB = GSTNCommandTags::Attack_Directional.GetTag();
	const FGameplayTag Other = GSTNCommandTags::Special_Neutral.GetTag();

	auto Check = [&Root](const FGameplayTagContainer& Tags)
	{
		TArray<FText> Errors;
		const bool bOk = FGSTN_DesignerIdentity::ValidateExactlyOne(Tags, Root, TEXT("thing"), Errors);
		return bOk == Errors.IsEmpty() ? (bOk ? 1 : 0) : -1;
	};

	TestEqual(TEXT("no tags: error"), Check(FGameplayTagContainer()), 0);
	TestEqual(TEXT("one leaf: valid"), Check(FGameplayTagContainer(LeafA)), 1);
	TestEqual(TEXT("two leaves: error"), Check(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>({ LeafA, LeafB }))), 0);
	TestEqual(TEXT("the bare root: error"), Check(FGameplayTagContainer(Root)), 0);
	TestEqual(TEXT("one leaf and a tag of another root: valid"), Check(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>({ LeafA, Other }))), 1);
	TestEqual(TEXT("only another root: error"), Check(FGameplayTagContainer(Other)), 0);

	FGameplayTag Found;
	TestTrue(TEXT("the blueprint reader finds the single leaf"), UGSTN_DesignerIdentityUtils::FindIdentityTag(FGameplayTagContainer(LeafA), Root, Found) && Found == LeafA);
	TestFalse(TEXT("and none when there are two"), UGSTN_DesignerIdentityUtils::FindIdentityTag(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>({ LeafA, LeafB })), Root, Found));

	TArray<FText> Errors;
	TestFalse(TEXT("an invalid root is an error, not a crash"), FGSTN_DesignerIdentity::ValidateExactlyOne(FGameplayTagContainer(LeafA), FGameplayTag(), TEXT("thing"), Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CoreDataIdentityUniqueTest, "GSTNCombat.Core.Data.IdentityUniquePerType", GSTN_CORE_TEST_FLAGS)
bool FGSTN_CoreDataIdentityUniqueTest::RunTest(const FString& Parameters)
{
	TArray<FText> Errors;
	const TArray<FGSTN_UniqueKeyEntry> Distinct = { { TEXT("KitA"), TEXT("Id.A") }, { TEXT("KitB"), TEXT("Id.B") }, { TEXT("KitC"), NAME_None } };
	TestTrue(TEXT("distinct identities are unique"), FGSTN_DesignerIdentity::FindDuplicateKeys(Distinct, TEXT("Identity"), Errors));
	TestEqual(TEXT("and report nothing"), Errors.Num(), 0);

	const TArray<FGSTN_UniqueKeyEntry> Shared = { { TEXT("KitA"), TEXT("Id.A") }, { TEXT("KitB"), TEXT("Id.B") }, { TEXT("KitC"), TEXT("Id.A") },
		{ TEXT("KitD"), NAME_None }, { TEXT("KitE"), NAME_None }, { TEXT("KitF"), TEXT("Id.A") } };
	TestFalse(TEXT("a shared identity is not unique"), FGSTN_DesignerIdentity::FindDuplicateKeys(Shared, TEXT("Identity"), Errors));
	if (TestEqual(TEXT("one error per shared identity"), Errors.Num(), 1))
	{
		const FString Message = Errors[0].ToString();
		TestTrue(TEXT("it names every asset that shares it"), Message.Contains(TEXT("KitA")) && Message.Contains(TEXT("KitC")) && Message.Contains(TEXT("KitF")));
		TestFalse(TEXT("and no other"), Message.Contains(TEXT("KitB")));
	}

	// Assets without an identity are not duplicates of each other (their own rule reports them)
	Errors.Reset();
	TestTrue(TEXT("two assets with no identity do not collide"), FGSTN_DesignerIdentity::FindDuplicateKeys({ { TEXT("KitD"), NAME_None }, { TEXT("KitE"), NAME_None } }, TEXT("Identity"), Errors));

	// Two shared identities are two errors, in first-seen order
	Errors.Reset();
	FGSTN_DesignerIdentity::FindDuplicateKeys({ { TEXT("A"), TEXT("X") }, { TEXT("B"), TEXT("Y") }, { TEXT("C"), TEXT("X") }, { TEXT("D"), TEXT("Y") } }, TEXT("Identity"), Errors);
	if (TestEqual(TEXT("two shared identities, two errors"), Errors.Num(), 2))
	{
		TestTrue(TEXT("first seen first"), Errors[0].ToString().Contains(TEXT("'X'")) && Errors[1].ToString().Contains(TEXT("'Y'")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
