// Automation tests of the backpack pickup payload: take, swap one at a time, refuse the same one, keep it through a kit swap.

#include "Misc/AutomationTest.h"
#include "GSTN_BackpackPickupPayload.h"
#include "GSTN_BackpackTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackPickupPayloadTest, "GSTNBackpack.Pickups.Payload", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTNBackpackPickupPayloadTest::RunTest(const FString& Parameters)
{
	const FSoftObjectPath JetPack(TEXT("/Game/Test/BP_JetPack.BP_JetPack"));
	const FSoftObjectPath Supply(TEXT("/Game/Test/BP_Supply.BP_Supply"));
	TArray<FString> Errors;
	FGSTN_LoadoutData New;
	FInstancedStruct Left;

	TestTrue(TEXT("an empty payload is empty"), FGSTN_BackpackPickupPayload().IsEmpty());
	TestFalse(TEXT("and cannot be taken"), FGSTN_BackpackPickupPayload().MakeChange(FGSTN_LoadoutData(), New, Left, Errors));

	const FGSTN_BackpackPickupPayload Jet(JetPack);
	TestEqual(TEXT("the label names the backpack"), Jet.GetDisplayId().PrimaryAssetName, FName(TEXT("BP_JetPack")));
	TestTrue(TEXT("a player without one takes it"), Jet.MakeChange(FGSTN_LoadoutData(), New, Left, Errors));
	TestEqual(TEXT("and carries it"), FGSTN_BackpackLoadoutRules::GetBackpack(New), JetPack);
	TestFalse(TEXT("the pickup empties"), Left.IsValid());

	const FGSTN_LoadoutData Carrying = New;
	TestTrue(TEXT("a player with another one swaps"), FGSTN_BackpackPickupPayload(Supply).MakeChange(Carrying, New, Left, Errors));
	const FGSTN_BackpackPickupPayload* Dropped = Left.GetPtr<FGSTN_BackpackPickupPayload>();
	TestTrue(TEXT("their old one lies here"), Dropped && Dropped->Backpack == JetPack);
	TestFalse(TEXT("the same one twice is refused"), Jet.MakeChange(Carrying, New, Left, Errors));

	// Kit swap: the backpack stays with the player (spec §10), through the generic carry-over of the kit payload
	FGSTN_LoadoutData Ground;
	Ground.PrimaryKit = FPrimaryAssetId(TEXT("GSTN_PrimaryKit"), TEXT("KitB"));
	TestTrue(TEXT("a kit is taken"), FGSTN_KitPickupPayload(Ground).MakeChange(Carrying, New, Left, Errors));
	TestEqual(TEXT("the backpack came along"), FGSTN_BackpackLoadoutRules::GetBackpack(New), JetPack);
	const FGSTN_KitPickupPayload* LeftKit = Left.GetPtr<FGSTN_KitPickupPayload>();
	TestTrue(TEXT("and is not left with the old kit"), LeftKit && !FGSTN_BackpackLoadoutRules::GetBackpack(LeftKit->Loadout).IsValid());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
