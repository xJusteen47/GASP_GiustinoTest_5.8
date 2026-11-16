// Test-only actor for GSTNMinion.Runtime.Promote.* (M8 G9a).

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.h"
#include "GSTN_MinionTestActors.generated.h"

/**
 * Stands in for the promotion fighter class: a plain actor that keeps the team the promotion gave it. No pawn, no Mover, no ASC
 * (the real fighter is the kit's, G9b); it only has to be spawnable and implement the team interface.
 */
UCLASS(NotBlueprintable, meta = (Hidden))
class AGSTN_MinionTestFighter : public AActor, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	/** A root so the actor has a location the promotion can place and a test can read */
	AGSTN_MinionTestFighter()
	{
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	}

	virtual void SetGenericTeamId(const FGenericTeamId& InTeamId) override { TeamId = InTeamId; }
	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }

	FGenericTeamId TeamId = FGenericTeamId::NoTeam;
};
