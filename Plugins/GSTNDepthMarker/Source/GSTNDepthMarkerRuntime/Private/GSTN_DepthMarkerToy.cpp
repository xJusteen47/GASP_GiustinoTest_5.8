#include "GSTN_DepthMarkerToy.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GSTN_DepthMarkerSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
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

float FGSTN_DepthToy::HeightScale(const FGSTN_DepthToyParams& Params, float HeightCm)
{
	const float Span = FMath::Max(Params.FarHeightCm - Params.NearHeightCm, 1.f);
	const float T = FMath::Clamp((HeightCm - Params.NearHeightCm) / Span, 0.f, 1.f);
	return FMath::Lerp(Params.NearScale, Params.FarScale, T * T * (3.f - 2.f * T));
}

float FGSTN_DepthToy::LandingRadius(const FGSTN_DepthToyParams& Params, float FootRadius, float HeightCm)
{
	return FMath::Max(FootRadius, 1.f) * Params.LandingRadiusFootMultiplier * HeightScale(Params, HeightCm);
}

float FGSTN_DepthToy::Softness(const FGSTN_DepthToyParams& Params, float HeightCm)
{
	const float Span = FMath::Max(Params.FarHeightCm - Params.NearHeightCm, 1.f);
	const float T = FMath::Clamp((HeightCm - Params.NearHeightCm) / Span, 0.f, 1.f);
	return FMath::Lerp(Params.SoftnessNear, Params.SoftnessFar, T * T * (3.f - 2.f * T));
}

float FGSTN_DepthToy::TopRadius(const FGSTN_DepthToyParams& Params, float FootRadius, float LandingRadius)
{
	return FMath::Min(FMath::Max(FootRadius, 1.f) * Params.BeamTopFootMultiplier, LandingRadius);
}

float FGSTN_DepthToy::SizeFade(const FGSTN_DepthToyParams& Params, float LengthCm)
{
	const float T = FMath::Clamp((LengthCm - Params.MinHeightCm) / FMath::Max(Params.FadeBandCm, 1.f), 0.f, 1.f);
	return T * T * (3.f - 2.f * T);
}

float FGSTN_DepthToy::SlopeFade(const FGSTN_DepthToyParams& Params, float NormalDotUp)
{
	const float Span = FMath::Max(Params.SlopeFadeStartDot - Params.SlopeFadeEndDot, UE_KINDA_SMALL_NUMBER);
	const float T = FMath::Clamp((NormalDotUp - Params.SlopeFadeEndDot) / Span, 0.f, 1.f);
	return T * T * (3.f - 2.f * T);
}

float FGSTN_DepthToy::DecalHalfDepth(const FGSTN_DepthToyParams& Params, float Radius, float NormalDotUp)
{
	const float Cos = FMath::Clamp(NormalDotUp, 0.05f, 1.f);
	const float Tan = FMath::Sqrt(FMath::Max(1.f - Cos * Cos, 0.f)) / Cos;
	return FMath::Clamp(Params.DecalDepthCm + Radius * Tan, Params.DecalDepthCm, FMath::Max(Params.DecalMaxDepthCm, Params.DecalDepthCm));
}

float FGSTN_DepthToy::ClearanceRadius(const FGSTN_DepthToyParams& Params, float TopRadius, float LandingRadius)
{
	return FMath::Lerp(TopRadius, LandingRadius, Params.ClearanceRadiusMix);
}

bool FGSTN_DepthToy::ClearanceSweep(const FVector& Feet, const FVector& Up, const FVector& Point, float Radius, FVector& OutStart, FVector& OutEnd)
{
	const FVector UnitUp = Up.GetSafeNormal();
	// The sphere ends a little above a floor at Point (10 % and 2 cm of margin: a gentle slope under the column is not a hit)
	OutEnd = Point + UnitUp * (Radius * 1.1f + 2.f);
	OutStart = Feet;
	return FVector::DotProduct(OutStart - OutEnd, UnitUp) > 1.f;
}

float FGSTN_DepthToy::SmoothTowards(float Current, float Target, float DeltaSeconds, float Tau)
{
	if (Tau <= UE_KINDA_SMALL_NUMBER)
	{
		return Target;
	}
	return FMath::Lerp(Current, Target, 1.f - FMath::Exp(-FMath::Max(DeltaSeconds, 0.f) / Tau));
}

float FGSTN_DepthToy::MoveClearance(float Current, float Target, float DeltaSeconds, float OutSeconds, float InSeconds)
{
	const bool bRise = Target > Current;
	const float Seconds = bRise ? InSeconds : OutSeconds;
	if (Seconds <= UE_KINDA_SMALL_NUMBER)
	{
		return Target;
	}
	const float Step = FMath::Max(DeltaSeconds, 0.f) / Seconds;
	return bRise ? FMath::Min(Current + Step, Target) : FMath::Max(Current - Step, Target);
}

float FGSTN_DepthToy::ApexExtension(float LengthCm, float TopRadius, float BaseRadius)
{
	return BaseRadius > TopRadius + UE_KINDA_SMALL_NUMBER ? FMath::Max(TopRadius, 0.f) * LengthCm / (BaseRadius - TopRadius) : 0.f;
}

FGSTN_DepthToy::FLook FGSTN_DepthToy::ResolveLook(const FGSTN_DepthMarkerView& View, EGSTN_DepthLayer Layer)
{
	const FGSTN_DepthToyParams& Params = View.Toy.Params;
	const FGSTN_DepthTarget& Target = View.GetTarget(Layer);
	const float FootRadius = View.Subject.FootRadius;
	const float Height = FMath::Max(Target.Height, 0.f);

	FLook Look;
	const bool bSmoothed = Layer == EGSTN_DepthLayer::Below && View.Toy.LandingRadius > 0.f;
	Look.Point = bSmoothed ? View.Toy.BeamEnd : Target.Point;
	Look.Radius = bSmoothed ? View.Toy.LandingRadius : LandingRadius(Params, FootRadius, Height);
	Look.BeamClearance = bSmoothed ? View.Toy.BeamClearance : 1.f;
	Look.TopRadius = TopRadius(Params, FootRadius, Look.Radius);
	Look.Softness = Softness(Params, Height);
	FVector Start, End;
	BeamEnds(View.Subject, Layer, Target.Point, Start, End);
	Look.SizeFade = SizeFade(Params, static_cast<float>((End - Start).Size()));
	Look.SlopeFade = SlopeFade(Params, static_cast<float>(FVector::DotProduct(Target.Normal.GetSafeNormal(), View.Subject.Up.GetSafeNormal())));
	return Look;
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
	FGSTN_DepthToy::FLook Look;
	FVector Start, End;
	float Opacity = GetAlpha(View);
	if (View.Toy.bActive)
	{
		// Toy: a soft column that ends where the decal is, never wider than it, fading with height and when geometry hugs it
		Look = FGSTN_DepthToy::ResolveLook(View, Layer);
		FGSTN_DepthToy::BeamEnds(View.Subject, Layer, Look.Point, Start, End);
		Opacity *= Look.SizeFade * Look.BeamClearance * View.Toy.Params.BeamOpacity;
		if (Opacity < 0.002f)
		{
			OnMarkerReleased();
			return;
		}
	}
	else
	{
		FGSTN_DepthToy::BeamEnds(View.Subject, Layer, View.GetTarget(Layer).Point, Start, End);
	}
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
	const float Width = View.Toy.bActive ? Look.Radius * 2.f : View.Subject.FootRadius * BaseWidthMultiplier * View.RadiusScale;
	// Toy: the cone continues above the character up to its apex, so its radius at the character is the top radius (the material cuts it there)
	FVector MeshStart = Start;
	if (View.Toy.bActive)
	{
		const float Length = static_cast<float>((End - Start).Size());
		MeshStart = Start - (End - Start).GetSafeNormal() * FGSTN_DepthToy::ApexExtension(Length, Look.TopRadius, Look.Radius);
	}
	Component->SetWorldTransform(FGSTN_DepthToy::BeamTransform(MeshStart, End, static_cast<float>(Bounds.Origin.Z - Bounds.BoxExtent.Z),
		static_cast<float>(Bounds.Origin.Z + Bounds.BoxExtent.Z), static_cast<float>(Bounds.BoxExtent.X), Width, bFlipTaper));
	FGSTN_DepthToy::WriteColor(Component, Settings.ToyCustomPrimitiveDataBase, View.Tint, Opacity);
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
	double Side = View.Subject.FootRadius * SizeMultiplier * View.RadiusScale;
	FVector Centre = Target.Point;
	float Opacity = GetAlpha(View);
	if (View.Toy.bActive)
	{
		// The same base radius as the beam that lands here; nothing under the minimum height
		const FGSTN_DepthToy::FLook Look = FGSTN_DepthToy::ResolveLook(View, Layer);
		Side = Look.Radius * 2.0 * PuffSpan;
		Centre = Look.Point;
		Opacity *= Look.SizeFade;
		if (Opacity < 0.002f)
		{
			OnMarkerReleased();
			return;
		}
	}
	const double Scale = Side / FMath::Max(2.0 * Bounds.BoxExtent.X, UE_SMALL_NUMBER);
	Component->SetWorldTransform(FTransform(FGSTN_DepthToy::QuadRotation(Normal, View.Subject.Up), Centre, FVector(Scale)));
	FGSTN_DepthToy::WriteColor(Component, Settings.ToyCustomPrimitiveDataBase, View.Tint, Opacity);
	Component->SetVisibility(true);
}

void UGSTN_DepthPresenter_ToyDecal::OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder)
{
	if (!View.Toy.bActive)
	{
		Super::OnMarkerUpdated_Implementation(View, Holder);
		return;
	}
	UMaterialInterface* DecalMaterial = !MaterialOverride.IsNull() ? MaterialOverride.LoadSynchronous()
		: UGSTN_DepthMarkerSettings::Get().DefaultDecalMaterial.LoadSynchronous();
	if (!Holder || !DecalMaterial)
	{
		return;
	}
	const FGSTN_DepthTarget& Target = View.GetTarget(Layer);
	const FGSTN_DepthToy::FLook Look = FGSTN_DepthToy::ResolveLook(View, Layer);
	const float Alpha = GetAlpha(View) * Look.SizeFade * Look.SlopeFade;
	if (Alpha < 0.003f)
	{
		OnMarkerReleased();
		return;
	}
	if (!Decal)
	{
		Decal = NewObject<UDecalComponent>(Holder);
		Decal->SetupAttachment(Holder->GetRootComponent());
		Decal->RegisterComponent();
		// A landing spot stays visible from far away; its box is thin, so the screen-size cull would only ever remove it
		Decal->SetFadeScreenSize(0.f);
	}
	if (!Material)
	{
		Material = UMaterialInstanceDynamic::Create(DecalMaterial, this);
	}
	if (Material)
	{
		const UGSTN_DepthMarkerSettings& Settings = UGSTN_DepthMarkerSettings::Get();
		Material->SetVectorParameterValue(Settings.TintParameterName, View.Tint);
		Material->SetScalarParameterValue(Settings.OpacityParameterName, Alpha);
		Material->SetScalarParameterValue(TEXT("Softness"), Look.Softness);
		// The material keeps the spot off surfaces that do not face Up (walls beside it): Up is the subject's, never world Z
		const FVector UpDirection = View.Subject.Up.GetSafeNormal();
		Material->SetVectorParameterValue(TEXT("UpDirection"), FLinearColor(UpDirection.X, UpDirection.Y, UpDirection.Z, 0.f));
		Decal->SetDecalMaterial(Material);
	}

	// Straight along -Up and thin: the spot lies on the surface under the column and never smears onto walls behind it. It stays centred
	// on the column (the raw target point); the smoothing is for the beam end only
	const FVector Up = View.Subject.Up.GetSafeNormal();
	const float NormalDotUp = static_cast<float>(FVector::DotProduct(Target.Normal.GetSafeNormal(), Up));
	const float Depth = FGSTN_DepthToy::DecalHalfDepth(View.Toy.Params, Look.Radius, NormalDotUp);
	// Any perpendicular of Up as the roll: stable from frame to frame
	const FVector Seed = FMath::Abs(Up.Z) < 0.9 ? FVector::UpVector : FVector::RightVector;
	Decal->SetWorldLocationAndRotation(Target.Point, FRotationMatrix::MakeFromXZ(-Up, Seed).Rotator());
	Decal->DecalSize = FVector(Depth, Look.Radius, Look.Radius);
	Decal->SetVisibility(true);
}
