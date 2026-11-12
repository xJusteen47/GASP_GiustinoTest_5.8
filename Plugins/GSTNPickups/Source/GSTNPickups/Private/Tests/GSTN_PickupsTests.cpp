// Equipment pickups, tests (M11 L2): GSTNPickups.Payload.*, GSTNPickups.Race, GSTNPickups.Take.*

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GSTN_CombatTags.h"
#include "Input/GSTN_InputSlot.h"
#include "Input/GSTN_StandardSlots.h"
#include "Kits/GSTN_CombatAssetTypes.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Pickups/GSTN_EquipmentPickup.h"
#include "Pickups/GSTN_KitPickup.h"
#include "Pickups/GSTN_KitSwapSubsystem.h"
#include "Pickups/GSTN_PickupPayload.h"
#include "Tests/GSTN_PickupsTestTypes.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags PickupsTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FPrimaryAssetId KitId(const TCHAR* Name) { return FPrimaryAssetId(GSTNCombatAssetTypes::PrimaryKit(), FName(Name)); }
	FPrimaryAssetId WeaponId(const TCHAR* Name) { return FPrimaryAssetId(GSTNCombatAssetTypes::Weapon(), FName(Name)); }

	FGSTN_LoadoutData Loadout(const TCHAR* Kit, std::initializer_list<const TCHAR*> Weapons = {})
	{
		FGSTN_LoadoutData Data;
		Data.PrimaryKit = KitId(Kit);
		for (const TCHAR* Weapon : Weapons)
		{
			Data.AdditionalWeapons.Add(WeaponId(Weapon));
		}
		return Data;
	}

	/** A game world, one player actor with a loadout component, two kits (A carries one support weapon, B none) and two support weapons */
	struct FWorldFixture
	{
		UWorld* World = nullptr;
		AActor* Player = nullptr;
		TStrongObjectPtr<UGSTN_InputSlotSet> Base;
		TStrongObjectPtr<UGSTN_PrimaryKitDefinition> KitA;
		TStrongObjectPtr<UGSTN_PrimaryKitDefinition> KitB;
		TStrongObjectPtr<UGSTN_WeaponDefinition> Support1;
		TStrongObjectPtr<UGSTN_WeaponDefinition> Support2;
		UGSTN_PickupsTestLoadoutComponent* Component = nullptr;

		FWorldFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNPickupsTestWorld"));
			World->AddToRoot();
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Player = World->SpawnActor<AActor>();

			Base.Reset(NewObject<UGSTN_InputSlotSet>(GetTransientPackage()));
			for (const FGSTN_InputSlotSpec& Spec : UGSTN_StandardSlots::Build())
			{
				Base->Slots.AddDefaulted_GetRef().Inline = Spec;
			}
			const FGameplayTagQuery SupportOnly = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNCombatTags::Weapon_Role_Support.GetTag()));
			KitA.Reset(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
			KitA->CommandSet.BaseSet = TSoftObjectPtr<UGSTN_InputSlotSet>(Base.Get());
			KitA->AllowedAdditionalWeapons = SupportOnly;
			KitA->MaxAdditionalWeapons = 1;
			KitB.Reset(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
			KitB->CommandSet.BaseSet = TSoftObjectPtr<UGSTN_InputSlotSet>(Base.Get());
			KitB->MaxAdditionalWeapons = 0;
			for (TStrongObjectPtr<UGSTN_WeaponDefinition>* Weapon : { &Support1, &Support2 })
			{
				Weapon->Reset(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
				(*Weapon)->WeaponTags.AddTag(GSTNCombatTags::Weapon_Role_Support.GetTag());
			}

			Component = NewObject<UGSTN_PickupsTestLoadoutComponent>(Player);
			Component->RegisterComponent();
			Component->Assets.Add(KitId(TEXT("KitA")), KitA.Get());
			Component->Assets.Add(KitId(TEXT("KitB")), KitB.Get());
			Component->Assets.Add(WeaponId(TEXT("S1")), Support1.Get());
			Component->Assets.Add(WeaponId(TEXT("S2")), Support2.Get());
		}

		~FWorldFixture()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		template <typename T>
		T* Spawn(const FVector& Offset) const
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<T>(T::StaticClass(), FTransform(Player->GetActorLocation() + Offset), Params);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PickupsPayloadKitTest, "GSTNPickups.Payload.Kit", PickupsTestFlags)
bool FGSTN_PickupsPayloadKitTest::RunTest(const FString& Parameters)
{
	TArray<FString> Errors;
	FGSTN_LoadoutData New;
	FInstancedStruct Left;
	TestTrue(TEXT("an empty kit payload is empty"), FGSTN_KitPickupPayload().IsEmpty());
	TestFalse(TEXT("and cannot be taken"), FGSTN_KitPickupPayload().MakeChange(Loadout(TEXT("KitA")), New, Left, Errors));

	const FGSTN_KitPickupPayload Kit(Loadout(TEXT("KitB")));
	TestEqual(TEXT("the label shows the kit"), Kit.GetDisplayId(), KitId(TEXT("KitB")));
	Errors.Reset();
	TestTrue(TEXT("a kit is taken"), Kit.MakeChange(Loadout(TEXT("KitA"), { TEXT("S1") }), New, Left, Errors));
	TestTrue(TEXT("the taker plays the whole loadout on the ground"), New == Loadout(TEXT("KitB")));
	const FGSTN_KitPickupPayload* LeftKit = Left.GetPtr<FGSTN_KitPickupPayload>();
	TestTrue(TEXT("their whole previous loadout stays on the ground"), LeftKit && LeftKit->Loadout == Loadout(TEXT("KitA"), { TEXT("S1") }));

	TestTrue(TEXT("a player with no kit takes it too"), Kit.MakeChange(FGSTN_LoadoutData(), New, Left, Errors));
	LeftKit = Left.GetPtr<FGSTN_KitPickupPayload>();
	TestTrue(TEXT("and leaves an empty pickup"), LeftKit && LeftKit->IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PickupsPayloadSupportTest, "GSTNPickups.Payload.SupportWeapon", PickupsTestFlags)
bool FGSTN_PickupsPayloadSupportTest::RunTest(const FString& Parameters)
{
	TArray<FString> Errors;
	FGSTN_LoadoutData New;
	FInstancedStruct Left;
	const FGSTN_SupportWeaponPickupPayload Support(WeaponId(TEXT("S2")));
	TestEqual(TEXT("the label shows the weapon"), Support.GetDisplayId(), WeaponId(TEXT("S2")));

	TestTrue(TEXT("a kit with no support weapon takes it"), Support.MakeChange(Loadout(TEXT("KitA")), New, Left, Errors));
	TestTrue(TEXT("it is added and the kit stays"), New == Loadout(TEXT("KitA"), { TEXT("S2") }));
	TestFalse(TEXT("nothing stays on the ground"), Left.IsValid());

	FGSTN_LoadoutData Carrying = Loadout(TEXT("KitA"), { TEXT("S1") });
	FGSTN_SlotCustomisation OnSupport;
	OnSupport.WeaponIndex = 1;
	Carrying.SlotCustomisations.Add(OnSupport);
	FGSTN_SlotCustomisation OnPrimary;
	OnPrimary.WeaponIndex = 0;
	Carrying.SlotCustomisations.Add(OnPrimary);
	TestTrue(TEXT("one at a time: a carried support weapon is swapped"), Support.MakeChange(Carrying, New, Left, Errors));
	TestTrue(TEXT("the new one takes its place"), New.AdditionalWeapons == TArray<FPrimaryAssetId>({ WeaponId(TEXT("S2")) }));
	const FGSTN_SupportWeaponPickupPayload* LeftWeapon = Left.GetPtr<FGSTN_SupportWeaponPickupPayload>();
	TestTrue(TEXT("the old one stays on the ground"), LeftWeapon && LeftWeapon->Weapon == WeaponId(TEXT("S1")));
	TestEqual(TEXT("the old weapon's customisation is dropped, the primary's kept"), New.SlotCustomisations.Num(), 1);
	TestEqual(TEXT("the kept one is the primary's"), New.SlotCustomisations[0].WeaponIndex, 0);

	Errors.Reset();
	TestFalse(TEXT("the same weapon twice is refused"), Support.MakeChange(Loadout(TEXT("KitA"), { TEXT("S2") }), New, Left, Errors));
	TestFalse(TEXT("a player with no kit is refused"), Support.MakeChange(FGSTN_LoadoutData(), New, Left, Errors));
	TestEqual(TEXT("each with a reason"), Errors.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PickupsRaceTest, "GSTNPickups.Race", PickupsTestFlags)
bool FGSTN_PickupsRaceTest::RunTest(const FString& Parameters)
{
	// Queue order is arrival order on the host; the rule ignores it
	TArray<FGSTN_PickupClaim> Claims = {
		{ 12, 1, 0 },          // later frame, lowest id
		{ 10, 7, 1 },
		{ 10, INDEX_NONE, 2 }, // no player state: after every real id on the same frame
		{ 10, 3, 3 },
	};
	FGSTN_PickupClaim::SortForRace(Claims);
	TArray<int32> Order;
	for (const FGSTN_PickupClaim& Claim : Claims)
	{
		Order.Add(Claim.Index);
	}
	TestTrue(TEXT("earliest frame first, then lower player id, then the claim with no id, then the later frame"), Order == TArray<int32>({ 3, 1, 2, 0 }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PickupsTakeKitTest, "GSTNPickups.Take.Kit", PickupsTestFlags)
bool FGSTN_PickupsTakeKitTest::RunTest(const FString& Parameters)
{
	// TH-1b, now through the equipment pickup: a kit on the ground swaps with the player's and the old kit stays there
	FWorldFixture Fixture;
	UGSTN_PickupsTestLoadoutComponent& Component = *Fixture.Component;
	UGSTN_KitSwapSubsystem* Swap = Fixture.World->GetSubsystem<UGSTN_KitSwapSubsystem>();
	if (!TestNotNull(TEXT("the game world has the kit swap subsystem"), Swap))
	{
		return false;
	}
	TArray<FString> Errors;
	TestTrue(FString::Printf(TEXT("the player starts with kit A (%s)"), *FString::Join(Errors, TEXT(" | "))), Component.AcceptLoadout(Loadout(TEXT("KitA")), Errors));

	AGSTN_KitPickup* Near = Fixture.Spawn<AGSTN_KitPickup>(FVector(100.f, 0.f, 0.f));
	AGSTN_KitPickup* Far = Fixture.Spawn<AGSTN_KitPickup>(FVector(1000.f, 0.f, 0.f));
	if (!TestTrue(TEXT("two pickups spawned"), Near && Far))
	{
		return false;
	}
	TestTrue(TEXT("a kit pickup starts empty"), Near->IsEmpty());
	TestNotNull(TEXT("with a kit payload ready for the details panel"), Near->GetPayload().GetPtr<FGSTN_KitPickupPayload>());
	TestNull(TEXT("an empty pickup in reach is not offered"), Swap->FindPickupFor(Fixture.Player));
	TestFalse(TEXT("and a swap with it is refused"), UGSTN_KitSwapSubsystem::SwapWithPickup(Component, *Near, Errors));

	Near->SetLoadout(Loadout(TEXT("KitB")));
	Far->SetLoadout(Loadout(TEXT("KitB")));
	TestTrue(TEXT("the pickup in reach is found, not the far one"), Swap->FindPickupFor(Fixture.Player) == Near);
	Near->InteractRadius = 50.f;
	TestNull(TEXT("out of its radius it is not"), Swap->FindPickupFor(Fixture.Player));
	Near->InteractRadius = 150.f;

	TestTrue(TEXT("the swap is accepted"), UGSTN_KitSwapSubsystem::SwapWithPickup(Component, *Near, Errors));
	TestTrue(TEXT("the player now plays kit B"), Component.GetApplied() == Loadout(TEXT("KitB")));
	TestTrue(TEXT("kit A lies on the ground in its place"), Near->GetLoadout() == Loadout(TEXT("KitA")));
	TestEqual(TEXT("one loadout revision per swap"), Component.GetRevision(), 2);

	// A kit the rules refuse changes nothing on either side
	Near->SetLoadout(Loadout(TEXT("Missing")));
	TestFalse(TEXT("an unknown kit is refused"), UGSTN_KitSwapSubsystem::SwapWithPickup(Component, *Near, Errors));
	TestTrue(TEXT("with a finding"), Errors.Num() > 0);
	TestTrue(TEXT("the player keeps kit B"), Component.GetApplied() == Loadout(TEXT("KitB")));
	TestTrue(TEXT("and the pickup keeps its kit"), Near->GetLoadout() == Loadout(TEXT("Missing")));

	// The death drop hook (M12): a copy of the player's kit appears on the ground
	AGSTN_KitPickup* Dropped = Swap->DropKit(&Component, FTransform(Fixture.Player->GetActorLocation() + FVector(0.f, 300.f, 0.f)), nullptr);
	TestTrue(TEXT("the drop hook spawns a pickup with the player's kit"), Dropped && Dropped->GetLoadout() == Component.GetApplied());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PickupsTakeSupportTest, "GSTNPickups.Take.SupportWeapon", PickupsTestFlags)
bool FGSTN_PickupsTakeSupportTest::RunTest(const FString& Parameters)
{
	FWorldFixture Fixture;
	UGSTN_PickupsTestLoadoutComponent& Component = *Fixture.Component;
	UGSTN_KitSwapSubsystem* Swap = Fixture.World->GetSubsystem<UGSTN_KitSwapSubsystem>();
	TArray<FString> Errors;
	TestTrue(TEXT("the player starts with kit A and support S1"), Component.AcceptLoadout(Loadout(TEXT("KitA"), { TEXT("S1") }), Errors));

	AGSTN_EquipmentPickup* Pickup = Fixture.Spawn<AGSTN_EquipmentPickup>(FVector(100.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("an equipment pickup spawned"), Pickup))
	{
		return false;
	}
	TestTrue(TEXT("with no payload it is empty"), Pickup->IsEmpty());
	Pickup->SetPayload(FInstancedStruct::Make(FGSTN_SupportWeaponPickupPayload(WeaponId(TEXT("S2")))));
	TestEqual(TEXT("it shows the weapon"), Pickup->GetDisplayId(), WeaponId(TEXT("S2")));
	TestTrue(TEXT("a support weapon is found like a kit"), Swap && Swap->FindPickupFor(Fixture.Player) == Pickup);

	TestTrue(FString::Printf(TEXT("taking it is accepted (%s)"), *FString::Join(Errors, TEXT(" | "))), Pickup->Take(Component, Errors));
	TestTrue(TEXT("the player keeps kit A with S2"), Component.GetApplied() == Loadout(TEXT("KitA"), { TEXT("S2") }));
	TestEqual(TEXT("S1 lies on the ground"), Pickup->GetDisplayId(), WeaponId(TEXT("S1")));

	// Kit B carries no additional weapon: the rules refuse, nothing changes
	TestTrue(TEXT("switch to kit B"), Component.AcceptLoadout(Loadout(TEXT("KitB")), Errors));
	TestFalse(TEXT("kit B may not carry it"), Pickup->Take(Component, Errors));
	TestTrue(TEXT("with the rules' reason"), Errors.Num() > 0);
	TestTrue(TEXT("the player keeps kit B as it was"), Component.GetApplied() == Loadout(TEXT("KitB")));
	TestEqual(TEXT("and S1 stays on the ground"), Pickup->GetDisplayId(), WeaponId(TEXT("S1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PickupsPayloadKitCarryOverTest, "GSTNPickups.Payload.KitCarryOver", PickupsTestFlags)
bool FGSTN_PickupsPayloadKitCarryOverTest::RunTest(const FString& Parameters)
{
	auto Kept = [](int32 Value) { FGSTN_PickupsTestKeptExt Ext; Ext.Value = Value; return FInstancedStruct::Make(Ext); };
	auto Plain = [](int32 Value) { FGSTN_PickupsTestPlainExt Ext; Ext.Value = Value; return FInstancedStruct::Make(Ext); };
	TArray<FString> Errors;
	FGSTN_LoadoutData New;
	FInstancedStruct Left;

	// The player carries a kept entry (a backpack) and a plain one (mods); the kit on the ground carries nothing
	FGSTN_LoadoutData Current = Loadout(TEXT("KitA"));
	Current.Extensions = { Kept(1), Plain(2) };
	TestTrue(TEXT("the kit is taken"), FGSTN_KitPickupPayload(Loadout(TEXT("KitB"))).MakeChange(Current, New, Left, Errors));
	const FGSTN_PickupsTestKeptExt* NewKept = GSTNLoadoutExtension::Find<FGSTN_PickupsTestKeptExt>(New.Extensions);
	TestTrue(TEXT("the kept entry goes with the player"), NewKept && NewKept->Value == 1);
	TestNull(TEXT("the plain one does not"), GSTNLoadoutExtension::Find<FGSTN_PickupsTestPlainExt>(New.Extensions));
	const FGSTN_KitPickupPayload* LeftKit = Left.GetPtr<FGSTN_KitPickupPayload>();
	TestTrue(TEXT("the plain one stays with the kit on the ground"), LeftKit && GSTNLoadoutExtension::Find<FGSTN_PickupsTestPlainExt>(LeftKit->Loadout.Extensions));
	TestTrue(TEXT("the kept one is not left behind"), LeftKit && !GSTNLoadoutExtension::Find<FGSTN_PickupsTestKeptExt>(LeftKit->Loadout.Extensions));

	// The kit on the ground brought its own kept entry: the two trade places, nothing is lost
	FGSTN_LoadoutData Ground = Loadout(TEXT("KitB"));
	Ground.Extensions = { Kept(3) };
	TestTrue(TEXT("the kit is taken again"), FGSTN_KitPickupPayload(Ground).MakeChange(Current, New, Left, Errors));
	NewKept = GSTNLoadoutExtension::Find<FGSTN_PickupsTestKeptExt>(New.Extensions);
	TestTrue(TEXT("the player keeps their own"), NewKept && NewKept->Value == 1);
	LeftKit = Left.GetPtr<FGSTN_KitPickupPayload>();
	const FGSTN_PickupsTestKeptExt* LeftKept = LeftKit ? GSTNLoadoutExtension::Find<FGSTN_PickupsTestKeptExt>(LeftKit->Loadout.Extensions) : nullptr;
	TestTrue(TEXT("the ground's one stays on the ground"), LeftKept && LeftKept->Value == 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
