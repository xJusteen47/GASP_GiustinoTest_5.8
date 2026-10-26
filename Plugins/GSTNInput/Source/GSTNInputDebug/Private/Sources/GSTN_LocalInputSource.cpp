// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#include "Sources/GSTN_LocalInputSource.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"

#define LOCTEXT_NAMESPACE "GSTNInputDebug"

namespace
{
	const TCHAR* NetModeName(ENetMode Mode)
	{
		switch (Mode)
		{
		case NM_Standalone:     return TEXT("Standalone");
		case NM_ListenServer:   return TEXT("Listen server");
		case NM_Client:         return TEXT("Client");
		case NM_DedicatedServer: return TEXT("Dedicated server");
		default:                return TEXT("Game");
		}
	}
}

void FGSTN_LocalInputSource::CollectPlayers(UWorld* OnlyWorld, TArray<FPlayer>& OutPlayers)
{
	const auto AddWorld = [&OutPlayers](const FWorldContext* Context, UWorld* InWorld)
	{
		if (InWorld == nullptr)
		{
			return;
		}
		const int32 PIEInstance = Context ? Context->PIEInstance : INDEX_NONE;

		// A window has the focus when its viewport widget has the user focus; the display itself and the editor do not count as a window
		const bool bFocused = Context && Context->GameViewport && Context->GameViewport->Viewport && Context->GameViewport->Viewport->HasFocus();

		int32 PlayerIndex = 0;
		for (FConstPlayerControllerIterator Iterator = InWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			const APlayerController* Controller = Iterator->Get();
			if (Controller == nullptr || !Controller->IsLocalController())
			{
				continue;
			}
			APawn* Pawn = Controller->GetPawn();

			FPlayer& Player = OutPlayers.AddDefaulted_GetRef();
			Player.Pawn = Pawn;
			Player.Candidate.Key = FString::Printf(TEXT("%d/%d"), PIEInstance, PlayerIndex);
			Player.Candidate.Label = FText::Format(LOCTEXT("PlayerLabel", "{0} (PIE {1}): {2}"), FText::FromString(NetModeName(InWorld->GetNetMode())),
				FText::AsNumber(PIEInstance), FText::FromString(Pawn ? GetNameSafe(Pawn) : TEXT("no pawn")));
			Player.Candidate.bFocused = bFocused;
			Player.Candidate.bHasSource = Pawn != nullptr && FGSTN_InputSourceRegistry::Get().FindSource(*Pawn).IsValid();
			++PlayerIndex;
		}
	};

	if (OnlyWorld != nullptr)
	{
		AddWorld(GEngine ? GEngine->GetWorldContextFromWorld(OnlyWorld) : nullptr, OnlyWorld);
		return;
	}
	if (GEngine == nullptr)
	{
		return;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
		{
			AddWorld(&Context, Context.World());
		}
	}
}

void FGSTN_LocalInputSource::ListPlayers(TArray<FGSTN_InputTargetCandidate>& OutCandidates)
{
	TArray<FPlayer> Players;
	CollectPlayers(nullptr, Players);
	for (const FPlayer& Player : Players)
	{
		OutCandidates.Add(Player.Candidate);
	}
}

FGSTN_InputDisplayProfile FGSTN_LocalInputSource::GetProfile() const
{
	FGSTN_InputDisplayProfile Profile;
	if (Source.IsValid())
	{
		Source->GetButtonTags(Profile.ButtonTags);
		Source->GetScriptSlots(Profile.Slots);
		Profile.Label = Source->GetSourceLabel();
	}
	if (Profile.ButtonTags.IsEmpty())
	{
		// A source that names no buttons: the default profile's buttons if one is registered, else six unnamed
		Profile.ButtonTags = FGSTN_InputSourceRegistry::Get().GetDefaultProfile().ButtonTags;
	}
	return Profile;
}

bool FGSTN_LocalInputSource::Fetch(FGSTN_InputDisplayData& OutData)
{
	TArray<FPlayer> Players;
	CollectPlayers(World.Get(), Players);

	const FPlayer* Chosen = nullptr;
	if (Players.IsEmpty())
	{
		CurrentKey.Reset();
		Status = LOCTEXT("NoPlayer", "No local player: start a play session");
	}
	else if (World.IsValid())
	{
		Chosen = &Players[0];
	}
	else
	{
		TArray<FGSTN_InputTargetCandidate> Candidates;
		for (const FPlayer& Player : Players)
		{
			Candidates.Add(Player.Candidate);
		}
		const FString Key = Selection.Choose(Candidates);
		for (const FPlayer& Player : Players)
		{
			if (Player.Candidate.Key == Key)
			{
				Chosen = &Player;
			}
		}
	}

	APawn* Pawn = Chosen ? Chosen->Pawn.Get() : nullptr;
	if (Chosen != nullptr)
	{
		CurrentKey = Chosen->Candidate.Key;
	}
	if (Pawn == nullptr)
	{
		if (Chosen != nullptr)
		{
			Status = FText::Format(LOCTEXT("NoPawn", "{0}: the player has no pawn yet"), Chosen->Candidate.Label);
		}
		Source.Reset();
		Markers.Reset();
		return false;
	}

	// The pawn can be replaced (respawn, possession) or the player can change: look again, and start the markers over for the new one
	bool bReuse = Source.IsValid() && Source->IsSourceValid();
	if (bReuse)
	{
		const UObject* Owner = Source->GetSourceObject();
		bReuse = Owner == nullptr || Owner->GetTypedOuter<APawn>() == Pawn;
	}
	if (!bReuse)
	{
		Source = FGSTN_InputSourceRegistry::Get().FindSource(*Pawn);
		Markers.Reset();
	}
	if (!Source.IsValid())
	{
		Status = FText::Format(LOCTEXT("NoSource", "{0}: nothing to read its input history from (no source registered for this pawn)"), Chosen->Candidate.Label);
		return false;
	}

	const FGSTN_InputHistory* History = Source->GetInputHistoryView();
	if (History == nullptr)
	{
		Status = FText::Format(LOCTEXT("NoHistory", "{0}: no input history yet"), Chosen->Candidate.Label);
		return false;
	}
	Status = Chosen->Candidate.Label;

	const int32 Latest = History->GetLatestFrame();
	Markers.RemoveAll([Latest](const FGSTN_InputDisplayMarker& Marker) { return Marker.Frame > Latest; });

	int32 RequestFrame = INDEX_NONE;
	FGameplayTag RequestCommand;
	if (Source->GetLatestCommandRequest(RequestFrame, RequestCommand) && RequestFrame != INDEX_NONE)
	{
		const bool bKnown = Markers.ContainsByPredicate([RequestFrame](const FGSTN_InputDisplayMarker& Marker) { return Marker.Frame == RequestFrame; });
		if (!bKnown)
		{
			Markers.Add({ RequestFrame, RequestCommand });
			if (Markers.Num() > MaxMarkers)
			{
				Markers.RemoveAt(0);
			}
		}
	}

	OutData.History = *History;
	OutData.Markers = Markers;
	return true;
}

#undef LOCTEXT_NAMESPACE
