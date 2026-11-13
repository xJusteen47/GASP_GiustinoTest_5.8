// Logical buttons and their Enhanced Input actions: who holds a player's input config.

#include "Input/GSTN_InputConfigSource.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

const UGSTN_InputConfig* IGSTN_InputConfigSource::FindOn(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}
	for (const UActorComponent* Component : Actor->GetComponents())
	{
		if (const IGSTN_InputConfigSource* Source = Cast<IGSTN_InputConfigSource>(Component))
		{
			if (const UGSTN_InputConfig* Config = Source->GetPlayerInputConfig())
			{
				return Config;
			}
		}
	}
	return nullptr;
}
