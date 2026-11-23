// Held objects, world side.

#include "GSTN_HeldObjectActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GSTN_HeldObjectComponent.h"

AGSTN_HeldObjectActor::AGSTN_HeldObjectActor()
{
	bReplicates = true;
	SetReplicatingMovement(false);
	// Dormant at rest: nothing changes, so a low rate costs nothing; a pickup or throw forces an update
	SetNetUpdateFrequency(10.f);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	RootComponent = Visual;
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetGenerateOverlapEvents(false);

	HeldObject = CreateDefaultSubobject<UGSTN_HeldObjectComponent>(TEXT("HeldObject"));
}

void AGSTN_HeldObjectActor::BeginPlay()
{
	if (Visual && !Visual->GetStaticMesh())
	{
		// Placeholder look (owner rule: no aesthetics yet): the engine cube (100 cm) scaled to the object's size
		if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			Visual->SetStaticMesh(Cube);
			const double Size = HeldObject ? HeldObject->GetTuning().RestHalfHeight * 2.0 / 100.0 : 0.4;
			Visual->SetWorldScale3D(FVector(Size));
		}
	}
	Super::BeginPlay();
}
