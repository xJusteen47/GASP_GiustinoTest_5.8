#include "Tests/GSTN_TestUtils.h"
#include "Tests/GSTN_TestKeyBindingsSubsystem.h"
#include "Bindings/GSTN_KeyBindings.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "PlayerMappableKeySettings.h"
#include "UObject/StrongObjectPtr.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_KeyBindingSetTest, "GSTNInput.Core.Bindings.Set", GSTN_CORE_TEST_FLAGS)
bool FGSTN_KeyBindingSetTest::RunTest(const FString& Parameters)
{
	FGSTN_KeyBindingSet Set;
	const FName Jump(TEXT("IA_Jump"));

	TestEqual(TEXT("no override: the default key"), Set.Resolve(Jump, EKeys::SpaceBar), EKeys::SpaceBar);
	TestTrue(TEXT("a key is moved"), Set.Rebind(Jump, EKeys::SpaceBar, EKeys::J));
	TestEqual(TEXT("and resolves to the new key"), Set.Resolve(Jump, EKeys::SpaceBar), EKeys::J);
	TestEqual(TEXT("the pad row of the same action is another mapping"), Set.Resolve(Jump, EKeys::Gamepad_FaceButton_Bottom), EKeys::Gamepad_FaceButton_Bottom);
	TestFalse(TEXT("moving it to the same key changes nothing"), Set.Rebind(Jump, EKeys::SpaceBar, EKeys::J));
	TestTrue(TEXT("moving it again replaces the override"), Set.Rebind(Jump, EKeys::SpaceBar, EKeys::K));
	TestEqual(TEXT("one override per mapping"), Set.Overrides.Num(), 1);
	TestTrue(TEXT("moving it back to its default removes the override"), Set.Rebind(Jump, EKeys::SpaceBar, EKeys::SpaceBar));
	TestEqual(TEXT("so the set is empty again"), Set.Overrides.Num(), 0);
	TestFalse(TEXT("an invalid key is refused"), Set.Rebind(Jump, EKeys::SpaceBar, EKeys::Invalid));
	TestFalse(TEXT("a mapping without a name is refused"), Set.Rebind(NAME_None, EKeys::SpaceBar, EKeys::J));

	Set.Rebind(Jump, EKeys::SpaceBar, EKeys::J);
	Set.Rebind(Jump, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right);
	TestTrue(TEXT("reset puts one mapping back"), Set.Reset(Jump, EKeys::SpaceBar));
	TestFalse(TEXT("resetting it twice does nothing"), Set.Reset(Jump, EKeys::SpaceBar));
	TestTrue(TEXT("the other mapping keeps its override"), Set.IsCustomized(Jump, EKeys::Gamepad_FaceButton_Bottom));
	Set.ResetAll();
	TestEqual(TEXT("reset all empties the set"), Set.Overrides.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_KeyBindingsTwoPlayersTest, "GSTNInput.Core.Bindings.TwoLocalPlayers", GSTN_CORE_TEST_FLAGS)
bool FGSTN_KeyBindingsTwoPlayersTest::RunTest(const FString& Parameters)
{
	UGSTN_TestKeyBindingsSubsystem::Disk().Reset();
	const FName Jump(TEXT("IA_Jump"));
	const FName Attack(TEXT("IA_Attack"));

	// Two split-screen players: two local players, each with its own subsystem (never initialised: no controller, no world)
	TStrongObjectPtr<ULocalPlayer> PlayerOne(NewObject<ULocalPlayer>(GEngine));
	TStrongObjectPtr<ULocalPlayer> PlayerTwo(NewObject<ULocalPlayer>(GEngine));
	TStrongObjectPtr<UGSTN_TestKeyBindingsSubsystem> One(NewObject<UGSTN_TestKeyBindingsSubsystem>(PlayerOne.Get()));
	TStrongObjectPtr<UGSTN_TestKeyBindingsSubsystem> Two(NewObject<UGSTN_TestKeyBindingsSubsystem>(PlayerTwo.Get()));
	One->SlotIndex = 0;
	Two->SlotIndex = 1;
	TestNotEqual(TEXT("each player has its own save slot"), One->GetSaveSlotName(), Two->GetSaveSlotName());

	TestTrue(TEXT("player one moves jump"), One->Rebind(Jump, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Top));
	TestTrue(TEXT("player two moves attack"), Two->Rebind(Attack, EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_RightShoulder));
	TestEqual(TEXT("player one jumps on Y"), One->GetKey(Jump, EKeys::Gamepad_FaceButton_Bottom), EKeys::Gamepad_FaceButton_Top);
	TestEqual(TEXT("player two still jumps on A"), Two->GetKey(Jump, EKeys::Gamepad_FaceButton_Bottom), EKeys::Gamepad_FaceButton_Bottom);
	TestEqual(TEXT("player one still attacks on X"), One->GetKey(Attack, EKeys::Gamepad_FaceButton_Left), EKeys::Gamepad_FaceButton_Left);
	TestEqual(TEXT("player two attacks on RB"), Two->GetKey(Attack, EKeys::Gamepad_FaceButton_Left), EKeys::Gamepad_RightShoulder);

	TestTrue(TEXT("player one saves"), One->Save());
	TestTrue(TEXT("player two saves"), Two->Save());
	TestEqual(TEXT("two files"), UGSTN_TestKeyBindingsSubsystem::Disk().Num(), 2);

	// A new session: fresh subsystems read their own slot
	TStrongObjectPtr<UGSTN_TestKeyBindingsSubsystem> OneAgain(NewObject<UGSTN_TestKeyBindingsSubsystem>(PlayerOne.Get()));
	TStrongObjectPtr<UGSTN_TestKeyBindingsSubsystem> TwoAgain(NewObject<UGSTN_TestKeyBindingsSubsystem>(PlayerTwo.Get()));
	OneAgain->SlotIndex = 0;
	TwoAgain->SlotIndex = 1;
	TestTrue(TEXT("player one loads"), OneAgain->Load());
	TestTrue(TEXT("player two loads"), TwoAgain->Load());
	TestTrue(TEXT("player one gets back exactly its layout"), OneAgain->GetBindings() == One->GetBindings());
	TestTrue(TEXT("player two gets back exactly its layout"), TwoAgain->GetBindings() == Two->GetBindings());
	TestFalse(TEXT("and the two layouts differ"), OneAgain->GetBindings() == TwoAgain->GetBindings());

	// A third player with no file plays on the defaults
	TStrongObjectPtr<UGSTN_TestKeyBindingsSubsystem> Three(NewObject<UGSTN_TestKeyBindingsSubsystem>(PlayerOne.Get()));
	Three->SlotIndex = 2;
	TestFalse(TEXT("no file: nothing read"), Three->Load());
	TestEqual(TEXT("so no override"), Three->GetBindings().Overrides.Num(), 0);

	OneAgain->ResetAllToDefault();
	TestEqual(TEXT("reset to default empties player one"), OneAgain->GetBindings().Overrides.Num(), 0);
	TestEqual(TEXT("and leaves player two alone"), TwoAgain->GetBindings().Overrides.Num(), 1);

	UGSTN_TestKeyBindingsSubsystem::Disk().Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_KeyBindingsEnhancedInputTest, "GSTNInput.Core.Bindings.EnhancedInputProfiles", GSTN_CORE_TEST_FLAGS)
bool FGSTN_KeyBindingsEnhancedInputTest::RunTest(const FString& Parameters)
{
	// A context like the project's: actions whose assets have no player-mappable settings, a keyboard and a pad row
	TStrongObjectPtr<UInputAction> JumpAction(NewObject<UInputAction>(GetTransientPackage(), TEXT("IA_GSTNTest_Jump")));
	TStrongObjectPtr<UInputAction> AttackAction(NewObject<UInputAction>(GetTransientPackage(), TEXT("IA_GSTNTest_Attack")));
	TStrongObjectPtr<UInputMappingContext> Context(NewObject<UInputMappingContext>(GetTransientPackage(), TEXT("IMC_GSTNTest")));
	Context->MapKey(JumpAction.Get(), EKeys::SpaceBar);
	Context->MapKey(JumpAction.Get(), EKeys::Gamepad_FaceButton_Bottom);
	Context->MapKey(AttackAction.Get(), EKeys::Gamepad_FaceButton_Left);

	TestNull(TEXT("the action starts without player-mappable settings"), JumpAction->GetPlayerMappableKeySettings().Get());
	FGSTN_KeyBindingsApply::EnsureMappable(Context.Get());
	const UPlayerMappableKeySettings* Made = JumpAction->GetPlayerMappableKeySettings().Get();
	if (!TestNotNull(TEXT("it gets an identity"), Made))
	{
		return false;
	}
	TestEqual(TEXT("named after the action"), Made->Name, JumpAction->GetFName());
	TestTrue(TEXT("never saved into the asset"), Made->HasAnyFlags(RF_Transient));
	FGSTN_KeyBindingsApply::EnsureMappable(Context.Get());
	TestTrue(TEXT("a second call keeps the same identity"), JumpAction->GetPlayerMappableKeySettings().Get() == Made);

	// Two local players, each with its own engine user settings
	TStrongObjectPtr<ULocalPlayer> PlayerOne(NewObject<ULocalPlayer>(GEngine));
	TStrongObjectPtr<ULocalPlayer> PlayerTwo(NewObject<ULocalPlayer>(GEngine));
	TStrongObjectPtr<UEnhancedInputUserSettings> SettingsOne(NewObject<UEnhancedInputUserSettings>(GetTransientPackage()));
	TStrongObjectPtr<UEnhancedInputUserSettings> SettingsTwo(NewObject<UEnhancedInputUserSettings>(GetTransientPackage()));
	SettingsOne->Initialize(PlayerOne.Get());
	SettingsTwo->Initialize(PlayerTwo.Get());
	SettingsOne->RegisterInputMappingContext(Context.Get());
	SettingsTwo->RegisterInputMappingContext(Context.Get());

	const FName Jump = JumpAction->GetFName();
	const FName Attack = AttackAction->GetFName();
	TestEqual(TEXT("registered rows start on the default key"), FGSTN_KeyBindingsApply::GetCurrentKey(*SettingsOne, Jump, EKeys::SpaceBar), EKeys::SpaceBar);

	FGSTN_KeyBindingSet LayoutOne;
	LayoutOne.Rebind(Jump, EKeys::SpaceBar, EKeys::J);
	LayoutOne.Rebind(Jump, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Top);
	FGSTN_KeyBindingSet LayoutTwo;
	LayoutTwo.Rebind(Attack, EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_RightShoulder);
	LayoutTwo.Rebind(FName(TEXT("IA_NotInAnyContext")), EKeys::E, EKeys::R);

	TestEqual(TEXT("player one: both jump rows applied"), FGSTN_KeyBindingsApply::ApplyToUserSettings(*SettingsOne, LayoutOne), 2);
	TestEqual(TEXT("player two: only the row that exists"), FGSTN_KeyBindingsApply::ApplyToUserSettings(*SettingsTwo, LayoutTwo), 1);

	TestEqual(TEXT("P1 keyboard jump on J"), FGSTN_KeyBindingsApply::GetCurrentKey(*SettingsOne, Jump, EKeys::SpaceBar), EKeys::J);
	TestEqual(TEXT("P1 pad jump on Y"), FGSTN_KeyBindingsApply::GetCurrentKey(*SettingsOne, Jump, EKeys::Gamepad_FaceButton_Bottom), EKeys::Gamepad_FaceButton_Top);
	TestEqual(TEXT("P1 attack untouched"), FGSTN_KeyBindingsApply::GetCurrentKey(*SettingsOne, Attack, EKeys::Gamepad_FaceButton_Left), EKeys::Gamepad_FaceButton_Left);
	TestEqual(TEXT("P2 pad jump untouched"), FGSTN_KeyBindingsApply::GetCurrentKey(*SettingsTwo, Jump, EKeys::Gamepad_FaceButton_Bottom), EKeys::Gamepad_FaceButton_Bottom);
	TestEqual(TEXT("P2 attack on RB"), FGSTN_KeyBindingsApply::GetCurrentKey(*SettingsTwo, Attack, EKeys::Gamepad_FaceButton_Left), EKeys::Gamepad_RightShoulder);

	// Removing an override brings its row back on the next apply
	LayoutOne.Reset(Jump, EKeys::SpaceBar);
	FGSTN_KeyBindingsApply::ApplyToUserSettings(*SettingsOne, LayoutOne);
	TestEqual(TEXT("P1 keyboard jump back on space"), FGSTN_KeyBindingsApply::GetCurrentKey(*SettingsOne, Jump, EKeys::SpaceBar), EKeys::SpaceBar);
	TestEqual(TEXT("P1 pad jump still on Y"), FGSTN_KeyBindingsApply::GetCurrentKey(*SettingsOne, Jump, EKeys::Gamepad_FaceButton_Bottom), EKeys::Gamepad_FaceButton_Top);
	return true;
}

#endif
