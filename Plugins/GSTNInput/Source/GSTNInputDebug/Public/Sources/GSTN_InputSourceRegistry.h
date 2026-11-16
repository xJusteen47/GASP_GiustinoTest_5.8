// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "Debug/GSTN_InputHistorySource.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_InputSlot.h"

class APawn;

/**
 * What the display needs to know about the inputs of a game that no history can tell it: the buttons by bit (rows, letters, the names
 * of the notation) and the command set the scripted box resolves against. A source hands out its own (IGSTN_InputHistorySource::GetButtonTags,
 * GetScriptSlots); the registry keeps default profiles for a display that has no source yet (no play session), so the scripted box of the
 * editor tab works before a pawn exists.
 */
struct GSTNINPUTDEBUG_API FGSTN_InputDisplayProfile
{
	/** Button names by bit. The display always shows at least one row: an empty list means six buttons with no tag, A to F */
	TArray<FGameplayTag> ButtonTags;

	/** The command set of "what does this input do". Empty: the scripted box plays the notation and says no command set is known */
	TArray<FGSTN_InputSlotSpec> Slots;

	/** Where the profile comes from, for the line under the strip */
	FText Label;

	/** How many button rows: the tags, or six when there are none */
	int32 GetNumButtons() const { return ButtonTags.IsEmpty() ? 6 : ButtonTags.Num(); }
};

/** Makes the source for a pawn, or null when the pawn has nothing of the kind this finder knows */
using FGSTN_InputSourceFinder = TFunction<TSharedPtr<IGSTN_InputHistorySource>(APawn& Pawn)>;

/** Makes a default profile (called when the display has no source) */
using FGSTN_InputProfileFactory = TFunction<FGSTN_InputDisplayProfile()>;

/**
 * Where the debug modules that know a component type tell the input display how to read it. GSTNInputDebug knows only the interface:
 * GSTNCombatDebug registers a finder for the combat component, GSTNInputNPDebug one for the standalone input component, and a game can
 * register one for its own. A pawn is asked first through IGSTN_InputHistoryProvider on its components (a component that implements the
 * interface needs no finder), then through the finders in the order they were registered; the first source wins.
 *
 * Registered at module startup and removed at shutdown, by key (the module's name). Game thread only.
 */
class GSTNINPUTDEBUG_API FGSTN_InputSourceRegistry
{
public:
	static FGSTN_InputSourceRegistry& Get();

	/** Adds or replaces the finder under Key */
	void RegisterFinder(FName Key, FGSTN_InputSourceFinder Finder);
	void UnregisterFinder(FName Key);

	/** Adds or replaces the default profile under Key */
	void RegisterDefaultProfile(FName Key, FGSTN_InputProfileFactory Factory);
	void UnregisterDefaultProfile(FName Key);

	/** The source of a pawn: its provider components first, then the finders. Null when nothing can read this pawn */
	TSharedPtr<IGSTN_InputHistorySource> FindSource(APawn& Pawn) const;

	/** How many finders and default profiles are registered, for tests */
	int32 GetNumFinders() const { return Finders.Num(); }
	int32 GetNumDefaultProfiles() const { return Profiles.Num(); }

	/** The first registered default profile that names buttons, or the generic one (six buttons, no command set) */
	FGSTN_InputDisplayProfile GetDefaultProfile() const;

private:
	struct FFinderEntry
	{
		FName Key;
		FGSTN_InputSourceFinder Finder;
	};
	struct FProfileEntry
	{
		FName Key;
		FGSTN_InputProfileFactory Factory;
	};
	TArray<FFinderEntry> Finders;
	TArray<FProfileEntry> Profiles;
};
