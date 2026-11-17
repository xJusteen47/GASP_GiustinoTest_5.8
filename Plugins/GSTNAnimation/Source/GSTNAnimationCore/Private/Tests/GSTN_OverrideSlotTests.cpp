// Tests of the override slot choice (anim-layer-skid-wallslide-piano L1): GSTNAnimation.Core.OverrideSlot. Pure.

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_OverrideLayerSource.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_OverrideSlotPickTest, "GSTNAnimation.Core.OverrideSlot.Pick",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_OverrideSlotPickTest::RunTest(const FString& Parameters)
{
	auto Make = [](bool bActive, int32 Slot, float BlendTime = 0.15f)
	{
		FGSTN_OverrideCandidate C;
		C.bActive = bActive;
		C.Slot = Slot;
		C.Priority = GSTNOverrideSlot::DefaultPriority(Slot);
		C.BlendTime = BlendTime;
		return C;
	};

	FGSTN_OverridePick Pick = FGSTN_OverridePick::Pick({});
	TestEqual(TEXT("no layer: base pose"), Pick.Slot, GSTNOverrideSlot::None);
	TestEqual(TEXT("no layer: no index"), Pick.Index, INDEX_NONE);

	Pick = FGSTN_OverridePick::Pick({Make(false, GSTNOverrideSlot::Wall), Make(false, GSTNOverrideSlot::Skid)});
	TestEqual(TEXT("none active: base pose"), Pick.Slot, GSTNOverrideSlot::None);

	Pick = FGSTN_OverridePick::Pick({Make(true, GSTNOverrideSlot::RunStart, 0.1f), Make(true, GSTNOverrideSlot::Skid, 0.2f)});
	TestEqual(TEXT("skid over run start"), Pick.Slot, GSTNOverrideSlot::Skid);
	TestEqual(TEXT("its blend time"), Pick.BlendTime, 0.2f);
	TestEqual(TEXT("its index"), Pick.Index, 1);

	Pick = FGSTN_OverridePick::Pick({Make(true, GSTNOverrideSlot::Skid), Make(true, GSTNOverrideSlot::Wall), Make(true, GSTNOverrideSlot::RunStart)});
	TestEqual(TEXT("wall over everything"), Pick.Slot, GSTNOverrideSlot::Wall);

	Pick = FGSTN_OverridePick::Pick({Make(true, GSTNOverrideSlot::None), Make(true, GSTNOverrideSlot::RunStart)});
	TestEqual(TEXT("slot 0 is never a layer"), Pick.Slot, GSTNOverrideSlot::RunStart);

	FGSTN_OverrideCandidate Custom = Make(true, GSTNOverrideSlot::RunStart);
	Custom.Priority = 99;
	Pick = FGSTN_OverridePick::Pick({Make(true, GSTNOverrideSlot::Wall), Custom});
	TestEqual(TEXT("a layer may raise its own priority"), Pick.Slot, GSTNOverrideSlot::RunStart);

	Pick = FGSTN_OverridePick::Pick({Make(true, GSTNOverrideSlot::Skid, 0.1f), Make(true, GSTNOverrideSlot::Skid, 0.3f)});
	TestEqual(TEXT("tie: the first"), Pick.Index, 0);

	TestTrue(TEXT("wall > skid > run start"), GSTNOverrideSlot::DefaultPriority(GSTNOverrideSlot::Wall) > GSTNOverrideSlot::DefaultPriority(GSTNOverrideSlot::Skid)
		&& GSTNOverrideSlot::DefaultPriority(GSTNOverrideSlot::Skid) > GSTNOverrideSlot::DefaultPriority(GSTNOverrideSlot::RunStart));
	return true;
}

#endif
