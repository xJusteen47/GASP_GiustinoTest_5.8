// Tests of the toy look (GSTNDepthMarker.Runtime.Toy.*): colour order and assignment, custom primitive data, fog puff on air only, beam geometry.

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GSTN_DepthMarkerStyle.h"
#include "GSTN_DepthMarkerToy.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNDepthMarkerToyTest
{
	constexpr EAutomationTestFlags ToyTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	static const FLinearColor Red(0.9f, 0.1f, 0.08f);
	static const FLinearColor Blue(0.12f, 0.38f, 1.f);
	static const FLinearColor Yellow(1.f, 0.82f, 0.05f);
	static const FLinearColor Green(0.15f, 0.8f, 0.22f);

	static TArray<FLinearColor> Palette() { return { Red, Blue, Yellow, Green }; }

	static const TArray<FVector>& UpVectors()
	{
		static const TArray<FVector> Ups = { FVector::UpVector, -FVector::UpVector, FVector::ForwardVector,
			FVector(1.0, 1.0, 1.0).GetSafeNormal(), FVector(-0.3, 0.8, -0.5).GetSafeNormal() };
		return Ups;
	}
}

#define GSTN_DMT_TEST(Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNDepthMarkerToyTest_##Name, "GSTNDepthMarker.Runtime.Toy." #Name, GSTNDepthMarkerToyTest::ToyTestFlags) \
	bool FGSTNDepthMarkerToyTest_##Name::RunTest(const FString& Parameters)

using namespace GSTNDepthMarkerToyTest;

GSTN_DMT_TEST(ColorRankIsAlphabeticalAndStable)
{
	const TArray<FGSTN_DepthPlayerKey> Players = { { TEXT("Zed"), TEXT("id3") }, { TEXT("anna"), TEXT("id1") }, { TEXT("Bob"), TEXT("id2") } };
	TestEqual(TEXT("anna is first whatever the case"), FGSTN_DepthToy::ColorRankOf(Players, Players[1]), 0);
	TestEqual(TEXT("Bob second"), FGSTN_DepthToy::ColorRankOf(Players, Players[2]), 1);
	TestEqual(TEXT("Zed third, although listed first (join order is irrelevant)"), FGSTN_DepthToy::ColorRankOf(Players, Players[0]), 2);

	const TArray<FGSTN_DepthPlayerKey> Twins = { { TEXT("Sam"), TEXT("b") }, { TEXT("SAM"), TEXT("a") } };
	TestEqual(TEXT("equal names (ignoring case) break the tie by the unique id"), FGSTN_DepthToy::ColorRankOf(Twins, Twins[1]), 0);
	TestEqual(TEXT("and the other is second"), FGSTN_DepthToy::ColorRankOf(Twins, Twins[0]), 1);
	TestEqual(TEXT("a player that is not listed has no rank"), FGSTN_DepthToy::ColorRankOf(Players, { TEXT("Nobody"), TEXT("x") }), static_cast<int32>(INDEX_NONE));
	return true;
}

GSTN_DMT_TEST(TwoPlayersGetRedAndBlue)
{
	const TArray<FGSTN_DepthPlayerKey> Players = { { TEXT("Mario"), TEXT("1") }, { TEXT("Luigi"), TEXT("2") } };
	const TMap<int32, FLinearColor> NoTeams;
	const FLinearColor Fallback(0.5f, 0.5f, 0.5f);
	const FLinearColor First = FGSTN_DepthToy::ResolveColor(Palette(), FGSTN_DepthToy::ColorRankOf(Players, Players[1]), NoTeams, 0, nullptr, Fallback);
	const FLinearColor Second = FGSTN_DepthToy::ResolveColor(Palette(), FGSTN_DepthToy::ColorRankOf(Players, Players[0]), NoTeams, 0, nullptr, Fallback);
	TestTrue(TEXT("alphabetically first (Luigi) is red"), First.Equals(Red));
	TestTrue(TEXT("alphabetically second (Mario) is blue"), Second.Equals(Blue));
	TestFalse(TEXT("they read differently"), First.Equals(Second));

	const FLinearColor Pink(1.f, 0.4f, 0.7f);
	TestTrue(TEXT("a component override wins"), FGSTN_DepthToy::ResolveColor(Palette(), 0, NoTeams, 0, &Pink, Fallback).Equals(Pink));
	TestTrue(TEXT("ranks wrap on a short palette"), FGSTN_DepthToy::ResolveColor(Palette(), 5, NoTeams, 0, nullptr, Fallback).Equals(Blue));

	TMap<int32, FLinearColor> Teams;
	Teams.Add(7, Green);
	TestTrue(TEXT("no player: the team colour"), FGSTN_DepthToy::ResolveColor(Palette(), INDEX_NONE, Teams, 7, nullptr, Fallback).Equals(Green));
	TestTrue(TEXT("no player, no team: the fallback"), FGSTN_DepthToy::ResolveColor(Palette(), INDEX_NONE, Teams, 8, nullptr, Fallback).Equals(Fallback));
	return true;
}

GSTN_DMT_TEST(LookRequestParses)
{
	TestTrue(TEXT("toy"), FGSTN_DepthToy::ParseLookRequest(TEXT("toy")) == EGSTN_DepthLookRequest::Toy);
	TestTrue(TEXT("classic, any case"), FGSTN_DepthToy::ParseLookRequest(TEXT("Classic")) == EGSTN_DepthLookRequest::Classic);
	TestTrue(TEXT("empty is the settings"), FGSTN_DepthToy::ParseLookRequest(TEXT("")) == EGSTN_DepthLookRequest::None);
	TestTrue(TEXT("unknown is the settings"), FGSTN_DepthToy::ParseLookRequest(TEXT("fancy")) == EGSTN_DepthLookRequest::None);
	return true;
}

GSTN_DMT_TEST(ColorWrittenToCustomPrimitiveData)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(GetTransientPackage());
	FGSTN_DepthToy::WriteColor(Component, 3, Blue, 0.25f);
	FGSTN_DepthToy::WriteBeamEnds(Component, 3, FVector(1, 2, 3), FVector(4, 5, 6));
	const TArray<float>& Data = Component->GetCustomPrimitiveData().Data;
	if (!TestTrue(TEXT("enough floats"), Data.Num() >= 3 + FGSTN_DepthToy::NumFloats))
	{
		return false;
	}
	TestTrue(TEXT("colour at the base index"), FMath::IsNearlyEqual(Data[3], Blue.R) && FMath::IsNearlyEqual(Data[4], Blue.G) && FMath::IsNearlyEqual(Data[5], Blue.B));
	TestTrue(TEXT("opacity after the colour"), FMath::IsNearlyEqual(Data[3 + FGSTN_DepthToy::OpacityOffset], 0.25f));
	TestTrue(TEXT("beam start"), FMath::IsNearlyEqual(Data[3 + FGSTN_DepthToy::BeamStartOffset + 2], 3.f));
	TestTrue(TEXT("beam end"), FMath::IsNearlyEqual(Data[3 + FGSTN_DepthToy::BeamEndOffset], 4.f));

	// Two players, two components: each keeps its own colour (no shared material instance)
	UStaticMeshComponent* Other = NewObject<UStaticMeshComponent>(GetTransientPackage());
	FGSTN_DepthToy::WriteColor(Other, 3, Red, 1.f);
	TestTrue(TEXT("independent per component"), FMath::IsNearlyEqual(Component->GetCustomPrimitiveData().Data[3], Blue.R)
		&& FMath::IsNearlyEqual(Other->GetCustomPrimitiveData().Data[3], Red.R));
	return true;
}

GSTN_DMT_TEST(FogPuffOnlyOnAirMarkers)
{
	TestTrue(TEXT("no ground is air"), FGSTN_DepthToy::IsAirState(EGSTN_DepthMarkerState::NoGround));
	TestTrue(TEXT("above is air"), FGSTN_DepthToy::IsAirState(EGSTN_DepthMarkerState::AboveReference));
	TestTrue(TEXT("approach is air"), FGSTN_DepthToy::IsAirState(EGSTN_DepthMarkerState::Approach));
	TestFalse(TEXT("ground is not"), FGSTN_DepthToy::IsAirState(EGSTN_DepthMarkerState::Ground));
	TestFalse(TEXT("edge hold is not"), FGSTN_DepthToy::IsAirState(EGSTN_DepthMarkerState::EdgeHold));

	UGSTN_DepthMarkerStyle* Style = NewObject<UGSTN_DepthMarkerStyle>(GetTransientPackage());
	UGSTN_DepthMarkerStyle* ApproachStyle = NewObject<UGSTN_DepthMarkerStyle>(GetTransientPackage());
	Style->PopulateToyPresenters();
	ApproachStyle->PopulateToyApproachPresenters();
	int32 Puffs = 0;
	for (const UGSTN_DepthMarkerStyle* Candidate : { Style, ApproachStyle })
	{
		for (const UGSTN_DepthMarkerPresenter* Presenter : Candidate->Presenters)
		{
			if (!Presenter->IsA<UGSTN_DepthPresenter_FogPuff>())
			{
				continue;
			}
			++Puffs;
			TestFalse(TEXT("a puff has states"), Presenter->ActiveStates.IsEmpty());
			for (const EGSTN_DepthMarkerState State : Presenter->ActiveStates)
			{
				TestTrue(TEXT("a puff draws in air states only"), FGSTN_DepthToy::IsAirState(State));
			}
			// And through the real gate: a ground view does not draw it, an air view of its own layer does
			FGSTN_DepthMarkerView View;
			FGSTN_DepthTarget& Target = const_cast<FGSTN_DepthTarget&>(View.GetTarget(Presenter->Layer));
			Target.Kind = EGSTN_DepthTargetKind::Surface;
			Target.State = EGSTN_DepthMarkerState::Ground;
			View.bBelowVisible = View.bAboveVisible = View.bApproachVisible = true;
			TestFalse(TEXT("ground view: no puff"), Presenter->ShouldDraw(View));
			Target.State = Presenter->ActiveStates[0];
			TestTrue(TEXT("air view: puff"), Presenter->ShouldDraw(View));
		}
	}
	TestTrue(TEXT("the toy styles contain puffs"), Puffs >= 3);

	// The ground beam and decal exist; the decal is a ground-only element
	bool bDecal = false;
	for (const UGSTN_DepthMarkerPresenter* Presenter : Style->Presenters)
	{
		bDecal |= Presenter->IsA<UGSTN_DepthPresenter_Decal>();
	}
	TestTrue(TEXT("the toy style has the decal"), bDecal);
	TestTrue(TEXT("the toy style takes the player palette"), Style->bUsePlayerPalette && ApproachStyle->bUsePlayerPalette);
	return true;
}

GSTN_DMT_TEST(BeamRunsAlongMinusUp)
{
	for (const FVector& Up : UpVectors())
	{
		FGSTN_DepthSubjectState Subject;
		Subject.Feet = FVector(120, -40, 75);
		Subject.Up = Up;
		Subject.TopOffset = 180.f;
		const FVector Ground = Subject.Feet - Up * 300.0;

		FVector Start, End;
		FGSTN_DepthToy::BeamEnds(Subject, EGSTN_DepthLayer::Below, Ground, Start, End);
		TestTrue(TEXT("below: starts at the feet"), Start.Equals(Subject.Feet, 0.01));
		TestTrue(TEXT("below: ends at the point"), End.Equals(Ground, 0.01));
		TestTrue(TEXT("below: the axis is -Up"), (End - Start).GetSafeNormal().Equals(-Up, 1.0e-4));

		// Any cone pivot works: tip (local +Z top) at the feet, base (local -Z bottom) at the point
		const float Pivots[][2] = { { 0.f, 100.f }, { -50.f, 50.f }, { -100.f, 0.f } };
		for (const float (&Range)[2] : Pivots)
		{
			const FTransform Beam = FGSTN_DepthToy::BeamTransform(Start, End, Range[0], Range[1], 50.f, 72.f, false);
			TestTrue(TEXT("tip at the character"), Beam.TransformPosition(FVector(0, 0, Range[1])).Equals(Start, 0.01));
			TestTrue(TEXT("base at the marker point"), Beam.TransformPosition(FVector(0, 0, Range[0])).Equals(End, 0.01));
			TestTrue(TEXT("base diameter"), FMath::IsNearlyEqual(static_cast<float>(Beam.GetScale3D().X * 100.0), 72.f, 0.01f));
			const FTransform Flipped = FGSTN_DepthToy::BeamTransform(Start, End, Range[0], Range[1], 50.f, 72.f, true);
			TestTrue(TEXT("flipped taper: tip (local -Z) at the character"), Flipped.TransformPosition(FVector(0, 0, Range[0])).Equals(Start, 0.01));
		}

		const FVector Sky = Subject.Feet + Up * 500.0;
		FGSTN_DepthToy::BeamEnds(Subject, EGSTN_DepthLayer::Above, Sky, Start, End);
		TestTrue(TEXT("above: starts at the head"), Start.Equals(Subject.Feet + Up * 180.0, 0.01));
		TestTrue(TEXT("above: the axis is +Up"), (End - Start).GetSafeNormal().Equals(Up, 1.0e-4));

		const FQuat Quad = FGSTN_DepthToy::QuadRotation(Up, FVector::ForwardVector);
		TestTrue(TEXT("a quad's normal is the asked one for any Up"), Quad.RotateVector(FVector::UpVector).Equals(Up, 1.0e-4));
	}
	return true;
}

GSTN_DMT_TEST(ConePresenterWritesCustomPrimitiveData)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNDepthMarkerToyTestWorld"));
	World->AddToRoot();
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	AActor* Holder = World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();

	UGSTN_DepthPresenter_Cone* Cone = NewObject<UGSTN_DepthPresenter_Cone>(GetTransientPackage());
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	Cone->MeshOverride = Mesh;
	FGSTN_DepthMarkerView View;
	View.Subject.Feet = FVector(0, 0, 100);
	View.Below.Kind = EGSTN_DepthTargetKind::Surface;
	View.Below.State = EGSTN_DepthMarkerState::Ground;
	View.Below.Point = FVector::ZeroVector;
	View.bBelowVisible = true;
	View.Tint = Blue;
	View.BelowAlpha = 0.5f;

	Cone->OnMarkerUpdated(View, Holder);
	TestNull(TEXT("no material, nothing is drawn"), Cone->GetMeshComponent());

	bool bOk = true;
	if (Mesh)
	{
		Cone->MaterialOverride = TSoftObjectPtr<UMaterialInterface>(UMaterial::GetDefaultMaterial(MD_Surface));
		Cone->OnMarkerUpdated(View, Holder);
		if (TestNotNull(TEXT("material and mesh: the beam is created"), Cone->GetMeshComponent()))
		{
			const TArray<float>& Data = Cone->GetMeshComponent()->GetCustomPrimitiveData().Data;
			TestTrue(TEXT("colour and opacity written, endpoints too"), Data.Num() >= FGSTN_DepthToy::NumFloats
				&& FMath::IsNearlyEqual(Data[2], Blue.B) && FMath::IsNearlyEqual(Data[FGSTN_DepthToy::OpacityOffset], 0.5f)
				&& FMath::IsNearlyEqual(Data[FGSTN_DepthToy::BeamStartOffset + 2], 100.f) && FMath::IsNearlyEqual(Data[FGSTN_DepthToy::BeamEndOffset + 2], 0.f));
			TestTrue(TEXT("visible"), Cone->GetMeshComponent()->IsVisible());
		}
		else
		{
			bOk = false;
		}
	}
	else
	{
		AddWarning(TEXT("Engine cone mesh not available: the presenter half of the test was skipped"));
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return bOk;
}

#endif // WITH_DEV_AUTOMATION_TESTS
