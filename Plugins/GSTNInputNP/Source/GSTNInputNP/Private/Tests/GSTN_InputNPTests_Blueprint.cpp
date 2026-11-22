#include "Tests/GSTN_InputNPTestObjects.h"

#include "Input/GSTN_BufferResetRules.h"
#include "Input/GSTN_InputTags.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

// Outside the test guard: UHT reflects the class whether or not tests are compiled.
FGameplayTagContainer UGSTN_TestBlueprintContextProvider::GetBlueprintContextTags_Implementation() const
{
	FGameplayTagContainer Tags;
	Tags.AddTag(GSTNInputTags::Context_Airborne);
	return Tags;
}

FGameplayTagContainer UGSTN_TestBlueprintContextProvider::GetBlueprintFrameEvents_Implementation(uint8 PreviousFingerprint, uint8 CurrentFingerprint) const
{
	FGameplayTagContainer Tags;
	if (PreviousFingerprint == 2 && CurrentFingerprint == 1)
	{
		Tags.AddTag(GSTNInputTags::BufferEvent_Landed);
	}
	return Tags;
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPBlueprintContextProviderTest, "GSTNInput.NP.Context.BlueprintProvider", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_NPBlueprintContextProviderTest::RunTest(const FString& Parameters)
{
	// The simulation calls the plain C++ virtuals; the Blueprint provider routes them to its events
	const TStrongObjectPtr<UGSTN_TestBlueprintContextProvider> Provider(NewObject<UGSTN_TestBlueprintContextProvider>(GetTransientPackage()));
	const UGSTN_InputContextProvider* Base = Provider.Get();

	TestEqual(TEXT("facing comes from the event"), Base->GetFacingYaw(), 90.f);
	TestEqual(TEXT("fingerprint comes from the event"), static_cast<int32>(Base->GetContextFingerprint()), 2);

	FGameplayTagContainer Tags;
	Tags.AddTag(GSTNInputTags::Context_Grounded);
	Base->AppendContextTags(Tags);
	TestTrue(TEXT("the event's tags are appended"), Tags.HasTagExact(GSTNInputTags::Context_Airborne));
	TestTrue(TEXT("what the host had is kept"), Tags.HasTagExact(GSTNInputTags::Context_Grounded));

	FGSTN_BufferFrameEvents Events;
	Base->AppendFrameEvents(2, 2, Events);
	TestTrue(TEXT("no change, no event"), Events.EventTags.IsEmpty());
	Base->AppendFrameEvents(2, 1, Events);
	TestTrue(TEXT("a change gives the event's tags"), Events.EventTags.HasTagExact(GSTNInputTags::BufferEvent_Landed));

	// The default Blueprint provider (no override) is neutral: yaw 0, no tags, no events
	const TStrongObjectPtr<UGSTN_BlueprintInputContextProvider> Plain(NewObject<UGSTN_BlueprintInputContextProvider>(GetTransientPackage()));
	FGameplayTagContainer PlainTags;
	Plain->AppendContextTags(PlainTags);
	TestEqual(TEXT("default facing"), Plain->GetFacingYaw(), 0.f);
	TestTrue(TEXT("default tags"), PlainTags.IsEmpty());
	TestEqual(TEXT("default fingerprint"), static_cast<int32>(Plain->GetContextFingerprint()), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
