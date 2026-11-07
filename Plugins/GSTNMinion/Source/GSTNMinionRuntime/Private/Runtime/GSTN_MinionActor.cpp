// Minion runtime (authority batch, M8 G3).

#include "Runtime/GSTN_MinionActor.h"

#include "Components/SceneComponent.h"
#include "Runtime/GSTN_MinionCombatComponent.h"

AGSTN_MinionActor::AGSTN_MinionActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MinionComponent = CreateDefaultSubobject<UGSTN_MinionCombatComponent>(TEXT("MinionCombat"));
}

void AGSTN_MinionActor::OnAcquired(const FTransform& Transform)
{
	bPooled = false;
	SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}

void AGSTN_MinionActor::OnReleased()
{
	bPooled = true;
	if (MinionComponent)
	{
		MinionComponent->ResetForPool();
	}
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}
