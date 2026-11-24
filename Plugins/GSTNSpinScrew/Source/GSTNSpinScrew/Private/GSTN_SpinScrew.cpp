// A screw on the floor that the spin and the drill dive unscrew.

#include "GSTN_SpinScrew.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GSTN_HeldSubsystem.h"
#include "GSTN_SpinScrewPickup.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AGSTN_SpinScrew::AGSTN_SpinScrew()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	// Turns are rare: a low rate costs nothing at rest; a turn marks it dirty
	SetNetUpdateFrequency(10.f);
	PickupClass = AGSTN_SpinScrewPickup::StaticClass();

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	// The head turns and rises at run time: everything under the root moves
	Root->SetMobility(EComponentMobility::Movable);
	Turning = CreateDefaultSubobject<USceneComponent>(TEXT("Turning"));
	Turning->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Engine basic shapes are 100 cm with the pivot at the centre. Greybox: head 60 cm wide and 12 tall with its top 6 cm above the floor (the
	// actor's origin = floor level), a shaft going down into the floor that shows as the head rises, a dark slot across the top to read the turn
	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Turning);
	Head->SetRelativeLocation(FVector(0, 0, 0));
	Head->SetRelativeScale3D(FVector(0.6, 0.6, 0.12));
	// Blocks pawns like the floor: the drill dive ends on it, a player can stand on it
	Head->SetCollisionProfileName(TEXT("BlockAll"));

	Shaft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shaft"));
	Shaft->SetupAttachment(Turning);
	Shaft->SetRelativeLocation(FVector(0, 0, -25));
	Shaft->SetRelativeScale3D(FVector(0.3, 0.3, 0.4));
	Shaft->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Slot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Slot"));
	Slot->SetupAttachment(Turning);
	Slot->SetRelativeLocation(FVector(0, 0, 6.5));
	Slot->SetRelativeScale3D(FVector(0.5, 0.08, 0.02));
	Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (Cylinder.Succeeded())
	{
		Head->SetStaticMesh(Cylinder.Object);
		Shaft->SetStaticMesh(Cylinder.Object);
	}
	if (Cube.Succeeded())
	{
		Slot->SetStaticMesh(Cube.Object);
	}
}

void AGSTN_SpinScrew::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGSTN_SpinScrew, Progress);
	DOREPLIFETIME(AGSTN_SpinScrew, bReleased);
}

void AGSTN_SpinScrew::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	DrawnProgress = static_cast<float>(Progress);
	ApplyLook();
}

bool AGSTN_SpinScrew::CanBeSpinTurned_Implementation(AActor* Turner) const
{
	return !bReleased;
}

void AGSTN_SpinScrew::OnSpinTurned_Implementation(AActor* Turner, int32 Sense, EGSTN_SpinTurnSource Source)
{
	TurnBy(Turner, Sense, Source == EGSTN_SpinTurnSource::Drill ? DrillSteps : SpinSteps);
}

void AGSTN_SpinScrew::TurnBy(AActor* Turner, int32 Sense, int32 Steps)
{
	if (!HasAuthority() || bReleased || Steps <= 0 || Sense == 0)
	{
		return;
	}
	const bool bLoosens = (Sense > 0) == (LoosenSense > 0);
	if (!bLoosens && !bOppositeTightens)
	{
		return;
	}
	const int32 Before = Progress;
	Progress = FMath::Clamp(Progress + (bLoosens ? Steps : -Steps), 0, FMath::Max(1, StepsToRelease));
	if (Progress == Before)
	{
		return;
	}
	LastTurner = Turner;
	ForceNetUpdate();
	OnRep_Progress();
	if (Progress >= StepsToRelease)
	{
		bReleased = true;
		ForceNetUpdate();
		OnReleased.Broadcast(this, Turner);
		BP_OnReleased(Turner);
		ReleaseScrew(Turner);
	}
}

void AGSTN_SpinScrew::ReleaseScrew_Implementation(AActor* Turner)
{
	if (Release == EGSTN_ScrewRelease::ToHandOrFloor && PickupClass)
	{
		UWorld* World = GetWorld();
		const FVector At = GetActorLocation() + GetActorUpVector() * (StepsToRelease * RisePerStepCm);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Pickup = World ? World->SpawnActor<AActor>(PickupClass, At, GetActorRotation(), Params) : nullptr;
		UGSTN_HeldSubsystem* Held = UGSTN_HeldSubsystem::Get(World);
		// Hands are busy when they carry an object or lift an enemy (the hand link): then the screw stays on the floor, free to pick up later
		if (Pickup && Held && Turner && !Held->FindHeldBy(Turner) && !UGSTN_HeldSubsystem::FindLiftedEnemy(Turner))
		{
			// Resolved by the host after this frame with the ordinary pickup race (nearest object in range): the new screw is right there
			Held->RequestPickup(Turner);
		}
	}
	HideScrew();
	// Gone for good once every client heard bReleased
	SetLifeSpan(2.f);
}

void AGSTN_SpinScrew::OnRep_Progress()
{
	OnTurned.Broadcast(this, Progress, LastTurner.Get());
	BP_OnTurned(Progress, LastTurner.Get());
}

void AGSTN_SpinScrew::OnRep_Released()
{
	if (bReleased)
	{
		if (!HasAuthority())
		{
			OnReleased.Broadcast(this, nullptr);
		}
		HideScrew();
	}
}

void AGSTN_SpinScrew::HideScrew()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}

void AGSTN_SpinScrew::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Target = static_cast<float>(Progress);
	if (!FMath::IsNearlyEqual(DrawnProgress, Target, 0.001f))
	{
		DrawnProgress = FMath::FInterpTo(DrawnProgress, Target, DeltaSeconds, LookFollowSpeed);
		ApplyLook();
	}
}

void AGSTN_SpinScrew::ApplyLook()
{
	if (!Turning)
	{
		return;
	}
	// Unscrewing turns it in the loosening sense: +1 clockwise seen from above = positive yaw in UE (Z up, left-handed)
	const float Yaw = static_cast<float>(LoosenSense) * DrawnProgress * DegreesPerStep;
	Turning->SetRelativeLocationAndRotation(FVector(0, 0, DrawnProgress * RisePerStepCm), FRotator(0, Yaw, 0));
}
