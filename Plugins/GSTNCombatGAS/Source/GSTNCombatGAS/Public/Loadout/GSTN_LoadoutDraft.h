// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Loadout/GSTN_LoadoutChoices.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UGSTN_LoadoutComponent;

/**
 * The loadout screen without its widget (M11 loadout screen): the draft loadout, its rows (UGSTN_LoadoutComponent::GatherChoices) and a
 * cursor. Up and down move the cursor over the rows, left and right pick the previous or next option of the cursor's row that the
 * server's rules would accept; every pick rebuilds the rows, because a new kit brings other specials and mod slots. Nothing is sent: the
 * screen sends GetData() with UGSTN_LoadoutComponent::ServerRequestLoadout when the player confirms, and the server judges it again.
 * Local and out of band; a pick resolves assets, so it runs on a button press, never per tick.
 */
struct GSTNCOMBATGAS_API FGSTN_LoadoutDraft
{
	/** Starts from Start (the applied loadout, or the player's saved one). A Start without a valid kit takes the first kit the rules accept */
	void Begin(const UGSTN_LoadoutComponent& InComponent, const FGSTN_LoadoutData& Start);

	/** Moves the cursor by Step rows, wrapping. False when there are no rows */
	bool MoveCursor(int32 Step);

	/** Picks the option Step places after the cursor row's current one, skipping the ones the rules refuse, wrapping. False when nothing changed */
	bool Cycle(int32 Step);

	const FGSTN_LoadoutData& GetData() const { return Data; }
	const TArray<FGSTN_LoadoutChoiceRow>& GetRows() const { return Rows; }
	int32 GetCursor() const { return Cursor; }

	/** Whether the server's rules would accept the draft as it is, and the first finding when not */
	bool IsValid() const { return bValid; }
	const FString& GetProblem() const { return Problem; }

private:
	/** Gathers the rows for Data again and keeps the cursor on the row it was on (by id) */
	void Rebuild();

	TWeakObjectPtr<const UGSTN_LoadoutComponent> Component;
	FGSTN_LoadoutData Data;
	TArray<FGSTN_LoadoutChoiceRow> Rows;
	int32 Cursor = 0;
	bool bValid = false;
	FString Problem;
};
