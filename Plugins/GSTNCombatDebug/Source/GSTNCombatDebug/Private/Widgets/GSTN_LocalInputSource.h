// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/GSTN_InputDisplayLayout.h"

class UGSTN_CombatSimComponent;
class UWorld;

/**
 * Feeds the input display with what this machine knows about its local player: the input history of the combat component of the
 * pawn a local player controller possesses, and the commands that came out of it. Read-only: it copies the history through
 * UGSTN_CombatSimComponent::GetInputHistoryView and never writes or listens to the simulation, so it shows the same thing on a
 * dedicated-server client, a listen-server client and the listen-server host.
 *
 * The command markers are not taken from UGSTN_SimEventSubsystem: the event is published on the authority only, so a client would
 * never see its own commands. They come from the request the sync state holds (the frame of the request and the binding it chose),
 * which every machine has; each new request frame adds one marker, and a marker past the newest frame (a re-simulation took it back)
 * is dropped. A command requested and replaced inside one polling interval can be missed: the display polls once per frame.
 *
 * Game thread only.
 */
class FGSTN_LocalInputSource
{
public:
	/** World to read; null = the first game or PIE world that has a local player with a combat component (the editor tab) */
	explicit FGSTN_LocalInputSource(UWorld* InWorld = nullptr) : World(InWorld) {}

	/** Fills OutData from the local pawn. False (and OutData untouched) when there is none yet */
	bool Fetch(FGSTN_InputDisplayData& OutData);

	/** The combat component the last Fetch read, null if none */
	UGSTN_CombatSimComponent* GetComponent() const { return Component.Get(); }

	/** The combat component of the first local player of the world, null if it has none */
	static UGSTN_CombatSimComponent* FindLocalComponent(const UWorld* InWorld);

	/** The same over every game and PIE world, first found */
	static UGSTN_CombatSimComponent* FindLocalComponentInAnyWorld();

private:
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UGSTN_CombatSimComponent> Component;
	TArray<FGSTN_InputDisplayMarker> Markers;

	static constexpr int32 MaxMarkers = 32;
};
