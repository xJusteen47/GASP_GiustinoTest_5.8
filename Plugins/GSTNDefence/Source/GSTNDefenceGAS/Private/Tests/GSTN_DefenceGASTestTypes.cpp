// Defence, ability-system half: test fixtures (see the header).

#include "Tests/GSTN_DefenceGASTestTypes.h"

#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace GSTNDefenceGASTest
{
	FRig::FRig()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNDefenceGASTestWorld"));
		World->AddToRoot();
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		AActor* Owner = World->SpawnActor<AActor>();
		ASC = NewObject<UAbilitySystemComponent>(Owner);
		ASC->RegisterComponent();
		ASC->InitAbilityActorInfo(Owner, Owner);
	}

	FRig::~FRig()
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		World->RemoveFromRoot();
	}
}
