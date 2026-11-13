// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "Display/GSTN_InputDisplayLayout.h"
#include "Sources/GSTN_InputSourceRegistry.h"
#include "Sources/GSTN_InputTargetSelection.h"

class APawn;
class UWorld;

/**
 * Feeds the input display with what this machine knows about a local player: the history of the pawn a local player controller
 * possesses, read through the IGSTN_InputHistorySource the registry finds for that pawn (a provider component, or a finder registered by
 * the combat or the input NP debug module), and the commands that came out of it. Read-only: it copies the history and never writes or
 * listens to the simulation, so it shows the same thing on a dedicated-server client, a listen-server client and the listen-server host.
 *
 * Which player it reads: with a world given (the corner overlay: one per viewport) that world's first local player; without one (the
 * editor tab) the player chosen by FGSTN_InputTargetSelection among the local players of every PIE and game world, by default the one
 * of the window that has the focus or had it last.
 *
 * The command markers are not taken from UGSTN_SimEventSubsystem: the event is published on the authority only, so a client would never
 * see its own commands. They come from IGSTN_InputHistorySource::GetLatestCommandRequest, which every machine has; each new request frame
 * adds one marker, and a marker past the newest frame (a re-simulation took it back) is dropped. A command requested and replaced inside
 * one polling interval can be missed: the display polls once per frame.
 *
 * Game thread only.
 */
class FGSTN_LocalInputSource
{
public:
	/** World to read; null = the local players of every game or PIE world, chosen by the selection (the editor tab) */
	explicit FGSTN_LocalInputSource(UWorld* InWorld = nullptr) : World(InWorld) {}

	/** Fills OutData from the chosen player's pawn. False (and OutData untouched) when there is none yet */
	bool Fetch(FGSTN_InputDisplayData& OutData);

	/** The profile of the source the last Fetch read (its buttons and command set), the registry's default one when there is no source */
	FGSTN_InputDisplayProfile GetProfile() const;

	/** The source the last Fetch read, null if none */
	const TSharedPtr<IGSTN_InputHistorySource>& GetSource() const { return Source; }

	/** What the strip says it shows, or why it shows nothing ("No local player: start a play session") */
	FText GetStatusText() const { return Status; }

	/** Which player an editor tab follows */
	FGSTN_InputTargetSelection& GetSelection() { return Selection; }
	const FString& GetCurrentKey() const { return CurrentKey; }

	/** Every local player of every game and PIE world, for the picker and the selection */
	static void ListPlayers(TArray<FGSTN_InputTargetCandidate>& OutCandidates);

private:
	/** A local player resolved to its pawn */
	struct FPlayer
	{
		FGSTN_InputTargetCandidate Candidate;
		TWeakObjectPtr<APawn> Pawn;
	};

	static void CollectPlayers(UWorld* OnlyWorld, TArray<FPlayer>& OutPlayers);

	TWeakObjectPtr<UWorld> World;
	FGSTN_InputTargetSelection Selection;
	FString CurrentKey;
	FText Status;

	TSharedPtr<IGSTN_InputHistorySource> Source;
	TArray<FGSTN_InputDisplayMarker> Markers;

	static constexpr int32 MaxMarkers = 32;
};
