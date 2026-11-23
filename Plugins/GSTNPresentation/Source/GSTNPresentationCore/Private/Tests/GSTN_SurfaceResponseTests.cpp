// Tests of the surface response table (docs/notes/surface-feedback.md): GSTNPresentation.Core.Surface.*
//
// Pure: transient tables, no world. Native tags only (Presentation.Event.*, Presentation.Footwear.*), so no ini is needed.

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_PresentationTags.h"
#include "GSTN_SurfaceFeedbackComponent.h"
#include "GSTN_SurfaceResponseTypes.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace GSTNSurfaceResponseTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	static FGSTN_SurfaceResponseRow Row(const FGameplayTag& Event, const FGameplayTag& Footwear, int32 FloorOrAny, float Volume)
	{
		FGSTN_SurfaceResponseRow R;
		R.EventTag = Event;
		R.Footwear = Footwear;
		R.bAnyFloor = FloorOrAny < 0;
		R.Floor = FloorOrAny < 0 ? SurfaceType_Default : static_cast<EPhysicalSurface>(FloorOrAny);
		R.Volume = Volume;  // the row's id in these tests
		return R;
	}

	static float Pick(const UGSTN_SurfaceResponseTable* Table, const FGameplayTag& Event, const FGameplayTag& Footwear, int32 Floor)
	{
		const FGSTN_SurfaceResponseRow* R = Table->FindRow(Event, Footwear, static_cast<EPhysicalSurface>(Floor));
		return R ? R->Volume : -1.f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceResponseLookupTest, "GSTNPresentation.Core.Surface.Lookup", GSTNSurfaceResponseTest::Flags)
bool FGSTN_SurfaceResponseLookupTest::RunTest(const FString& Parameters)
{
	using namespace GSTNSurfaceResponseTest;
	const FGameplayTag Land = GSTNPresentationTags::Land;
	const FGameplayTag Hit = GSTNPresentationTags::Hit;
	const FGameplayTag Shield = GSTNPresentationTags::Shield;
	const FGameplayTag ShieldHit = GSTNPresentationTags::Shield_Hit;
	const FGameplayTag Metal = GSTNPresentationTags::Footwear_Metal;
	const FGameplayTag Clay = GSTNPresentationTags::Footwear_Clay;
	constexpr int32 Any = -1;
	constexpr int32 Stone = 1;
	constexpr int32 Grass = 2;

	UGSTN_SurfaceResponseTable* Parent = NewObject<UGSTN_SurfaceResponseTable>(GetTransientPackage());
	Parent->Rows = {Row(Hit, FGameplayTag(), Any, 9.f)};

	UGSTN_SurfaceResponseTable* Table = NewObject<UGSTN_SurfaceResponseTable>(GetTransientPackage());
	Table->Parent = Parent;
	Table->Rows = {
		Row(Land, FGameplayTag(), Any, 1.f),	// event default
		Row(Land, Metal, Any, 2.f),				// footwear x any floor
		Row(Land, Metal, Stone, 3.f),			// exact
		Row(Land, FGameplayTag(), Grass, 4.f),	// any footwear x floor
		Row(Shield, FGameplayTag(), Any, 5.f),	// parent event tag
		Row(Land, Metal, Any, 6.f)				// a duplicate key: never wins (tie keeps the first)
	};

	TestEqual(TEXT("exact row wins"), Pick(Table, Land, Metal, Stone), 3.f);
	TestEqual(TEXT("footwear beats floor"), Pick(Table, Land, Metal, Grass), 2.f);
	TestEqual(TEXT("footwear x any floor"), Pick(Table, Land, Metal, 0), 2.f);
	TestEqual(TEXT("any footwear x floor"), Pick(Table, Land, Clay, Grass), 4.f);
	TestEqual(TEXT("event default"), Pick(Table, Land, Clay, Stone), 1.f);
	TestEqual(TEXT("child event uses the parent tag's row"), Pick(Table, ShieldHit, Clay, 0), 5.f);
	TestEqual(TEXT("no row here: the parent table answers"), Pick(Table, Hit, Metal, Stone), 9.f);
	TestEqual(TEXT("no row anywhere: null (stock)"), Pick(Table, GSTNPresentationTags::Jump, Clay, 0), -1.f);

	TestTrue(TEXT("Land has a floor row: trace needed"), Table->NeedsFloor(Land));
	TestFalse(TEXT("Shield.Hit has none: no trace"), Table->NeedsFloor(ShieldHit));

	TestEqual(TEXT("default footwear order: Metal = 1"), Table->GetFootwearParam(Metal), 1);
	TestEqual(TEXT("unknown footwear sends Clay 0"), Table->GetFootwearParam(FGameplayTag()), 0);
	Table->FootwearParamOrder = {Metal, Clay};
	TestEqual(TEXT("custom order"), Table->GetFootwearParam(Clay), 1);

	TestEqual(TEXT("haptic level 0 = off"), UGSTN_SurfaceFeedbackComponent::HapticLevelScale(0), 0.f);
	TestEqual(TEXT("haptic level 2 = normal"), UGSTN_SurfaceFeedbackComponent::HapticLevelScale(2), 1.f);
	TestEqual(TEXT("haptic level clamps"), UGSTN_SurfaceFeedbackComponent::HapticLevelScale(9), 1.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SurfaceResponseLoopTest, "GSTNPresentation.Core.Surface.ParentLoop", GSTNSurfaceResponseTest::Flags)
bool FGSTN_SurfaceResponseLoopTest::RunTest(const FString& Parameters)
{
	UGSTN_SurfaceResponseTable* A = NewObject<UGSTN_SurfaceResponseTable>(GetTransientPackage());
	UGSTN_SurfaceResponseTable* B = NewObject<UGSTN_SurfaceResponseTable>(GetTransientPackage());
	A->Parent = B;
	B->Parent = A;
	TestNull(TEXT("a parent loop ends with no answer"), A->FindRow(GSTNPresentationTags::Land, FGameplayTag(), SurfaceType_Default));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
