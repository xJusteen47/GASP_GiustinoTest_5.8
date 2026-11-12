// Automation tests of the backpack loadout extension: the compile contribution (verbs and passives on the kit, nothing without a
// backpack, runs after the mods), the look on the implant's connector, and the rules view notes. Transient assets, no world.

#include "Misc/AutomationTest.h"
#include "Abilities/GameplayAbility.h"
#include "GSTN_BackpackDefinition.h"
#include "GSTN_BackpackLoadout.h"
#include "GSTN_BackpackTypes.h"
#include "Cosmetics/GSTN_EquipmentVisual.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Loadout/GSTN_LoadoutRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags BackpackGASTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	struct FBackpackFixture
	{
		UGSTN_BackpackDefinition* JetPack = nullptr;
		FGSTN_ResolvedLoadout Resolved;

		FBackpackFixture()
		{
			JetPack = NewObject<UGSTN_BackpackDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
			FGSTN_AbilityBinding& Burst = JetPack->Abilities.AddDefaulted_GetRef();
			Burst.Command = GSTNBackpackTags::Command_Backpack_Use;
			Burst.Ability = TSoftClassPtr<UGameplayAbility>(UGameplayAbility::StaticClass());
			JetPack->Passives.Add(TSoftClassPtr<UGameplayAbility>(UGameplayAbility::StaticClass()));
			FGSTN_AttachmentRequest& Pack = JetPack->Visual.Parts.AddDefaulted_GetRef();
			Pack.VisualSlot = TEXT("Backpack");
			FGSTN_AttachmentRequest& Nozzle = JetPack->Visual.Parts.AddDefaulted_GetRef();
			Nozzle.ParentSlot = TEXT("Backpack");
			Nozzle.Socket = TEXT("Nozzle");

			FSoftObjectPath Unused;
			TArray<FString> Errors;
			FGSTN_BackpackLoadoutRules::Equip(FGSTN_LoadoutData(), FSoftObjectPath(JetPack), Resolved.Loadout, Unused, Errors);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackCompileTest, "GSTNBackpack.GAS.Compile", BackpackGASTestFlags)
bool FGSTNBackpackCompileTest::RunTest(const FString& Parameters)
{
	FBackpackFixture Fixture;
	FGSTN_LoadoutExtensionContext Context;
	Context.Resolved = &Fixture.Resolved;
	FGSTN_CompileInput Input;
	FGSTN_BackpackLoadout::ContributeCompileInput(Context, Input);
	TestEqual(TEXT("the backpack's verb joins the kit's bindings"), Input.Kit.Bindings.Num(), 1);
	TestTrue(TEXT("on its own command"), Input.Kit.Bindings.Num() == 1 && Input.Kit.Bindings[0].Command == GSTNBackpackTags::Command_Backpack_Use);
	TestTrue(TEXT("its passive is granted with the kit"), Input.Kit.Passives.Contains(FSoftObjectPath(UGameplayAbility::StaticClass())));

	FGSTN_ResolvedLoadout NoBackpack;
	Context.Resolved = &NoBackpack;
	FGSTN_CompileInput Untouched;
	FGSTN_BackpackLoadout::ContributeCompileInput(Context, Untouched);
	TestEqual(TEXT("no backpack adds nothing"), Untouched.Kit.Bindings.Num() + Untouched.Kit.Passives.Num(), 0);

	TestTrue(TEXT("the extension is registered"), FGSTN_LoadoutExtensionRegistry::IsRegistered(FGSTN_BackpackLoadout::ExtensionId));
	TArray<FName> Order;
	FGSTN_LoadoutExtensionRegistry::GetOrderedIds(Order);
	const int32 Mods = Order.IndexOfByKey(FName(TEXT("GSTN.KitMods")));
	if (Mods != INDEX_NONE)
	{
		TestTrue(TEXT("it runs after the mods, so no mod reshapes a backpack verb"), Order.IndexOfByKey(FGSTN_BackpackLoadout::ExtensionId) > Mods);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackVisualTest, "GSTNBackpack.GAS.VisualOnConnector", BackpackGASTestFlags)
bool FGSTNBackpackVisualTest::RunTest(const FString& Parameters)
{
	FBackpackFixture Fixture;
	TestTrue(TEXT("the provider is registered"), FGSTN_AttachmentProviderRegistry::IsRegistered(FGSTN_BackpackLoadout::AttachmentProviderId));

	FGSTN_AttachmentContext Context;
	Context.Resolved = &Fixture.Resolved;
	TArray<FGSTN_AttachmentRequest> Requests;
	FGSTN_BackpackAttachmentProvider().GatherAttachments(Context, Requests);
	TestEqual(TEXT("one request per part"), Requests.Num(), 2);
	if (Requests.Num() == 2)
	{
		TestEqual(TEXT("a part without a place goes on the implant"), Requests[0].ParentSlot, GSTNEquipment::Slot_ImplantBack);
		TestEqual(TEXT("at the connector"), Requests[0].Socket, GSTNEquipment::Socket_ConnectorBackpack);
		TestEqual(TEXT("a placed part keeps its place"), Requests[1].Socket, FName(TEXT("Nozzle")));
	}

	// Composed after a kit implant: the backpack hangs on it. Without one it falls back to the pawn's mesh
	TArray<FGSTN_AttachmentRequest> WithImplant;
	FGSTN_AttachmentRequest& Implant = WithImplant.AddDefaulted_GetRef();
	Implant.Socket = GSTNEquipment::Socket_ImplantBack;
	Implant.VisualSlot = GSTNEquipment::Slot_ImplantBack;
	Implant.Mesh = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	for (FGSTN_AttachmentRequest& Request : Requests)
	{
		Request.Mesh = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	}
	WithImplant.Append(Requests);
	TArray<FGSTN_AttachmentRequest> Pieces;
	FGSTN_EquipmentVisualUtils::Compose(WithImplant, Pieces);
	TestTrue(TEXT("on a kit implant the backpack keeps the connector"), Pieces.Num() == 3 && Pieces[1].ParentSlot == GSTNEquipment::Slot_ImplantBack);
	FGSTN_EquipmentVisualUtils::Compose(Requests, Pieces);
	TestTrue(TEXT("with no implant it goes on the pawn"), Pieces.Num() == 2 && Pieces[0].ParentSlot.IsNone());

	FGSTN_ResolvedLoadout NoBackpack;
	Context.Resolved = &NoBackpack;
	TArray<FGSTN_AttachmentRequest> None;
	FGSTN_BackpackAttachmentProvider().GatherAttachments(Context, None);
	TestEqual(TEXT("no backpack shows nothing"), None.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBackpackRulesViewTest, "GSTNBackpack.GAS.RulesView", BackpackGASTestFlags)
bool FGSTNBackpackRulesViewTest::RunTest(const FString& Parameters)
{
	FBackpackFixture Fixture;
	Fixture.JetPack->CompatibleSupportWeapons = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNBackpackTags::Backpack));
	FGSTN_LoadoutExtensionContext Context;
	FGSTN_LoadoutRulesView View;
	View.Loadout = Fixture.Resolved.Loadout;
	View.Weapons.AddDefaulted();
	FGSTN_BackpackLoadout::ContributeRulesView(Context, View);
	TestEqual(TEXT("a feeding backpack with nothing to feed is a note"), View.Table.Warnings.Num(), 1);
	TestEqual(TEXT("never an error"), View.Table.Errors.Num(), 0);

	View.Table.Warnings.Reset();
	View.Weapons.AddDefaulted_GetRef().Tags.AddTag(GSTNBackpackTags::Backpack);
	FGSTN_BackpackLoadout::ContributeRulesView(Context, View);
	TestEqual(TEXT("with the support weapon it feeds there is nothing to say"), View.Table.Warnings.Num(), 0);

	View.Table.Warnings.Reset();
	FSoftObjectPath Unused;
	TArray<FString> Errors;
	// A loaded object that is not a backpack (no missing package, so no load warning in the log)
	FGSTN_BackpackLoadoutRules::Equip(FGSTN_LoadoutData(), FSoftObjectPath(UGameplayAbility::StaticClass()), View.Loadout, Unused, Errors);
	FGSTN_BackpackLoadout::ContributeRulesView(Context, View);
	TestEqual(TEXT("a path that is no backpack is a warning"), View.Table.Warnings.Num(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
