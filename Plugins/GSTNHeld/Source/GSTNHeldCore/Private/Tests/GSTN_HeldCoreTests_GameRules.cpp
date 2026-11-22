// GR-1: the held objects' piece of the game rules.

#include "GameRules/GSTN_GameRules.h"
#include "GSTN_HeldSettings.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldGameRulesTest, "GSTNHeld.Core.GameRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldGameRulesTest::RunTest(const FString& Parameters)
{
	const UGSTN_GameRules* Previous = FGSTN_ActiveGameRules::Get();
	FGSTN_ActiveGameRules::Set(nullptr);

	// No asset: the defaults (the class default object, from DefaultGSTNHeld.ini), the values the old Project Settings held
	const UGSTN_HeldRules& Defaults = UGSTN_HeldRules::Get();
	TestTrue(TEXT("no asset = class defaults"), &Defaults == GetDefault<UGSTN_HeldRules>());
	TestEqual(TEXT("default size classes kept"), Defaults.SizeClasses.Num(), 4);
	TestEqual(TEXT("default capacity kept"), Defaults.DefaultCarryCapacityKg, 50.f);
	TestEqual(TEXT("default aim hold kept"), Defaults.ThrowInput.AimHoldFrames, 12);

	// An asset with a held piece: its values win; a new piece starts from the defaults
	UGSTN_GameRules* Rules = NewObject<UGSTN_GameRules>(GetTransientPackage());
	UGSTN_HeldRules* Piece = NewObject<UGSTN_HeldRules>(Rules);
	TestEqual(TEXT("new piece starts from the defaults"), Piece->SizeClasses.Num(), 4);
	Piece->DefaultCarryCapacityKg = 80.f;
	Rules->Pieces.Add(Piece);
	FGSTN_ActiveGameRules::Set(Rules);
	TestTrue(TEXT("active piece read"), &UGSTN_HeldRules::Get() == Piece);
	TestEqual(TEXT("asset value wins"), UGSTN_HeldRules::Get().DefaultCarryCapacityKg, 80.f);

	// An asset without the held piece: the defaults again
	Rules->Pieces.Reset();
	TestTrue(TEXT("asset without piece = defaults"), &UGSTN_HeldRules::Get() == GetDefault<UGSTN_HeldRules>());

	FGSTN_ActiveGameRules::Set(Previous);
	return true;
}

#endif
