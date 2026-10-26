// Automation tests of the restore request (plan P7b): the effect that carries it, its component, the queue. On a stock ability system
// component: no Network Prediction, no Mover.

#include "AbilitySystemComponent.h"
#include "Effects/GSTN_GEComponent_RestoreParts.h"
#include "Events/GSTN_PartEvents.h"
#include "GSTN_PartsTags.h"
#include "Tests/GSTN_PartsGASTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PartsGASRestoreRequestsTest, "GSTNParts.GAS.Restore.GEComponentRequests", GSTNPartsGASTest::Flags)
bool FGSTN_PartsGASRestoreRequestsTest::RunTest(const FString& Parameters)
{
	GSTNPartsGASTest::FRig Rig;
	UAbilitySystemComponent& ASC = *Rig.ASC;

	// The native default carries the component with its defaults and is infinite: the effect itself is the request
	const UGSTN_GE_PartRestore* Default = GetDefault<UGSTN_GE_PartRestore>();
	TestTrue(TEXT("the default restore effect is infinite"), Default->DurationPolicy == EGameplayEffectDurationType::Infinite);
	const UGSTN_GEComponent_RestoreParts* Component = Default->FindComponent<UGSTN_GEComponent_RestoreParts>();
	TestNotNull(TEXT("and carries the restore component"), Component);

	TArray<FGSTN_PendingPartRestore> Pending;
	FGSTN_PartRestoreQueue::Collect(ASC, Pending);
	TestTrue(TEXT("nothing pending on a fresh ability system"), Pending.IsEmpty());

	// Mask and fraction ride in the spec: the level and one set-by-caller value
	FGSTN_PartRestoreRequest Asked;
	Asked.PartMask = 0b101;
	Asked.DamageFraction = 0.5f;
	TestTrue(TEXT("the request is applied"), FGSTN_PartRestoreQueue::Apply(ASC, nullptr, Asked, nullptr));
	FGSTN_PartRestoreRequest Every;
	TestTrue(TEXT("a second one, every part, is applied (they do not merge)"), FGSTN_PartRestoreQueue::Apply(ASC, nullptr, Every, nullptr));

	FGSTN_PartRestoreQueue::Collect(ASC, Pending);
	TestEqual(TEXT("both are pending"), Pending.Num(), 2);
	if (Pending.Num() == 2)
	{
		const int32 WithMask = Pending[0].Request.PartMask != 0 ? 0 : 1;
		const FGSTN_PartRestoreRequest& A = Pending[WithMask].Request;
		const FGSTN_PartRestoreRequest& B = Pending[1 - WithMask].Request;
		TestEqual(TEXT("the mask of the call"), A.PartMask, 0b101);
		TestTrue(TEXT("the fraction of the call"), FMath::IsNearlyEqual(A.DamageFraction, 0.5f, 1e-4f));
		TestEqual(TEXT("mask 0 stays the asset's mask (every part)"), B.PartMask, 0);
		TestTrue(TEXT("full repair by default"), FMath::IsNearlyEqual(B.DamageFraction, 0.f, 1e-4f));
	}

	// Collect twice gives the same list in the same order (every machine must consume them in the same order)
	TArray<FGSTN_PendingPartRestore> Again;
	FGSTN_PartRestoreQueue::Collect(ASC, Again);
	bool bSameOrder = Again.Num() == Pending.Num();
	for (int32 Index = 0; bSameOrder && Index < Again.Num(); ++Index)
	{
		bSameOrder = Again[Index].Handle == Pending[Index].Handle;
	}
	TestTrue(TEXT("the order is stable"), bSameOrder);

	// Consuming removes exactly the handled ones
	TArray<FGSTN_PendingPartRestore> First;
	First.Add(Pending[0]);
	TestEqual(TEXT("one effect removed"), FGSTN_PartRestoreQueue::Consume(ASC, First), 1);
	FGSTN_PartRestoreQueue::Collect(ASC, Again);
	TestEqual(TEXT("the other is still pending"), Again.Num(), 1);
	FGSTN_PartRestoreQueue::Consume(ASC, Again);
	FGSTN_PartRestoreQueue::Collect(ASC, Again);
	TestTrue(TEXT("none left"), Again.IsEmpty());

	// The event made for a restore: ascending parts, the new serial, no band
	FGSTN_SimEventOutbox Outbox;
	TestEqual(TEXT("one event per restored part"), FGSTN_PartEvents::AppendRestored(nullptr, nullptr, 9, 0b1010, 5, Outbox), 2);
	if (Outbox.Events.Num() == 2)
	{
		TestTrue(TEXT("type"), Outbox.Events[0].EventType == GSTNPartsTags::Event_Part_Restored);
		TestEqual(TEXT("first part"), static_cast<int32>(FGSTN_PartEvents::GetPayload(Outbox.Events[0]).PartIndex), 1);
		TestEqual(TEXT("second part"), static_cast<int32>(FGSTN_PartEvents::GetPayload(Outbox.Events[1]).PartIndex), 3);
		TestEqual(TEXT("serial"), static_cast<int32>(FGSTN_PartEvents::GetPayload(Outbox.Events[1]).BreakSerial), 5);
		TestEqual(TEXT("no band"), static_cast<int32>(FGSTN_PartEvents::GetPayload(Outbox.Events[1]).Band), 255);
		TestEqual(TEXT("frame"), Outbox.Events[0].Frame, 9);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
