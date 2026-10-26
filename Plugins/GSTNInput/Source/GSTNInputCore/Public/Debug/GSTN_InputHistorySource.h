// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputSlot.h"
#include "UObject/Interface.h"
#include "GSTN_InputHistorySource.generated.h"

/**
 * What a debug view needs from anything that owns an input history: the history itself and, if it knows them, the commands that came out
 * of it, the button names and the command set. A plain interface with only const reads: a view never writes the simulation, and an
 * implementation only forwards to the state it already keeps. It lives in the Core module (not in the debug module) so that a runtime
 * class can implement it without depending on something that is left out of Shipping builds.
 *
 * Ways in: a pawn component implements IGSTN_InputHistoryProvider and hands out its source, or a debug module registers a finder for a
 * component type it does not own (GSTNCombatDebug does this for the combat component, GSTNInputNPDebug for the standalone input component).
 * Game thread only.
 */
class IGSTN_InputHistorySource
{
public:
	virtual ~IGSTN_InputHistorySource() = default;

	/** Does the thing this reads still exist and have a state? A source that says no is dropped and looked for again */
	virtual bool IsSourceValid() const = 0;

	/** The history as the simulation has it now, null until it has a state. Read, never written */
	virtual const FGSTN_InputHistory* GetInputHistoryView() const = 0;

	/**
	 * The command the simulation chose most recently: the frame it was requested on and its tag. False when none, or when the source
	 * cannot tell (the display then shows no command markers). The display adds a marker per new frame and forgets the ones after a rollback.
	 */
	virtual bool GetLatestCommandRequest(int32& OutFrame, FGameplayTag& OutCommand) const { return false; }

	/** The buttons by bit (the rows, the letters A.. of the notation). Leave it empty and the display uses six unnamed buttons */
	virtual void GetButtonTags(TArray<FGameplayTag>& OutTags) const {}

	/** The command set "what does this input do" resolves against. False: no lookup, the scripted box only plays the notation */
	virtual bool GetScriptSlots(TArray<FGSTN_InputSlotSpec>& OutSlots) const { return false; }

	/** A short name for the picker and the strip, e.g. the pawn */
	virtual FText GetSourceLabel() const { return FText::GetEmpty(); }

	/** The object the source reads, to tell "the same source" from "a new one" (a respawn replaces the pawn) */
	virtual const UObject* GetSourceObject() const { return nullptr; }
};

UINTERFACE(MinimalAPI, NotBlueprintable)
class UGSTN_InputHistoryProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by a pawn component that wants the debug views to find its input history without a registered finder. The debug module
 * asks every component of a local pawn for it. Return null while there is nothing to read.
 */
class GSTNINPUTCORE_API IGSTN_InputHistoryProvider
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<IGSTN_InputHistorySource> CreateInputHistorySource() = 0;
};
