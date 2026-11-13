// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class SWidget;
class UGameViewportClient;

/**
 * The corner overlay of the input display: while the console variable gstn.Input.Debug.InputDisplay is not 0 (or its older name
 * gstn.Combat.Debug.InputDisplay, which GSTNCombatDebug forwards to it), every game or PIE viewport (a listen-server host and each
 * client window of a PIE session have their own) gets the strip in its lower right corner, fed by the local player of that viewport's
 * world. Off by default: with the variable at 0 nothing is created and the ticker only reads the variable five times a second.
 *
 * The widgets are added with UGameViewportClient::AddViewportWidgetContent and are hit-test invisible, so they never take the mouse
 * from the game. They are as wide as the viewport allows up to a limit, so the columns keep their size and the newest frame stays at
 * the right. Game thread only; Start and Stop belong to the module.
 */
class FGSTN_InputDisplayOverlay
{
public:
	void Start();
	void Stop();

private:
	bool Update(float DeltaTime);
	void RemoveAll();

	struct FEntry
	{
		TWeakObjectPtr<UGameViewportClient> Viewport;
		TSharedPtr<SWidget> Widget;
	};
	TArray<FEntry> Entries;
	FTSTicker::FDelegateHandle TickerHandle;
};
