// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#include "GSTN_PlatformTestFighter.h"

#include "Components/SceneComponent.h"
#include "GSTN_ScriptedPlatformComponent.h"
#include "GSTN_SurfaceTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PlatformTestFighter)

AGSTN_PlatformTestFighter::AGSTN_PlatformTestFighter()
{
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	Platform = CreateDefaultSubobject<UGSTN_ScriptedPlatformComponent>(TEXT("Platform"));
	Platform->Path.Axis = FVector(0.0, 1.0, 0.0);
	Platform->Path.Speed = 300.f;
	Platform->Path.HalfLength = 300.f;

	bReplicates = true;
	// Each machine computes the pose from the replicated origin and the simulation time
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;
}

FGSTN_PlatformSpec AGSTN_PlatformTestFighter::MakeTestFighterSpec()
{
	FGSTN_PlatformSpec Spec;

	FGSTN_PlatformShape Flank;
	Flank.ShapeId = 0;
	Flank.Type = EGSTN_PlatformShapeType::Box;
	Flank.Extent = FVector(25.0, 400.0, 250.0);
	Flank.LocalLocation = FVector(-150.0, 0.0, 300.0);
	Flank.Behaviour.Behaviours.AddTag(GSTNSurfaceTags::Behaviour_WallSlide);
	Spec.Shapes.Add(Flank);

	FGSTN_PlatformShape Top;
	Top.ShapeId = 1;
	Top.Type = EGSTN_PlatformShapeType::Box;
	Top.Extent = FVector(150.0, 400.0, 15.0);
	Top.LocalLocation = FVector(0.0, 0.0, 565.0);
	Top.Behaviour.Behaviours.AddTag(GSTNSurfaceTags::Behaviour_Stand);
	Spec.Shapes.Add(Top);

	return Spec;
}

void AGSTN_PlatformTestFighter::PreRegisterAllComponents()
{
	// Native tags are safe to use here (the constructor of the class default object may run before the tag tree exists)
	if (Platform)
	{
		Platform->Spec = MakeTestFighterSpec();
	}
	Super::PreRegisterAllComponents();
}
