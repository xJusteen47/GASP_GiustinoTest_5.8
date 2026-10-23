// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#include "Widgets/GSTN_LocalInputSource.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CommandSourceInterface.h"

UGSTN_CombatSimComponent* FGSTN_LocalInputSource::FindLocalComponent(const UWorld* InWorld)
{
	if (!InWorld)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator Iterator = InWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* Controller = Iterator->Get();
		if (Controller && Controller->IsLocalController())
		{
			if (const APawn* Pawn = Controller->GetPawn())
			{
				if (UGSTN_CombatSimComponent* Found = Pawn->FindComponentByClass<UGSTN_CombatSimComponent>())
				{
					return Found;
				}
			}
		}
	}
	return nullptr;
}

UGSTN_CombatSimComponent* FGSTN_LocalInputSource::FindLocalComponentInAnyWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
		{
			if (UGSTN_CombatSimComponent* Found = FindLocalComponent(Context.World()))
			{
				return Found;
			}
		}
	}
	return nullptr;
}

bool FGSTN_LocalInputSource::Fetch(FGSTN_InputDisplayData& OutData)
{
	UGSTN_CombatSimComponent* Current = Component.Get();
	if (!Current || !IsValid(Current) || !Current->GetOwner() || !Current->GetOwner()->HasActorBegunPlay())
	{
		// The pawn can be replaced (respawn, possession): look again, and start the markers over for the new one
		UGSTN_CombatSimComponent* Found = World.IsValid() ? FindLocalComponent(World.Get()) : FindLocalComponentInAnyWorld();
		if (Found != Current)
		{
			Markers.Reset();
		}
		Component = Found;
		Current = Found;
	}
	if (!Current)
	{
		return false;
	}

	const FGSTN_InputHistory* History = Current->GetInputHistoryView();
	const FGSTN_CombatSyncState* Sync = Current->GetSyncState();
	if (!History || !Sync)
	{
		return false;
	}

	const int32 Latest = History->GetLatestFrame();
	Markers.RemoveAll([Latest](const FGSTN_InputDisplayMarker& Marker) { return Marker.Frame > Latest; });

	const FGSTN_InputSimState& Input = Sync->Core.Input;
	if (Input.RequestedBinding != 0 && Input.RequestFrame != INDEX_NONE)
	{
		const bool bKnown = Markers.ContainsByPredicate([&Input](const FGSTN_InputDisplayMarker& Marker) { return Marker.Frame == Input.RequestFrame; });
		if (!bKnown)
		{
			const FGSTN_ActivationContext Context = FGSTN_ActivationContext::FromRequest(Input, Current->GetCommandTable(), Input.RequestFrame);
			if (Context.IsValid())
			{
				Markers.Add({ Input.RequestFrame, Context.Command });
				if (Markers.Num() > MaxMarkers)
				{
					Markers.RemoveAt(0);
				}
			}
		}
	}

	OutData.History = *History;
	OutData.Markers = Markers;
	return true;
}
