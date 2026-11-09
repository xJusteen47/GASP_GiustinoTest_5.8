// Projectile, world side (needs the GAS-NPP fork for the fixed tick it reads).

#include "GSTN_ProjectileActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GSTN_ProjectileSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AGSTN_Projectile::AGSTN_Projectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	// The only replicated state is sent with the spawn; nothing else is ever dirty
	SetNetUpdateFrequency(1.f);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetGenerateOverlapEvents(false);
	Visual->SetCastShadow(false);
	// A plain engine sphere so a projectile can be seen in a test; the look comes later (owner rule: no aesthetics yet)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Visual->SetStaticMesh(Sphere.Object);
	}
}

void AGSTN_Projectile::InitProjectile(const FGSTN_BallisticSegment& InSegment, int32 InLifetimeFrames, float InVisualRadius)
{
	Segment = InSegment;
	LifetimeFrames = InLifetimeFrames;
	VisualRadius = InVisualRadius;
}

void AGSTN_Projectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AGSTN_Projectile, Segment, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AGSTN_Projectile, LifetimeFrames, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AGSTN_Projectile, VisualRadius, COND_InitialOnly);
}

void AGSTN_Projectile::BeginPlay()
{
	Super::BeginPlay();
	// The engine sphere is 100 cm across
	Visual->SetRelativeScale3D(FVector(FMath::Max(VisualRadius, 1.f) / 50.f));
	UpdateFromFrame();
}

void AGSTN_Projectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFromFrame();
}

void AGSTN_Projectile::UpdateFromFrame()
{
	const UGSTN_ProjectileSubsystem* Subsystem = UGSTN_ProjectileSubsystem::Get(GetWorld());
	if (!Subsystem || Subsystem->GetLastFrame() == INDEX_NONE)
	{
		return;
	}
	const int32 Frame = Subsystem->GetLastFrame();
	SetActorHiddenInGame(Frame > Segment.StartFrame + LifetimeFrames);
	const FTransform Carrier = FGSTN_BallisticMode::CarrierAt(Segment, Frame);
	SetActorLocationAndRotation(Carrier.GetLocation(), Carrier.GetRotation());
}
