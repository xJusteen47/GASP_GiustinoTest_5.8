// Automation tests of the backpack's pure layer: equip and swap (one at a time), removal, the wire round trip of the loadout
// extension and its kit-swap carry-over flag. No world.

#include "Misc/AutomationTest.h"
#include "GSTN_BackpackTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags BackpackCoreTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FSoftObjectPath JetPackPath() { return FSoftObjectPath(TEXT("/Game/Test/BP_JetPack.BP_JetPack")); }
	FSoftObjectPath SupplyPath() { return FSoftObjectPath(TEXT("/Game/Test/BP_Supply.BP_Supply")); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackEquipTest, "GSTNBackpack.Core.Equip", BackpackCoreTestFlags)
bool FGSTNBackpackEquipTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath JetPack = JetPackPath();
	const FSoftObjectPath Supply = SupplyPath();
	FGSTN_LoadoutData Empty;
	FGSTN_LoadoutData WithJet;
	FSoftObjectPath Previous;
	TArray<FString> Errors;
	TestTrue(TEXT("a backpack goes on"), FGSTN_BackpackLoadoutRules::Equip(Empty, JetPack, WithJet, Previous, Errors));
	TestEqual(TEXT("the loadout carries it"), FGSTN_BackpackLoadoutRules::GetBackpack(WithJet), JetPack);
	TestFalse(TEXT("nothing was replaced"), Previous.IsValid());

	FGSTN_LoadoutData WithSupply;
	TestTrue(TEXT("another one swaps it"), FGSTN_BackpackLoadoutRules::Equip(WithJet, Supply, WithSupply, Previous, Errors));
	TestEqual(TEXT("one at a time"), WithSupply.Extensions.Num(), 1);
	TestEqual(TEXT("the new one is carried"), FGSTN_BackpackLoadoutRules::GetBackpack(WithSupply), Supply);
	TestEqual(TEXT("the old one is handed back to drop"), Previous, JetPack);

	Errors.Reset();
	FGSTN_LoadoutData Unused;
	TestFalse(TEXT("the same one twice is refused"), FGSTN_BackpackLoadoutRules::Equip(WithSupply, Supply, Unused, Previous, Errors));
	TestFalse(TEXT("an empty one is refused"), FGSTN_BackpackLoadoutRules::Equip(WithSupply, FSoftObjectPath(), Unused, Previous, Errors));
	TestEqual(TEXT("each with a reason"), Errors.Num(), 2);

	FGSTN_LoadoutData Removed;
	FSoftObjectPath Dropped;
	FGSTN_BackpackLoadoutRules::Remove(WithSupply, Removed, Dropped);
	TestEqual(TEXT("removal hands back what was carried"), Dropped, Supply);
	TestFalse(TEXT("and leaves no backpack"), FGSTN_BackpackLoadoutRules::GetBackpack(Removed).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackWireTest, "GSTNBackpack.Core.WireRoundTrip", BackpackCoreTestFlags)
bool FGSTNBackpackWireTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath JetPack = JetPackPath();
	TestEqual(TEXT("the wire id is registered"), FGSTN_LoadoutExtensionTypes::GetWireId(FGSTN_LoadoutBackpackExt::StaticStruct()), FGSTN_LoadoutBackpackExt::WireId);

	FGSTN_LoadoutBackpackExt Ext;
	Ext.Backpack = JetPack;
	Ext.bKeepOnKitSwap = false;
	TArray<FInstancedStruct> Extensions = { FInstancedStruct::Make(Ext) };
	TArray<uint8> Bytes;
	GSTNLoadoutExtension::WritePayload(Extensions, Bytes, 4096);
	TArray<FInstancedStruct> Read;
	TestTrue(TEXT("the payload reads back"), GSTNLoadoutExtension::ReadPayload(Bytes, Read));
	const FGSTN_LoadoutBackpackExt* Back = GSTNLoadoutExtension::Find<FGSTN_LoadoutBackpackExt>(Read);
	TestTrue(TEXT("with the backpack"), Back && Back->Backpack == JetPack);
	TestTrue(TEXT("and the carry-over choice"), Back && !Back->bKeepOnKitSwap);

	TestFalse(TEXT("a backpack set to travel with the kit does not stay"), Ext.StaysWithPlayerOnKitSwap());
	Ext.bKeepOnKitSwap = true;
	TestTrue(TEXT("by default it stays with the player"), Ext.StaysWithPlayerOnKitSwap());
	Ext.Backpack.Reset();
	TestFalse(TEXT("an empty entry carries nothing over"), Ext.StaysWithPlayerOnKitSwap());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
