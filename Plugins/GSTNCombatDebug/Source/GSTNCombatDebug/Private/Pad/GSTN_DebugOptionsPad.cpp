// Combat debug adapter (not in Shipping).

#include "Pad/GSTN_DebugOptionsPad.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

AGSTN_DebugOptionsPad::AGSTN_DebugOptionsPad()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Mesh->SetStaticMesh(Cylinder.Object);
	}
	// 240 cm wide, 10 cm high: the engine cylinder is 100 cm a side
	Mesh->SetRelativeScale3D(FVector(2.4, 2.4, 0.1));
}

void AGSTN_DebugOptionsPad::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	bool bOn = false;
	if (Pawn && Controller->IsLocalController())
	{
		const FVector Delta = Pawn->GetActorLocation() - GetActorLocation();
		bOn = FVector2D(Delta.X, Delta.Y).Size() <= Radius && Delta.Z > -50.0 && Delta.Z < 250.0;
	}
	if (bOn && !bOccupied && GEngine)
	{
		GEngine->Exec(const_cast<UWorld*>(World), *Command);
		UE_LOG(LogTemp, Display, TEXT("GSTN debug pad: %s"), *Command);
	}
	bOccupied = bOn;
}
