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
	// The only replicated state is sent with the spawn; nothing else is ever dirty, so a high rate costs property compares, not bytes.
	// It bounds how late the spawn reaches a client: a lost spawn bunch is resent at the actor's next update (at 1 Hz, slice 2, a
	// projectile living one second often never reached anyone and a ghost waited past its timeout at 2 % loss)
	SetNetUpdateFrequency(30.f);

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
	LifetimeFrames = static_cast<uint16>(FMath::Clamp(InLifetimeFrames, 1, static_cast<int32>(MAX_uint16)));
	VisualRadius = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(InVisualRadius), 1, 255));
}

void AGSTN_Projectile::InitGhost(const FGSTN_BallisticSegment& InSegment, int32 InLifetimeFrames, float InVisualRadius)
{
	InitProjectile(InSegment, InLifetimeFrames, InVisualRadius);
	bGhost = true;
	SetReplicates(false);
}

void AGSTN_Projectile::StartBlend(const FVector& Offset, int32 Frame, int32 InBlendFrames)
{
	BlendStartOffset = Offset;
	BlendStartFrame = Frame;
	BlendFrames = InBlendFrames;
	UpdateFromFrame();
}

FVector AGSTN_Projectile::GetDrawnPositionAt(int32 Frame) const
{
	const FVector Offset = BlendStartFrame == INDEX_NONE ? FVector::ZeroVector
		: FGSTN_ProjectileGhostReconcile::BlendOffset(BlendStartOffset, BlendStartFrame, BlendFrames, Frame);
	return FGSTN_BallisticMode::PositionAt(Segment, Frame) + Offset;
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
	Visual->SetRelativeScale3D(FVector(FMath::Max<double>(VisualRadius, 1.0) / 50.0));
	// The host's projectile reaching a client (counted), and the client that fired it: adopt or drop its ghost before the first draw
	if (!bGhost && GetLocalRole() != ROLE_Authority)
	{
		if (UGSTN_ProjectileSubsystem* Subsystem = UGSTN_ProjectileSubsystem::Get(GetWorld()))
		{
			Subsystem->NoteReceivedFromHost();
			Subsystem->ReconcileGhostFor(*this);
		}
	}
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
	SetActorLocationAndRotation(GetDrawnPositionAt(Frame), Carrier.GetRotation());
}
