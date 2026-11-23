// Respawn, server side.

#include "GSTN_Checkpoint.h"
#include "Components/BoxComponent.h"
#include "GSTN_RespawnLogic.h"
#include "GSTN_RespawnSubsystem.h"
#include "Net/UnrealNetwork.h"

AGSTN_Checkpoint::AGSTN_Checkpoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.f);

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	Area->SetBoxExtent(FVector(200.f, 200.f, 150.f));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetGenerateOverlapEvents(false);
	Area->SetHiddenInGame(true);
	Area->ShapeColor = FColor(255, 200, 0);
	RootComponent = Area;

	// Four players side by side, facing the actor's forward
	for (int32 Index = 0; Index < 4; ++Index)
	{
		SpawnPoints.Add(FTransform(FVector(0.f, (Index - 1.5f) * 120.f, 0.f)));
	}
}

FTransform AGSTN_Checkpoint::GetSpawnTransform(int32 Index) const
{
	int32 Row = 0;
	const int32 Point = GSTNRespawnLogic::SpawnPointFor(Index, SpawnPoints.Num(), Row);
	FTransform Local = SpawnPoints.IsValidIndex(Point) ? SpawnPoints[Point] : FTransform::Identity;
	Local.AddToTranslation(FVector(-RowSpacing * Row, 0.f, 0.f));
	FTransform World = Local * GetActorTransform();
	World.SetScale3D(FVector::OneVector);
	return World;
}

bool AGSTN_Checkpoint::ContainsLocation(const FVector& Location) const
{
	if (!Area)
	{
		return false;
	}
	const FVector Local = Area->GetComponentTransform().InverseTransformPosition(Location);
	const FVector Extent = Area->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

void AGSTN_Checkpoint::SetActive(bool bInActive)
{
	if (bActive != bInActive)
	{
		bActive = bInActive;
		OnRep_Active();
	}
}

void AGSTN_Checkpoint::BeginPlay()
{
	Super::BeginPlay();
	if (UGSTN_RespawnSubsystem* Respawn = UGSTN_RespawnSubsystem::GetRespawnSubsystem(this))
	{
		Respawn->RegisterCheckpoint(this);
	}
}

void AGSTN_Checkpoint::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UGSTN_RespawnSubsystem* Respawn = UGSTN_RespawnSubsystem::GetRespawnSubsystem(this))
	{
		Respawn->UnregisterCheckpoint(this);
	}
	Super::EndPlay(Reason);
}

void AGSTN_Checkpoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGSTN_Checkpoint, bActive);
}

void AGSTN_Checkpoint::OnRep_Active()
{
	OnActivatedChanged(bActive);
}
