// Links, Network Prediction half: shared helpers of the NP automation tests.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_LinkRegistry.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkSnapshotExt.h"
#include "Sync/GSTN_SyncFragment.h"
#include "UObject/GCObjectScopeGuard.h"
#include "UObject/Package.h"
#include "World/GSTN_StepSnapshot.h"

namespace GSTNLinkNPTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Test-only link types and roles (test builds only; no production data names them), so the tests never collide with content grab types */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TestGrab);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TestGrabber);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TestGrabbed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TestLatch);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TestLatcher);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(TestLatched);

	/** The hand socket the test grab parks its target on, and the spine the test latch parks its initiator on */
	inline FName HandSocket() { return TEXT("hand_r"); }
	inline FName SpineSocket() { return TEXT("spine_03"); }

	/** Registers a test grab (target parked on the grabber's hand, offset 30 cm forward, never acting) and a test latch (initiator parked on the
	 *  partner's spine, both roles acting) with the global registry for the scope, and removes them after */
	struct FScopedLinkTypes
	{
		UGSTN_LinkTypeDefinition* Grab = nullptr;
		UGSTN_LinkTypeDefinition* Latch = nullptr;
		uint8 GrabIndex = 0;
		uint8 LatchIndex = 0;

		FScopedLinkTypes();
		~FScopedLinkTypes();
	};

	/** A fragment whose Body channel holds a live link */
	FGSTN_LinkFragment MakeLinked(uint8 TypeIndex, uint8 Role, int32 PartnerId, uint8 Serial, int32 LinkFrame);

	/** A snapshot entry with a link blob (BodyIndex, the fragment's summary and its bits) at a transform */
	FGSTN_SnapshotEntry MakeEntry(int32 EntityId, const FTransform& Transform, uint16 BodyIndex, const FGSTN_LinkFragment* Fragment);

	/** The fragment list's bits and a read back, as the wire sees them */
	int64 BitsOfList(FGSTN_SyncFragmentArray& Fragments);
	FGSTN_SyncFragmentArray ReadBack(FGSTN_SyncFragmentArray& Fragments, bool& bOutError);
	int32 PayloadBytes(const FGSTN_LinkFragment& Fragment);
	FGSTN_SyncFragmentArray ListOf(const FGSTN_LinkFragment& Fragment);
}

#endif
