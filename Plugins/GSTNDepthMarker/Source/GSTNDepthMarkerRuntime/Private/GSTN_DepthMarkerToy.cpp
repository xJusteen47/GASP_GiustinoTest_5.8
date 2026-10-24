#include "GSTN_DepthMarkerToy.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GSTN_DepthMarkerSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

EGSTN_DepthLookRequest FGSTN_DepthToy::ParseLookRequest(const FString& Value)
{
	if (Value.Equals(TEXT("toy"), ESearchCase::IgnoreCase))
	{
		return EGSTN_DepthLookRequest::Toy;
	}
	if (Value.Equals(TEXT("classic"), ESearchCase::IgnoreCase))
	{
		return EGSTN_DepthLookRequest::Classic;
	}
	return EGSTN_DepthLookRequest::None;
}

int32 FGSTN_DepthToy::ColorRankOf(const TArray<FGSTN_DepthPlayerKey>& Players, const FGSTN_DepthPlayerKey& Self)
{
	TArray<FGSTN_DepthPlayerKey> Sorted = Players;
	Sorted.Sort([](const FGSTN_DepthPlayerKey& A, const FGSTN_DepthPlayerKey& B)
	{
		const int32 ByName = A.Name.Compare(B.Name, ESearchCase::IgnoreCase);
		return ByName != 0 ? ByName < 0 : A.UniqueId.Compare(B.UniqueId, ESearchCase::CaseSensitive) < 0;
	});
	return Sorted.IndexOfByPredicate([&Self](const FGSTN_DepthPlayerKey& Key)
	{
		return Key.UniqueId == Self.UniqueId && Key.Name.Equals(Self.Name, ESearchCase::IgnoreCase);
	});
}

FLinearColor FGSTN_DepthToy::ResolveColor(const TArray<FLinearColor>& PlayerPalette, int32 PlayerRank,
	const TMap<int32, FLinearColor>& TeamPalette, int32 TeamId, const FLinearColor* Override, const FLinearColor& Fallback)
{
	if (Override)
	{
		return *Override;
	}
	if (PlayerRank >= 0 && !PlayerPalette.IsEmpty())
	{
		return PlayerPalette[PlayerRank % PlayerPalette.Num()];
	}
	if (const FLinearColor* Team = TeamPalette.Find(TeamId))
	{
		return *Team;
	}
	return Fallback;
}

bool FGSTN_DepthToy::IsAirState(EGSTN_DepthMarkerState State)
{
	return State == EGSTN_DepthMarkerState::NoGround || State == EGSTN_DepthMarkerState::AboveReference || State == EGSTN_DepthMarkerState::Approach;
}

void FGSTN_DepthToy::BeamEnds(const FGSTN_DepthSubjectState& Subject, EGSTN_DepthLayer Layer, const FVector& Point, FVector& OutStart, FVector& OutEnd)
{
	// Above: from the head, so the beam starts off the character (same rule as the dashed line)
	OutStart = Layer == EGSTN_DepthLayer::Above ? Subject.Feet + Subject.Up.GetSafeNormal() * Subject.TopOffset : Subject.Feet;
	OutEnd = Point;
}

FTransform FGSTN_DepthToy::BeamTransform(const FVector& Start, const FVector& End, float ZMin, float ZMax, float ExtentXY, float WidthCm, bool bFlipTaper)
{
	const double Length = (End - Start).Size();
	const double Height = FMath::Max<double>(ZMax - ZMin, UE_SMALL_NUMBER);
	const double ZScale = Length / Height;
	const double XYScale = WidthCm / FMath::Max<double>(2.0 * ExtentXY, UE_SMALL_NUMBER);
	// Tip at +Z: the local Z axis runs from the base (End) to the tip (Start); flipped, the other way round
	const FVector ZAxis = bFlipTaper ? (End - Start).GetSafeNormal() : (Start - End).GetSafeNormal();
	const FVector Origin = (bFlipTaper ? Start : End) - ZAxis * (ZMin * ZScale);
	return FTransform(FRotationMatrix::MakeFromZ(ZAxis).ToQuat(), Origin, FVector(XYScale, XYScale, ZScale));
}

FQuat FGSTN_DepthToy::QuadRotation(const FVector& Normal, const FVector& UpHint)
{
	const FVector Z = Normal.GetSafeNormal();
	FVector X = FVector::CrossProduct(UpHint, Z);
	if (X.SizeSquared() < 1.0e-6)
	{
		const FVector Seed = FMath::Abs(Z.Z) < 0.9 ? FVector::UpVector : FVector::RightVector;
		X = FVector::CrossProduct(Seed, Z);
	}
	X.Normalize();
	return FMatrix(X, FVector::CrossProduct(Z, X), Z, FVector::ZeroVector).ToQuat();
}

void FGSTN_DepthToy::WriteColor(UPrimitiveComponent* Component, int32 Base, const FLinearColor& Color, float Opacity)
{
	if (Component)
	{
		Component->SetCustomPrimitiveDataVector4(Base + ColorOffset, FVector4(Color.R, Color.G, Color.B, Color.A));
		Component->SetCustomPrimitiveDataFloat(Base + OpacityOffset, Opacity);
	}
}

void FGSTN_DepthToy::WriteBeamEnds(UPrimitiveComponent* Component, int32 Base, const FVector& Start, const FVector& End)
{
	if (Component)
	{
		Component->SetCustomPrimitiveDataVector4(Base + BeamStartOffset, FVector4(Start.X, Start.Y, Start.Z, 0.0));
		Component->SetCustomPrimitiveDataVector4(Base + BeamEndOffset, FVector4(End.X, End.Y, End.Z, 0.0));
	}
}

UStaticMeshComponent* UGSTN_DepthPresenter_Toy::EnsureMesh(AActor* Holder, UStaticMesh* DefaultMesh)
{
	if (MeshComponent)
	{
		return MeshComponent;
	}
	// No material, no draw: the plugin ships no content, a style names it
	UMaterialInterface* Material = !MaterialOverride.IsNull() ? MaterialOverride.LoadSynchronous() : nullptr;
	UStaticMesh* Mesh = !MeshOverride.IsNull() ? MeshOverride.LoadSynchronous() : DefaultMesh;
	if (!Holder || !Material || !Mesh)
	{
		return nullptr;
	}
	MeshComponent = NewObject<UStaticMeshComponent>(Holder);
	MeshComponent->SetStaticMesh(Mesh);
	MeshComponent->SetMaterial(0, Material);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);
	MeshComponent->SetCanEverAffectNavigation(false);
	MeshComponent->SetupAttachment(Holder->GetRootComponent());
	MeshComponent->RegisterComponent();
	return MeshComponent;
}

void UGSTN_DepthPresenter_Toy::OnMarkerReleased_Implementation()
{
	if (MeshComponent)
	{
		MeshComponent->SetVisibility(false);
	}
}

UGSTN_DepthPresenter_Cone::UGSTN_DepthPresenter_Cone()
{
	ActiveStates = { EGSTN_DepthMarkerState::Ground, EGSTN_DepthMarkerState::EdgeHold, EGSTN_DepthMarkerState::NoGround };
}

void UGSTN_DepthPresenter_Cone::OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder)
{
	const UGSTN_DepthMarkerSettings& Settings = UGSTN_DepthMarkerSettings::Get();
	FVector Start, End;
	FGSTN_DepthToy::BeamEnds(View.Subject, Layer, View.GetTarget(Layer).Point, Start, End);
	if ((End - Start).SizeSquared() < 1.0)
	{
		OnMarkerReleased();
		return;
	}
	UStaticMeshComponent* Component = EnsureMesh(Holder, Settings.ToyBeamMesh.LoadSynchronous());
	if (!Component || !Component->GetStaticMesh())
	{
		return;
	}
	const FBoxSphereBounds Bounds = Component->GetStaticMesh()->GetBounds();
	const float Width = View.Subject.FootRadius * BaseWidthMultiplier * View.RadiusScale;
	Component->SetWorldTransform(FGSTN_DepthToy::BeamTransform(Start, End, static_cast<float>(Bounds.Origin.Z - Bounds.BoxExtent.Z),
		static_cast<float>(Bounds.Origin.Z + Bounds.BoxExtent.Z), static_cast<float>(Bounds.BoxExtent.X), Width, bFlipTaper));
	FGSTN_DepthToy::WriteColor(Component, Settings.ToyCustomPrimitiveDataBase, View.Tint, GetAlpha(View));
	FGSTN_DepthToy::WriteBeamEnds(Component, Settings.ToyCustomPrimitiveDataBase, Start, End);
	Component->SetVisibility(true);
}

UGSTN_DepthPresenter_FogPuff::UGSTN_DepthPresenter_FogPuff()
{
	// Air markers only: on a surface the beam lands on the decal
	ActiveStates = { EGSTN_DepthMarkerState::NoGround };
}

void UGSTN_DepthPresenter_FogPuff::OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder)
{
	const UGSTN_DepthMarkerSettings& Settings = UGSTN_DepthMarkerSettings::Get();
	UStaticMeshComponent* Component = EnsureMesh(Holder, Settings.ToyPuffMesh.LoadSynchronous());
	if (!Component || !Component->GetStaticMesh())
	{
		return;
	}
	const FGSTN_DepthTarget& Target = View.GetTarget(Layer);
	FVector Normal = Target.PresentNormal;
	if (bFaceCamera)
	{
		APlayerController* Controller = nullptr;
		if (const APawn* Pawn = Cast<APawn>(View.Owner.Get()))
		{
			Controller = Cast<APlayerController>(Pawn->GetController());
		}
		if ((!Controller || !Controller->IsLocalController()) && Holder && Holder->GetWorld())
		{
			Controller = Holder->GetWorld()->GetFirstPlayerController();
		}
		if (Controller && Controller->PlayerCameraManager)
		{
			Normal = Controller->PlayerCameraManager->GetCameraLocation() - Target.Point;
		}
	}
	const FBoxSphereBounds Bounds = Component->GetStaticMesh()->GetBounds();
	const double Side = View.Subject.FootRadius * SizeMultiplier * View.RadiusScale;
	const double Scale = Side / FMath::Max(2.0 * Bounds.BoxExtent.X, UE_SMALL_NUMBER);
	Component->SetWorldTransform(FTransform(FGSTN_DepthToy::QuadRotation(Normal, View.Subject.Up), Target.Point, FVector(Scale)));
	FGSTN_DepthToy::WriteColor(Component, Settings.ToyCustomPrimitiveDataBase, View.Tint, GetAlpha(View));
	Component->SetVisibility(true);
}
