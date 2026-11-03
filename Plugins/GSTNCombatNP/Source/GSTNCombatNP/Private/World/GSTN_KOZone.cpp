// Combat simulation (GAS-NPP fork).

#include "World/GSTN_KOZone.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "World/GSTN_CombatWorldSubsystem.h"

AGSTN_KOZone::AGSTN_KOZone()
{
	PrimaryActorTick.bCanEverTick = false;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetHiddenInGame(true);
	Box->ShapeColor = FColor::Red;
	SetRootComponent(Box);
}

void AGSTN_KOZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Box->SetBoxExtent(HalfExtent, false);
}

void AGSTN_KOZone::BeginPlay()
{
	Super::BeginPlay();
	if (UGSTN_CombatWorldSubsystem* Subsystem = UGSTN_CombatWorldSubsystem::Get(GetWorld()))
	{
		Subsystem->RegisterKOZone(this, GetActorTransform(), HalfExtent);
	}
}

void AGSTN_KOZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGSTN_CombatWorldSubsystem* Subsystem = UGSTN_CombatWorldSubsystem::Get(GetWorld()))
	{
		Subsystem->UnregisterKOZone(this);
	}
	Super::EndPlay(EndPlayReason);
}

AGSTN_KOZone* AGSTN_KOZone::SpawnZone(UObject* WorldContext, FVector Center, FVector InHalfExtent)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Deferred so HalfExtent is set before BeginPlay registers the bounds
	AGSTN_KOZone* Zone = World->SpawnActorDeferred<AGSTN_KOZone>(StaticClass(), FTransform(Center), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Zone)
	{
		Zone->HalfExtent = InHalfExtent;
		Zone->FinishSpawning(FTransform(Center));
	}
	return Zone;
}

void AGSTN_KOZone::DestroyAllZones(UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return;
	}
	TArray<AGSTN_KOZone*> Zones;
	for (TActorIterator<AGSTN_KOZone> It(World); It; ++It)
	{
		Zones.Add(*It);
	}
	for (AGSTN_KOZone* Zone : Zones)
	{
		Zone->Destroy();
	}
}
