// Automation test of the NP fork's late correction (M7-15): where a server state older than the whole client history is injected. The
// decision is the pure NpLateCorrectionFrame; the mismatch check that gates it is the model's own ShouldReconcile (covered by the
// fragment tests), so no world is needed.

#include "Misc/AutomationTest.h"
#include "Services/NetworkPredictionInstanceData.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CombatNPLateCorrectionTest, "GSTNCombat.NP.LateCorrection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FGSTN_CombatNPLateCorrectionTest::RunTest(const FString& Parameters)
{
	const int32 History = NpInstanceFrameBufferSize - 1;

	// The M7-14 mounted_hit run at 2 %: PendingFrame 573, a state for local frame 482 (91 frames back, history 63)
	TestEqual(TEXT("a state older than the history is injected at the newest simulated frame"), NpLateCorrectionFrame(573, 482, History), 572);
	TestEqual(TEXT("so is one that maps before frame 0 once the history is full"), NpLateCorrectionFrame(573, -5, History), 572);

	// Inside the history the normal reconcile compares it with its own frame
	TestEqual(TEXT("a state exactly History frames back is a normal reconcile"), NpLateCorrectionFrame(573, 573 - History, History), INDEX_NONE);
	TestEqual(TEXT("a recent state is a normal reconcile"), NpLateCorrectionFrame(573, 570, History), INDEX_NONE);

	// Warm-up: the history is not full and the offset is still settling, nothing to correct against yet
	TestEqual(TEXT("no late correction before the history is full"), NpLateCorrectionFrame(History, -1, History), INDEX_NONE);
	TestEqual(TEXT("the first full history frame may correct"), NpLateCorrectionFrame(History + 1, -1, History), History);
	return true;
}

#endif
