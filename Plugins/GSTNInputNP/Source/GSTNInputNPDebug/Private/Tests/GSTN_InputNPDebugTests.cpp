#include "GameFramework/Pawn.h"
#include "GSTN_InputSimComponentSource.h"
#include "Misc/AutomationTest.h"
#include "Sources/GSTN_InputSourceRegistry.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputNPDebugFinderTest, "GSTNInput.NP.Debug.InputComponentIsRegisteredWithTheInputDisplay",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_InputNPDebugFinderTest::RunTest(const FString& Parameters)
{
	// The module's finder is in the registry of the input display; a pawn without the standalone input component has nothing for it to read
	TestTrue(TEXT("a finder is registered"), FGSTN_InputSourceRegistry::Get().GetNumFinders() >= 1);
	const TStrongObjectPtr<APawn> Pawn(NewObject<APawn>(GetTransientPackage()));
	TestFalse(TEXT("a pawn with no input component has no source"), FGSTN_InputSimComponentSource::Find(*Pawn).IsValid());
	TestFalse(TEXT("so the registry finds none either"), FGSTN_InputSourceRegistry::Get().FindSource(*Pawn).IsValid());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
