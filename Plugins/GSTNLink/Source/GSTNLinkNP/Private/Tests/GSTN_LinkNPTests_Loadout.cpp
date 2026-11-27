// Links, Network Prediction half: automation tests of loadout extensions registered by GSTNLinkNP.

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_LinkTags.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutExtensions.h"

// One namespace per test file: the unity build merges the files of this module, so their helpers must not share a scope
namespace GSTNLinkNPTests_Loadout
{

using namespace GSTNLinkNPTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkNPSpinSuppressionLoadoutTest, "GSTNLink.NP.Loadout.SpinSuppression", Flags)
bool FGSTN_LinkNPSpinSuppressionLoadoutTest::RunTest(const FString& Parameters)
{
	const FGameplayTag MovementSpinRoot = FGameplayTag::RequestGameplayTag(TEXT("Command.Movement.Spin"), false);
	const FGameplayTag SpinClockwise = FGameplayTag::RequestGameplayTag(TEXT("Command.Movement.Spin.Clockwise"), false);
	const FGameplayTag SpinCounterClockwise = FGameplayTag::RequestGameplayTag(TEXT("Command.Movement.Spin.CounterClockwise"), false);
	const FGameplayTag SpinButton = FGameplayTag::RequestGameplayTag(TEXT("Command.Movement.Spin.Button"), false);
	if (!MovementSpinRoot.IsValid() || !SpinClockwise.IsValid() || !SpinCounterClockwise.IsValid() || !SpinButton.IsValid())
	{
		AddWarning(TEXT("GSTNSpinDrill Command.Movement.Spin.* tags not registered: skipped"));
		return true;
	}

	const FName SpinSuppressionId(TEXT("GSTNLinkSpinSuppression"));
	const FName KitThrowsId(TEXT("GSTNLinkThrows"));
	const FName SpinDrillId(TEXT("GSTNSpinDrill"));
	TestTrue(TEXT("GSTNLinkSpinSuppression is registered"), FGSTN_LoadoutExtensionRegistry::IsRegistered(SpinSuppressionId));

	TArray<FName> OrderedIds;
	FGSTN_LoadoutExtensionRegistry::GetOrderedIds(OrderedIds);
	const int32 SuppressionOrder = OrderedIds.Find(SpinSuppressionId);
	const int32 KitThrowsOrder = OrderedIds.Find(KitThrowsId);
	TestTrue(TEXT("GSTNLinkSpinSuppression runs strictly after GSTNLinkThrows"), SuppressionOrder != INDEX_NONE && KitThrowsOrder != INDEX_NONE && SuppressionOrder > KitThrowsOrder);
	const int32 SpinDrillOrder = OrderedIds.Find(SpinDrillId);
	if (SpinDrillOrder != INDEX_NONE)
	{
		TestTrue(TEXT("GSTNLinkSpinSuppression runs strictly after GSTNSpinDrill"), SuppressionOrder > SpinDrillOrder);
	}

	FGSTN_CompileInput Input;
	// Manual kit bindings: spin commands + non-spin grab command + a spin binding that explicitly requires Role_Grabber
	FGSTN_BindingInput& KitCW = Input.Kit.Bindings.AddDefaulted_GetRef();
	KitCW.Command = SpinClockwise;

	FGSTN_BindingInput& KitCCW = Input.Kit.Bindings.AddDefaulted_GetRef();
	KitCCW.Command = SpinCounterClockwise;

	FGSTN_BindingInput& KitBtn = Input.Kit.Bindings.AddDefaulted_GetRef();
	KitBtn.Command = SpinButton;

	FGSTN_BindingInput& KitGrab = Input.Kit.Bindings.AddDefaulted_GetRef();
	KitGrab.Command = GSTNLinkTags::Command_Link_Grab.GetTag();

	FGSTN_BindingInput& KitGrabSpinManual = Input.Kit.Bindings.AddDefaulted_GetRef();
	KitGrabSpinManual.Command = SpinClockwise;
	KitGrabSpinManual.Requirements.RequireTags.AddTag(GSTNLinkTags::Role_Grabber);

	// Weapon bindings: manual spin + non-spin command
	FGSTN_WeaponCompileInput& Weapon = Input.Weapons.AddDefaulted_GetRef();
	FGSTN_BindingInput& WeaponCCW = Weapon.Bindings.AddDefaulted_GetRef();
	WeaponCCW.Command = SpinCounterClockwise;

	FGSTN_BindingInput& WeaponOther = Weapon.Bindings.AddDefaulted_GetRef();
	WeaponOther.Command = GSTNLinkTags::Command_Link_Release.GetTag();

	FGSTN_LoadoutExtensionContext Context; // Null Resolved and WorldContext, as tools/tests may pass
	FGSTN_LoadoutExtensionRegistry::RunContributeCompileInput(Context, Input);

	const FGameplayTag RoleGrabber = GSTNLinkTags::Role_Grabber.GetTag();
	TestTrue(TEXT("kit CW spin ignores Role_Grabber"), Input.Kit.Bindings[0].Requirements.IgnoreTags.HasTagExact(RoleGrabber));
	TestTrue(TEXT("kit CCW spin ignores Role_Grabber"), Input.Kit.Bindings[1].Requirements.IgnoreTags.HasTagExact(RoleGrabber));
	TestTrue(TEXT("kit button spin ignores Role_Grabber"), Input.Kit.Bindings[2].Requirements.IgnoreTags.HasTagExact(RoleGrabber));
	TestFalse(TEXT("non-spin kit binding is untouched"), Input.Kit.Bindings[3].Requirements.IgnoreTags.HasTagExact(RoleGrabber));
	TestFalse(TEXT("binding requiring Role_Grabber does not also ignore it"), Input.Kit.Bindings[4].Requirements.IgnoreTags.HasTagExact(RoleGrabber));
	TestTrue(TEXT("weapon CCW spin ignores Role_Grabber"), Input.Weapons[0].Bindings[0].Requirements.IgnoreTags.HasTagExact(RoleGrabber));
	TestFalse(TEXT("non-spin weapon binding is untouched"), Input.Weapons[0].Bindings[1].Requirements.IgnoreTags.HasTagExact(RoleGrabber));

	const int32 CWIgnoreCountBefore = Input.Kit.Bindings[0].Requirements.IgnoreTags.Num();
	const int32 CCWIgnoreCountBefore = Input.Kit.Bindings[1].Requirements.IgnoreTags.Num();
	const int32 WeaponIgnoreCountBefore = Input.Weapons[0].Bindings[0].Requirements.IgnoreTags.Num();

	// Idempotent on second compile
	FGSTN_LoadoutExtensionRegistry::RunContributeCompileInput(Context, Input);
	TestEqual(TEXT("kit CW ignore tags unchanged on rerun"), Input.Kit.Bindings[0].Requirements.IgnoreTags.Num(), CWIgnoreCountBefore);
	TestEqual(TEXT("kit CCW ignore tags unchanged on rerun"), Input.Kit.Bindings[1].Requirements.IgnoreTags.Num(), CCWIgnoreCountBefore);
	TestEqual(TEXT("weapon CCW ignore tags unchanged on rerun"), Input.Weapons[0].Bindings[0].Requirements.IgnoreTags.Num(), WeaponIgnoreCountBefore);
	TestFalse(TEXT("non-spin kit binding still untouched on rerun"), Input.Kit.Bindings[3].Requirements.IgnoreTags.HasTagExact(RoleGrabber));
	TestFalse(TEXT("non-spin weapon binding still untouched on rerun"), Input.Weapons[0].Bindings[1].Requirements.IgnoreTags.HasTagExact(RoleGrabber));
	return true;
}

} // namespace GSTNLinkNPTests_Loadout

#endif
