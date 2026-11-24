// Automation tests of the pure cosmetics layer: piece lookup and fallback, fit, splines, silhouette, style, the plan, the cache key
// and validation. Pure: transient data assets, no mesh loaded, no world.

#include "Misc/AutomationTest.h"
#include "GSTN_CosmeticData.h"
#include "GSTN_CosmeticPlanning.h"
#include "GSTN_CosmeticTags.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags CosmeticsTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	const TCHAR* const PieceMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");

	/** Any tag works as a piece tag; a style option hides pieces by it */
	FGameplayTag CPSpikyTag() { return GSTNCosmeticTags::Slot_Body_Back.GetTag(); }

	FGSTN_CosmeticPiece CPPiece(const FGameplayTag& Class, const FVector& Size, EGSTN_CosmeticFit Fit = EGSTN_CosmeticFit::Uniform)
	{
		FGSTN_CosmeticPiece Piece;
		Piece.SlotClass = Class;
		Piece.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(PieceMeshPath));
		Piece.NativeSize = Size;
		Piece.Fit = Fit;
		return Piece;
	}

	FGSTN_CosmeticSlot CPSlot(const TCHAR* Socket, const FGameplayTag& Class, const FVector& Location, const FVector& Envelope, bool bMirrored = false)
	{
		FGSTN_CosmeticSlot Slot;
		Slot.Socket = Socket;
		Slot.SlotClass = Class;
		Slot.MeshTransform = FTransform(Location);
		Slot.Envelope.Size = Envelope;
		Slot.bMirrored = bMirrored;
		return Slot;
	}

	/** A sword: 100 cm along X from the grip (-20) to the tip (80), 10 cm wide, 4 cm thick; pommel, two guards, tip, one spine */
	TStrongObjectPtr<UGSTN_CosmeticHostData> CPSword()
	{
		TStrongObjectPtr<UGSTN_CosmeticHostData> Host(NewObject<UGSTN_CosmeticHostData>(GetTransientPackage()));
		Host->BaseMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(PieceMeshPath));
		Host->BaseBounds = FBox(FVector(-20, -5, -2), FVector(80, 5, 2));
		Host->Slots.Add(CPSlot(TEXT("SLOT_Pommel_0"), GSTNCosmeticTags::Slot_Grip_Pommel, FVector(-18, 0, 0), FVector(4)));
		Host->Slots.Add(CPSlot(TEXT("SLOT_Guard_R_0"), GSTNCosmeticTags::Slot_Guard, FVector(0, 4, 0), FVector(3, 4, 3)));
		Host->Slots.Add(CPSlot(TEXT("SLOT_Guard_L_0"), GSTNCosmeticTags::Slot_Guard, FVector(0, -4, 0), FVector(3, 4, 3), true));
		Host->Slots.Add(CPSlot(TEXT("SLOT_Tip_0"), GSTNCosmeticTags::Slot_Tip, FVector(70, 0, 0), FVector(6, 3, 3)));
		FGSTN_CosmeticSpline& Spine = Host->Splines.AddDefaulted_GetRef();
		Spine.SplineClass = GSTNCosmeticTags::Spline_Spine;
		Spine.Points = { FVector(5, 0, 2), FVector(35, 0, 2), FVector(65, 0, 2) };
		return Host;
	}

	/** A theme with a grip piece (no pommel of its own), a guard piece with a "Spiky" tag, a spine module and a colour channel */
	TStrongObjectPtr<UGSTN_CosmeticThemeData> CPTheme()
	{
		TStrongObjectPtr<UGSTN_CosmeticThemeData> Theme(NewObject<UGSTN_CosmeticThemeData>(GetTransientPackage()));
		// Any leaf under the theme root works as an identity for the tests
		Theme->ThemeTag = GSTNCosmeticTags::Theme;
		Theme->LayerMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
		Theme->Pieces.Add(CPPiece(GSTNCosmeticTags::Slot_Grip, FVector(8)));
		FGSTN_CosmeticPiece Guard = CPPiece(GSTNCosmeticTags::Slot_Guard, FVector(2, 2, 2));
		Guard.Tags.AddTag(CPSpikyTag());
		Theme->Pieces.Add(Guard);
		FGSTN_CosmeticSplineModule& Module = Theme->SplineModules.AddDefaulted_GetRef();
		Module.SplineClass = GSTNCosmeticTags::Spline_Spine;
		Module.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(PieceMeshPath));
		Module.ModuleLength = 7.f;
		FGSTN_CosmeticStyleChannel& Colour = Theme->StyleChannels.AddDefaulted_GetRef();
		Colour.Channel = GSTNCosmeticTags::Style;
		Colour.Options.AddDefaulted();
		FGSTN_CosmeticStyleOption& Dark = Colour.Options.AddDefaulted_GetRef();
		Dark.bOverridePalette = true;
		Dark.Palette.Primary = FLinearColor::Black;
		Dark.EmissiveScale = 2.f;
		Dark.HidePiecesWithTags.AddTag(CPSpikyTag());
		return Theme;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNCosmeticsFindPieceTest, "GSTNCosmetics.Core.FindPiece", CosmeticsTestFlags)
bool FGSTNCosmeticsFindPieceTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CosmeticThemeData> Theme = CPTheme();
	TStrongObjectPtr<UGSTN_CosmeticHostData> Sword = CPSword();
	bool bOverride = false;
	const FGSTN_CosmeticPiece* Pommel = FGSTN_CosmeticPlanning::FindPiece(*Theme, Sword.Get(), GSTNCosmeticTags::Slot_Grip_Pommel, bOverride);
	TestTrue(TEXT("Pommel falls back to the theme's grip piece"), Pommel && Pommel->SlotClass == GSTNCosmeticTags::Slot_Grip.GetTag());
	TestFalse(TEXT("not an override"), bOverride);
	TestNull(TEXT("No tip piece and no parent piece: nothing"), FGSTN_CosmeticPlanning::FindPiece(*Theme, Sword.Get(), GSTNCosmeticTags::Slot_Tip, bOverride));

	FGSTN_CosmeticPieceOverride& Override = Sword->Overrides.AddDefaulted_GetRef();
	Override.Theme = Theme->ThemeTag;
	Override.Piece = CPPiece(GSTNCosmeticTags::Slot_Grip_Pommel, FVector(3));
	const FGSTN_CosmeticPiece* Overridden = FGSTN_CosmeticPlanning::FindPiece(*Theme, Sword.Get(), GSTNCosmeticTags::Slot_Grip_Pommel, bOverride);
	TestTrue(TEXT("The item's override for this theme wins"), bOverride && Overridden == &Sword->Overrides[0].Piece);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNCosmeticsFitTest, "GSTNCosmetics.Core.Fit", CosmeticsTestFlags)
bool FGSTNCosmeticsFitTest::RunTest(const FString& Parameters)
{
	FGSTN_CosmeticEnvelope Envelope;
	Envelope.Size = FVector(4, 8, 2);
	FVector Scale;
	FString Why;

	FGSTN_CosmeticPiece Uniform = CPPiece(GSTNCosmeticTags::Slot_Guard, FVector(4));
	TestTrue(TEXT("Uniform fits"), FGSTN_CosmeticPlanning::FitScale(Uniform, Envelope, Scale, Why));
	TestTrue(TEXT("Uniform takes the tightest axis"), Scale.Equals(FVector(0.5), 0.001));

	FGSTN_CosmeticPiece PerAxis = CPPiece(GSTNCosmeticTags::Slot_Guard, FVector(4), EGSTN_CosmeticFit::PerAxis);
	TestTrue(TEXT("Per axis fits"), FGSTN_CosmeticPlanning::FitScale(PerAxis, Envelope, Scale, Why));
	TestTrue(TEXT("Per axis fills each axis, capped at max scale 2"), Scale.Equals(FVector(1, 2, 0.5), 0.001));

	FGSTN_CosmeticPiece Tiny = Uniform;
	Tiny.NativeSize = FVector(40);
	TestFalse(TEXT("Scaled under its minimum: dropped"), FGSTN_CosmeticPlanning::FitScale(Tiny, Envelope, Scale, Why));
	TestFalse(TEXT("with a reason"), Why.IsEmpty());

	FGSTN_CosmeticPiece Fixed = CPPiece(GSTNCosmeticTags::Slot_Guard, FVector(3, 3, 3), EGSTN_CosmeticFit::None);
	TestFalse(TEXT("Fit None larger than the slot: refused"), FGSTN_CosmeticPlanning::FitScale(Fixed, Envelope, Scale, Why));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNCosmeticsSplineTest, "GSTNCosmetics.Core.Spline", CosmeticsTestFlags)
bool FGSTNCosmeticsSplineTest::RunTest(const FString& Parameters)
{
	FGSTN_CosmeticSpline Ring;
	Ring.Points = { FVector(0, 0, 0), FVector(10, 0, 0), FVector(10, 10, 0), FVector(0, 10, 0) };
	TestEqual(TEXT("Open polyline length"), FGSTN_CosmeticPlanning::SplineLength(Ring), 30.f, 0.001f);
	Ring.bClosed = true;
	TestEqual(TEXT("Closed ring adds the way back"), FGSTN_CosmeticPlanning::SplineLength(Ring), 40.f, 0.001f);

	float Stretch = 0.f;
	TestEqual(TEXT("60 cm of 7 cm modules: 9 rounded, capped at 8"), FGSTN_CosmeticPlanning::SegmentCount(60.f, 7.f, 8, Stretch), 8);
	TestEqual(TEXT("stretched to fit exactly"), Stretch, 60.f / 56.f, 0.001f);
	TestEqual(TEXT("Shorter than one module: still one, squeezed"), FGSTN_CosmeticPlanning::SegmentCount(3.f, 7.f, 8, Stretch), 1);
	TestEqual(TEXT("No length: none"), FGSTN_CosmeticPlanning::SegmentCount(0.f, 7.f, 8, Stretch), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNCosmeticsSilhouetteTest, "GSTNCosmetics.Core.Silhouette", CosmeticsTestFlags)
bool FGSTNCosmeticsSilhouetteTest::RunTest(const FString& Parameters)
{
	const FBox Sword(FVector(-20, -5, -2), FVector(80, 5, 2));
	const FGSTN_CosmeticLimits Limits;
	const FBox Allowed = FGSTN_CosmeticPlanning::AllowedBounds(Sword, Limits);
	TestEqual(TEXT("Length grows 5% of 100 cm at each end"), Allowed.Max.X, 85.0, 0.001);
	TestEqual(TEXT("Width grows 15% of 10 cm on each side"), Allowed.Max.Y, 6.5, 0.001);

	const FGSTN_CosmeticSlot Tip = CPSlot(TEXT("SLOT_Tip_0"), GSTNCosmeticTags::Slot_Tip, FVector(80, 0, 0), FVector(20));
	TestTrue(TEXT("A 6 cm piece at the tip reaches 83: allowed"), FGSTN_CosmeticPlanning::FitsSilhouette(Sword, Tip, FVector(6, 2, 2), FVector::OneVector, Limits));
	TestFalse(TEXT("A 12 cm piece at the tip reaches 86: it would lie about the reach"), FGSTN_CosmeticPlanning::FitsSilhouette(Sword, Tip, FVector(12, 2, 2), FVector::OneVector, Limits));

	// The socket's turn counts: a long piece turned 90 degrees sticks out sideways
	FGSTN_CosmeticSlot Turned = CPSlot(TEXT("SLOT_Guard_R_0"), GSTNCosmeticTags::Slot_Guard, FVector(0, 0, 0), FVector(20));
	Turned.MeshTransform.SetRotation(FQuat(FRotator(0, 90, 0)));
	TestTrue(TEXT("Along X it fits"), FGSTN_CosmeticPlanning::FitsSilhouette(Sword, CPSlot(TEXT("S"), GSTNCosmeticTags::Slot_Guard, FVector::ZeroVector, FVector(20)), FVector(16, 2, 2), FVector::OneVector, Limits));
	TestFalse(TEXT("Turned across, it widens the sword"), FGSTN_CosmeticPlanning::FitsSilhouette(Sword, Turned, FVector(16, 2, 2), FVector::OneVector, Limits));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNCosmeticsPlanTest, "GSTNCosmetics.Core.Plan", CosmeticsTestFlags)
bool FGSTNCosmeticsPlanTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CosmeticThemeData> Theme = CPTheme();
	const TStrongObjectPtr<UGSTN_CosmeticHostData> Sword = CPSword();
	const FGSTN_CosmeticLimits Limits;

	const FGSTN_CosmeticPlan Plan = FGSTN_CosmeticPlanning::BuildPlan(*Sword, *Theme, {}, Limits);
	TestEqual(TEXT("Pommel (grip fallback) and both guards placed, no tip piece"), Plan.Placements.Num(), 3);
	TestEqual(TEXT("Coverage 3 of 4 slots"), Plan.Coverage, 0.75f, 0.001f);
	TestTrue(TEXT("Left guard mirrored on Y"), Plan.Placements.Num() == 3 && Plan.Placements[2].Scale.Y < 0.0 && Plan.Placements[1].Scale.Y > 0.0);
	TestEqual(TEXT("One spine run"), Plan.SplineRuns.Num(), 1);
	TestEqual(TEXT("60 cm spine of 7 cm modules: 8 segments"), Plan.SplineRuns.Num() == 1 ? Plan.SplineRuns[0].Segments : 0, 8);
	TestTrue(TEXT("Tags gathered from the placed pieces"), Plan.Tags.HasTagExact(CPSpikyTag()));
	TestTrue(TEXT("Default palette"), Plan.Palette == Theme->Palette);

	FGSTN_CosmeticStyleChoice Dark;
	Dark.Channel = GSTNCosmeticTags::Style;
	Dark.Option = 1;
	const FGSTN_CosmeticPlan Styled = FGSTN_CosmeticPlanning::BuildPlan(*Sword, *Theme, { Dark }, Limits);
	TestEqual(TEXT("The dark style hides the tagged guards"), Styled.Placements.Num(), 1);
	TestEqual(TEXT("hidden by choice still counts as covered"), Styled.Coverage, 0.75f, 0.001f);
	TestTrue(TEXT("and overrides the palette"), Styled.Palette.Primary.Equals(FLinearColor::Black));
	TestEqual(TEXT("and doubles the emissive"), Styled.EmissiveScale, 2.f, 0.001f);
	FGSTN_CosmeticStyleChoice OutOfRange = Dark;
	OutOfRange.Option = 7;
	TestEqual(TEXT("An out of range option is the default"), FGSTN_CosmeticPlanning::BuildPlan(*Sword, *Theme, { OutOfRange }, Limits).Placements.Num(), 3);

	FGSTN_CosmeticLimits Tight;
	Tight.MaxPieces = 2;
	Tight.MaxSplines = 0;
	const FGSTN_CosmeticPlan Budget = FGSTN_CosmeticPlanning::BuildPlan(*Sword, *Theme, {}, Tight);
	TestEqual(TEXT("Budget keeps the first slots"), Budget.Placements.Num(), 2);
	TestEqual(TEXT("no spline over the budget"), Budget.SplineRuns.Num(), 0);
	TestTrue(TEXT("and says why"), Budget.Notes.Num() >= 2);

	TStrongObjectPtr<UGSTN_CosmeticThemeData> Colliding = CPTheme();
	Colliding->Pieces[0].Collision = EGSTN_CosmeticCollision::KeepMeshCollision;
	const FGSTN_CosmeticPlan Safe = FGSTN_CosmeticPlanning::BuildPlan(*Sword, *Colliding, {}, Limits);
	TestTrue(TEXT("A placed piece never collides, whatever the asset says"), Safe.Placements.Num() > 0 && Safe.Placements[0].Piece.Collision == EGSTN_CosmeticCollision::NoCollision);

	const FGSTN_CosmeticPlan Again = FGSTN_CosmeticPlanning::BuildPlan(*Sword, *Theme, {}, Limits);
	bool bSame = Again.Placements.Num() == Plan.Placements.Num();
	for (int32 Index = 0; bSame && Index < Plan.Placements.Num(); ++Index)
	{
		bSame = Again.Placements[Index].SlotIndex == Plan.Placements[Index].SlotIndex && Again.Placements[Index].Scale.Equals(Plan.Placements[Index].Scale, 0.0);
	}
	TestTrue(TEXT("Deterministic: the same inputs give the same plan"), bSame);
	TestTrue(TEXT("Tier of a sword with slots and a spline"), FGSTN_CosmeticPlanning::GetTier(*Sword) == EGSTN_CosmeticTier::Splines);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNCosmeticsCacheKeyTest, "GSTNCosmetics.Core.CacheKey", CosmeticsTestFlags)
bool FGSTNCosmeticsCacheKeyTest::RunTest(const FString& Parameters)
{
	const FPrimaryAssetId Host(GSTNCosmeticAssetTypes::Host(), TEXT("HOST_Sword"));
	const FPrimaryAssetId Theme(GSTNCosmeticAssetTypes::Theme(), TEXT("THEME_Gold"));
	FGSTN_CosmeticStyleChoice A;
	A.Channel = GSTNCosmeticTags::Style;
	A.Option = 1;
	FGSTN_CosmeticStyleChoice B;
	B.Channel = GSTNCosmeticTags::Theme;
	B.Option = 2;
	const FString Key = FGSTN_CosmeticPlanning::MakeCacheKey(Host, 1, Theme, 1, { A, B });
	TestEqual(TEXT("Order of the choices does not matter"), FGSTN_CosmeticPlanning::MakeCacheKey(Host, 1, Theme, 1, { B, A }), Key);
	TestTrue(TEXT("A new theme version rebuilds"), FGSTN_CosmeticPlanning::MakeCacheKey(Host, 1, Theme, 2, { A, B }) != Key);
	TestTrue(TEXT("A new item version rebuilds"), FGSTN_CosmeticPlanning::MakeCacheKey(Host, 2, Theme, 1, { A, B }) != Key);
	TestTrue(TEXT("Another style option rebuilds"), FGSTN_CosmeticPlanning::MakeCacheKey(Host, 1, Theme, 1, { A }) != Key);
	TestTrue(TEXT("Readable and carries the assembler version"), Key.Contains(TEXT("HOST_Sword")) && Key.EndsWith(FString::Printf(TEXT("a%d"), FGSTN_CosmeticPlanning::AssemblerVersion)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNCosmeticsValidateTest, "GSTNCosmetics.Core.Validate", CosmeticsTestFlags)
bool FGSTNCosmeticsValidateTest::RunTest(const FString& Parameters)
{
	TArray<FString> Errors;
	TArray<FString> Warnings;
	TStrongObjectPtr<UGSTN_CosmeticThemeData> Theme = CPTheme();
	// The test theme uses the bare root as identity: that alone is reported
	FGSTN_CosmeticPlanning::ValidateTheme(*Theme, Errors, Warnings);
	TestEqual(TEXT("Only the root theme tag is wrong"), Errors.Num(), 1);

	Errors.Reset();
	Theme->Pieces[0].Collision = EGSTN_CosmeticCollision::KeepMeshCollision;
	Theme->Pieces.Add(CPPiece(GSTNCosmeticTags::Slot_Guard, FVector(1)));
	Theme->LayerMaterial.Reset();
	FGSTN_CosmeticPlanning::ValidateTheme(*Theme, Errors, Warnings);
	TestTrue(TEXT("Collision on a piece is an error"), Errors.ContainsByPredicate([](const FString& Line) { return Line.Contains(TEXT("never collide")); }));
	TestTrue(TEXT("Two pieces for one class is an error"), Errors.ContainsByPredicate([](const FString& Line) { return Line.Contains(TEXT("second piece")); }));
	TestTrue(TEXT("No material is an error"), Errors.ContainsByPredicate([](const FString& Line) { return Line.Contains(TEXT("layer material")); }));

	Errors.Reset();
	Warnings.Reset();
	TStrongObjectPtr<UGSTN_CosmeticHostData> Sword = CPSword();
	FGSTN_CosmeticPlanning::ValidateHost(*Sword, FGSTN_CosmeticLimits(), Errors, Warnings);
	TestEqual(TEXT("The test sword is valid"), Errors.Num(), 0);
	// Copy first: adding an element of the array to itself asserts when the add reallocates.
	const auto FirstSlot = Sword->Slots[0];
	Sword->Slots.Add(FirstSlot);
	Sword->Splines.SetNum(5);
	FGSTN_CosmeticPlanning::ValidateHost(*Sword, FGSTN_CosmeticLimits(), Errors, Warnings);
	TestTrue(TEXT("A socket used twice is an error"), Errors.ContainsByPredicate([](const FString& Line) { return Line.Contains(TEXT("two slots")); }));
	TestTrue(TEXT("Five splines is over the limit"), Errors.ContainsByPredicate([](const FString& Line) { return Line.Contains(TEXT("the limit is 4")); }));
	return true;
}

#endif
