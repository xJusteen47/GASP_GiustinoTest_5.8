#include "GSTN_DepthMarkerPresenter.h"

#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GSTN_DepthMarkerSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace GSTNDepthMarkerPresenterPrivate
{
	/** A very small scale hides an instance without removing it from the fixed pool */
	static FTransform HiddenInstance()
	{
		return FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.0001));
	}

	static FVector AnyPerpendicular(const FVector& Normal)
	{
		const FVector Seed = FMath::Abs(Normal.Z) < 0.9 ? FVector::UpVector : FVector::RightVector;
		return FVector::CrossProduct(Normal, Seed).GetSafeNormal();
	}
}

bool UGSTN_DepthMarkerPresenter::ShouldDraw(const FGSTN_DepthMarkerView& View) const
{
	const FGSTN_DepthTarget& Target = View.GetTarget(Layer);
	if (!Target.IsValid() || !View.IsVisible(Layer))
	{
		return false;
	}
	return ActiveStates.IsEmpty() || ActiveStates.Contains(Target.State);
}

void UGSTN_DepthPresenter_Primitive::ApplyLook(const FGSTN_DepthMarkerView& View)
{
	const UGSTN_DepthMarkerSettings& Settings = UGSTN_DepthMarkerSettings::Get();
	if (!Material)
	{
		// Soft until the first draw: a project that never shows a marker never loads the content
		UMaterialInterface* Parent = !MaterialOverride.IsNull() ? MaterialOverride.LoadSynchronous() : Settings.DefaultSegmentMaterial.LoadSynchronous();
		if (Parent)
		{
			Material = UMaterialInstanceDynamic::Create(Parent, this);
		}
	}
	if (Material)
	{
		Material->SetVectorParameterValue(Settings.TintParameterName, View.Tint);
		Material->SetScalarParameterValue(Settings.OpacityParameterName, GetAlpha(View));
	}
}

UGSTN_DepthPresenter_Decal::UGSTN_DepthPresenter_Decal()
{
	ActiveStates = { EGSTN_DepthMarkerState::Ground, EGSTN_DepthMarkerState::EdgeHold };
}

void UGSTN_DepthPresenter_Decal::OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder)
{
	// No default decal content ships: the decal draws only when a style or the settings name a decal material
	UMaterialInterface* DecalMaterial = !MaterialOverride.IsNull() ? MaterialOverride.LoadSynchronous()
		: UGSTN_DepthMarkerSettings::Get().DefaultDecalMaterial.LoadSynchronous();
	if (!Holder || !DecalMaterial)
	{
		return;
	}
	const FGSTN_DepthTarget& Target = View.GetTarget(Layer);
	if (!Decal)
	{
		Decal = NewObject<UDecalComponent>(Holder);
		Decal->SetupAttachment(Holder->GetRootComponent());
		Decal->RegisterComponent();
	}
	if (!Material)
	{
		Material = UMaterialInstanceDynamic::Create(DecalMaterial, this);
	}
	ApplyLook(View);
	if (Material)
	{
		Decal->SetDecalMaterial(Material);
	}

	// A decal projects along its local X: aim it along Down so it clips to geometry and follows slopes
	const FVector Down = -View.Subject.Up.GetSafeNormal();
	const float Radius = View.Subject.FootRadius * RadiusMultiplier * View.RadiusScale * Target.SlopeRadiusScale;
	Decal->SetWorldLocationAndRotation(Target.Point, FRotationMatrix::MakeFromX(Down).Rotator());
	Decal->DecalSize = FVector(DepthHalfExtentCm, Radius, Radius);
	Decal->SetVisibility(true);
}

void UGSTN_DepthPresenter_Decal::OnMarkerReleased_Implementation()
{
	if (Decal)
	{
		Decal->SetVisibility(false);
	}
}

UInstancedStaticMeshComponent* UGSTN_DepthPresenter_Instanced::EnsureComponent(AActor* Holder, int32 Count)
{
	if (Instances)
	{
		return Instances;
	}
	UStaticMesh* Mesh = !MeshOverride.IsNull() ? MeshOverride.LoadSynchronous() : UGSTN_DepthMarkerSettings::Get().DefaultSegmentMesh.LoadSynchronous();
	if (!Holder || !Mesh)
	{
		// No content: the presenter draws nothing until a style or the settings name a mesh
		return nullptr;
	}
	Instances = NewObject<UInstancedStaticMeshComponent>(Holder);
	Instances->SetStaticMesh(Mesh);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCastShadow(false);
	Instances->SetCanEverAffectNavigation(false);
	Instances->SetupAttachment(Holder->GetRootComponent());
	Instances->RegisterComponent();
	PoolSize = Count;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Instances->AddInstance(GSTNDepthMarkerPresenterPrivate::HiddenInstance(), true);
	}
	return Instances;
}

void UGSTN_DepthPresenter_Instanced::HideAll()
{
	if (Instances)
	{
		Instances->SetVisibility(false);
	}
}

void UGSTN_DepthPresenter_Instanced::OnMarkerReleased_Implementation()
{
	HideAll();
}

UGSTN_DepthPresenter_Ring::UGSTN_DepthPresenter_Ring()
{
	ActiveStates = { EGSTN_DepthMarkerState::Ground, EGSTN_DepthMarkerState::EdgeHold, EGSTN_DepthMarkerState::NoGround };
}

void UGSTN_DepthPresenter_Ring::OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder)
{
	UInstancedStaticMeshComponent* Component = EnsureComponent(Holder, Segments);
	if (!Component)
	{
		return;
	}
	ApplyLook(View);
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}

	const FGSTN_DepthTarget& Target = View.GetTarget(Layer);
	const FVector Normal = Target.PresentNormal.GetSafeNormal();
	const FVector A = GSTNDepthMarkerPresenterPrivate::AnyPerpendicular(Normal);
	const FVector B = FVector::CrossProduct(Normal, A);
	const float Radius = View.Subject.FootRadius * RadiusMultiplier * View.RadiusScale * Target.SlopeRadiusScale;
	const float SegmentLength = 2.f * UE_PI * Radius / Segments * 1.08f;

	for (int32 Index = 0; Index < PoolSize; ++Index)
	{
		const float Angle = 2.f * UE_PI * Index / Segments;
		const FVector Radial = A * FMath::Cos(Angle) + B * FMath::Sin(Angle);
		const FVector Tangent = FVector::CrossProduct(Normal, Radial);
		const FVector Location = Target.Point + Normal * 0.5f + Radial * Radius;
		const FRotator Rotation = FRotationMatrix::MakeFromXZ(Tangent, Normal).Rotator();
		// Cube meshes are 100 cm: length along the tangent, thickness along the radius, a flat slab
		Component->UpdateInstanceTransform(Index, FTransform(Rotation, Location, FVector(SegmentLength / 100.f, ThicknessCm / 100.f, 0.02f)),
			true, Index == PoolSize - 1, true);
	}
	Component->SetVisibility(true);
}

UGSTN_DepthPresenter_Line::UGSTN_DepthPresenter_Line()
{
	ActiveStates = { EGSTN_DepthMarkerState::Ground, EGSTN_DepthMarkerState::EdgeHold, EGSTN_DepthMarkerState::NoGround };
}

void UGSTN_DepthPresenter_Line::OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder)
{
	UInstancedStaticMeshComponent* Component = EnsureComponent(Holder, MaxDashes + 2);
	if (!Component)
	{
		return;
	}
	const FGSTN_DepthTarget& Target = View.GetTarget(Layer);
	// Below: from the feet. Above: from the head, so the line starts off the character (§4.1)
	const FVector Up = View.Subject.Up.GetSafeNormal();
	const FVector Start = Layer == EGSTN_DepthLayer::Above ? View.Subject.Feet + Up * View.Subject.TopOffset : View.Subject.Feet;
	const FVector Delta = Target.Point - Start;
	const float Length = static_cast<float>(Delta.Size());
	if (Length < 1.f)
	{
		HideAll();
		return;
	}
	ApplyLook(View);
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}

	const FVector Direction = Delta / Length;
	// A long line widens the period instead of growing the pool
	const float Period = FMath::Max(DashPeriodCm, Length / MaxDashes);
	const float DashLength = Period * DutyCycle;
	const float March = FMath::Fmod(View.TimeSeconds * MarchSpeed, Period);
	const FRotator Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();

	for (int32 Index = 0; Index < PoolSize; ++Index)
	{
		const float S0 = (Index - 1) * Period + March;
		const float From = FMath::Max(S0, 0.f);
		const float To = FMath::Min(S0 + DashLength, Length);
		FTransform Xf = GSTNDepthMarkerPresenterPrivate::HiddenInstance();
		if (To > From)
		{
			Xf = FTransform(Rotation, Start + Direction * ((From + To) * 0.5f),
				FVector((To - From) / 100.f, WidthCm / 100.f, WidthCm / 100.f));
		}
		Component->UpdateInstanceTransform(Index, Xf, true, Index == PoolSize - 1, true);
	}
	Component->SetVisibility(true);
}

UGSTN_DepthPresenter_Plane::UGSTN_DepthPresenter_Plane()
{
	ActiveStates = { EGSTN_DepthMarkerState::Ground, EGSTN_DepthMarkerState::EdgeHold, EGSTN_DepthMarkerState::NoGround };
}

void UGSTN_DepthPresenter_Plane::OnMarkerUpdated_Implementation(const FGSTN_DepthMarkerView& View, AActor* Holder)
{
	// Off without a material: nothing is created and no content is loaded
	if (!Holder || PlaneMaterial.IsNull())
	{
		return;
	}
	if (!PlaneComponent)
	{
		UMaterialInterface* Parent = PlaneMaterial.LoadSynchronous();
		UStaticMesh* Mesh = !MeshOverride.IsNull() ? MeshOverride.LoadSynchronous()
			: LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		if (!Parent || !Mesh)
		{
			return;
		}
		Material = UMaterialInstanceDynamic::Create(Parent, this);
		PlaneComponent = NewObject<UStaticMeshComponent>(Holder);
		PlaneComponent->SetStaticMesh(Mesh);
		PlaneComponent->SetMaterial(0, Material);
		PlaneComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PlaneComponent->SetCastShadow(false);
		PlaneComponent->SetCanEverAffectNavigation(false);
		PlaneComponent->SetupAttachment(Holder->GetRootComponent());
		PlaneComponent->RegisterComponent();
	}

	const FGSTN_DepthTarget& Target = View.GetTarget(Layer);
	const FVector Normal = Target.PresentNormal.GetSafeNormal();
	// A hair off the surface so the quad does not z-fight with it
	PlaneComponent->SetWorldLocationAndRotation(Target.Point + Normal * 0.5f, FRotationMatrix::MakeFromZ(Normal).Rotator());
	PlaneComponent->SetWorldScale3D(FVector(PlaneExtentCm / 100.f));

	static const FName PositionName(TEXT("MarkerWorldPosition"));
	static const FName NormalName(TEXT("MarkerNormal"));
	static const FName OpacityName(TEXT("Opacity"));
	static const FName EdgeDistanceName(TEXT("EdgeDistance"));
	Material->SetVectorParameterValue(PositionName, FLinearColor(Target.Point.X, Target.Point.Y, Target.Point.Z, 0.f));
	Material->SetVectorParameterValue(NormalName, FLinearColor(Normal.X, Normal.Y, Normal.Z, 0.f));
	Material->SetScalarParameterValue(OpacityName, GetAlpha(View));
	Material->SetScalarParameterValue(EdgeDistanceName, Layer == EGSTN_DepthLayer::Approach ? Target.EdgeDistanceCm : 0.f);
	PlaneComponent->SetVisibility(true);
}

void UGSTN_DepthPresenter_Plane::OnMarkerReleased_Implementation()
{
	if (PlaneComponent)
	{
		PlaneComponent->SetVisibility(false);
	}
}
