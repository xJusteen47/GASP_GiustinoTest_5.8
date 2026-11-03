#include "GSTN_Trampoline.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameplayTagsManager.h"
#include "GSTN_SurfaceTags.h"
#include "GSTN_SurfaceUtils.h"
#include "Internationalization/Text.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "GSTNLevelObjects"

namespace GSTNTrampoline
{
	/** The smallest size per axis, cm: the ClampMin of Size, enforced in code so a Blueprint default or a script cannot make an empty box */
	constexpr double MinSize = 10.0;
}

AGSTN_Trampoline::AGSTN_Trampoline()
{
	PrimaryActorTick.bCanEverTick = false;
	// A level object is placed, not moved: static by default, which is what the rollback rule asks for
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetMobility(EComponentMobility::Static);
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->SetBoxExtent(SizeToBoxExtent(Size));
	RootComponent = Box;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Box);
	// Visual only: movable so OnConstruction can swap the mesh and rescale it on a placed actor (a static mesh component refuses both once registered)
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// A soft reference, never loaded here: the CDO of a class must not pull assets into memory
	MeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));

	Launch.DirectionMode = EGSTN_SurfaceLaunchDirection::LocalDirection;
	Launch.LocalDirection = FVector::UpVector;

#if WITH_EDITORONLY_DATA
	LaunchArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("LaunchArrow"));
	if (LaunchArrow)
	{
		LaunchArrow->SetupAttachment(Box);
		LaunchArrow->SetMobility(EComponentMobility::Movable);
		LaunchArrow->ArrowColor = FColor(255, 160, 0);
		LaunchArrow->bIsScreenSizeScaled = false;
		LaunchArrow->bTreatAsASprite = false;
		LaunchArrow->SetHiddenInGame(true);
	}
#endif
}

FVector AGSTN_Trampoline::SizeToBoxExtent(const FVector& InSize)
{
	return FVector(FMath::Max(InSize.X, GSTNTrampoline::MinSize), FMath::Max(InSize.Y, GSTNTrampoline::MinSize), FMath::Max(InSize.Z, GSTNTrampoline::MinSize)) * 0.5;
}

FVector AGSTN_Trampoline::ComputeMeshScale(EGSTN_TrampolineMeshFit Fit, const FVector& InSize, const FVector& MeshBoundsSize)
{
	if (Fit == EGSTN_TrampolineMeshFit::Keep)
	{
		return FVector::OneVector;
	}
	const FVector Box = SizeToBoxExtent(InSize) * 2.0;
	// A mesh with no thickness on an axis cannot be stretched along it
	return FVector(MeshBoundsSize.X > UE_KINDA_SMALL_NUMBER ? Box.X / MeshBoundsSize.X : 1.0, MeshBoundsSize.Y > UE_KINDA_SMALL_NUMBER ? Box.Y / MeshBoundsSize.Y : 1.0,
		MeshBoundsSize.Z > UE_KINDA_SMALL_NUMBER ? Box.Z / MeshBoundsSize.Z : 1.0);
}

void AGSTN_Trampoline::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void AGSTN_Trampoline::Rebuild()
{
	Box->SetBoxExtent(SizeToBoxExtent(Size), true);

	// Instances load their mesh; the CDO never gets here (OnConstruction runs on placed and spawned actors only)
	UStaticMesh* StaticMesh = MeshAsset.IsNull() || HasAnyFlags(RF_ClassDefaultObject) ? nullptr : MeshAsset.LoadSynchronous();
	Mesh->SetStaticMesh(StaticMesh);
	Mesh->SetVisibility(StaticMesh != nullptr);
	if (StaticMesh)
	{
		const FVector Scale = ComputeMeshScale(MeshFit, Size, StaticMesh->GetBounds().BoxExtent * 2.0);
		// The mesh's own bounds can be off centre: the stretch keeps its centre in the middle of the box, the offset moves it from there
		const FVector Centre = StaticMesh->GetBounds().Origin * Scale;
		Mesh->SetRelativeScale3D(Scale);
		Mesh->SetRelativeLocation(MeshOffset - Centre);
	}

#if WITH_EDITORONLY_DATA
	if (LaunchArrow)
	{
		// Previews the direction of the top face, not of whatever face a sweep touches: the normal is the actor's up
		const FVector Direction = UGSTN_SurfaceUtils::ComputeLaunchDirection(Launch, GetActorTransform(), GetActorUpVector());
		const FQuat Rotation = FRotationMatrix::MakeFromX(Direction).ToQuat();
		LaunchArrow->SetWorldRotation(Rotation);
		LaunchArrow->SetRelativeLocation(FVector(0.0, 0.0, SizeToBoxExtent(Size).Z));
		LaunchArrow->ArrowSize = 2.f;
		LaunchArrow->ArrowLength = FMath::Clamp(Launch.Speed * 0.1f, 60.f, 400.f);
	}
#endif
}

FGSTN_SurfaceBehaviour AGSTN_Trampoline::GetSurfaceBehaviour_Implementation(const FGSTN_SurfaceQuery& Query) const
{
	FGSTN_SurfaceBehaviour Out;
	Out.SurfaceType = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Surface.Type.Trampoline"), false);
	Out.Behaviours.AddTag(GSTNSurfaceTags::Behaviour_Stand);

	// Only the top face launches. A query without a normal (a component resolution) is the whole object: it offers the launch
	const FVector Normal = Query.ContactNormal.GetSafeNormal();
	const double CosTolerance = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp<double>(LaunchFaceToleranceDegrees, 1.0, 60.0)));
	if (Normal.IsNearlyZero() || Normal.Dot(GetActorUpVector()) >= CosTolerance)
	{
		Out.Behaviours.AddTag(GSTNSurfaceTags::Behaviour_Launch);
		Out.Launch = Launch;
	}
	return Out;
}

#if WITH_EDITOR
EDataValidationResult AGSTN_Trampoline::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Rollback rule: the launch parameters are read as static configuration; a moving trampoline is out of scope until platform shapes land
	if (RootComponent && RootComponent->Mobility != EComponentMobility::Static)
	{
		Context.AddWarning(LOCTEXT("TrampolineNotStatic", "The trampoline's launch parameters are static configuration read by the simulation on every machine; set Mobility to Static (a moving or retuned trampoline is not supported yet)."));
		if (Result == EDataValidationResult::NotValidated)
		{
			Result = EDataValidationResult::Valid;
		}
	}

	TArray<FText> Errors;
	if (!Launch.Validate(Errors))
	{
		for (const FText& Error : Errors)
		{
			Context.AddError(Error);
		}
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
