// The screw once it is out.

#include "GSTN_SpinScrewPickup.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AGSTN_SpinScrewPickup::AGSTN_SpinScrewPickup()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Visual && Cylinder.Succeeded())
	{
		// A mesh set here keeps AGSTN_HeldObjectActor::BeginPlay from putting its placeholder cube
		Visual->SetStaticMesh(Cylinder.Object);
		Visual->SetRelativeScale3D(FVector(0.3, 0.3, 0.4));
	}
}
