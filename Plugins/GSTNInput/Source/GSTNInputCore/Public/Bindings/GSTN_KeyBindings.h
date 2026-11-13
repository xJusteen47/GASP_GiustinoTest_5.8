// Per local player key bindings (local only: keys never reach the simulation or the network).

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/SaveGame.h"
#include "GSTN_KeyBindings.generated.h"

class UEnhancedInputUserSettings;
class UInputMappingContext;

/**
 * One key a player moved: "the mapping of Action whose default key is DefaultKey now uses Key".
 *
 * A mapping is named by its action and its default key, not by a slot index, so the same override still finds its mapping
 * when the mapping context gains or loses rows, and a keyboard row and a pad row of the same action are told apart by
 * their default keys. Action is the Input Action's object name (the row name of the Enhanced Input user settings).
 */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_KeyBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Bindings")
	FName Action;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Bindings")
	FKey DefaultKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Bindings")
	FKey Key;

	bool operator==(const FGSTN_KeyBinding& Other) const = default;
};

/**
 * The overrides of one local player: only what differs from the mapping contexts. An empty set is the default layout,
 * so a new mapping context row needs no migration and "reset to default" is just emptying the set.
 */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_KeyBindingSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Bindings")
	TArray<FGSTN_KeyBinding> Overrides;

	/** Moves one mapping to NewKey; moving it back to its default key removes the override. False if nothing changed */
	bool Rebind(FName Action, const FKey& DefaultKey, const FKey& NewKey);

	/** Puts one mapping back on its default key. False if it was not moved */
	bool Reset(FName Action, const FKey& DefaultKey);

	/** Every override removed */
	void ResetAll() { Overrides.Reset(); }

	/** The key a mapping uses for this player: the override, else the default */
	FKey Resolve(FName Action, const FKey& DefaultKey) const;

	bool IsCustomized(FName Action, const FKey& DefaultKey) const { return Find(Action, DefaultKey) != nullptr; }

	const FGSTN_KeyBinding* Find(FName Action, const FKey& DefaultKey) const;

	bool operator==(const FGSTN_KeyBindingSet& Other) const = default;
};

/** One rebindable mapping as a menu lists it */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_BindableMapping
{
	GENERATED_BODY()

	/** Input Action object name: pass it back to Rebind / ResetBinding */
	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	FName Action;

	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	FKey DefaultKey;

	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	FKey CurrentKey;

	/** Mapping context the row comes from, for grouping in a menu */
	UPROPERTY(BlueprintReadOnly, Category = "Input|Bindings")
	FName Context;
};

/** Save file of one local player's overrides (own slot per player, see UGSTN_KeyBindingsSubsystem) */
UCLASS()
class GSTNINPUTCORE_API UGSTN_KeyBindingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	FGSTN_KeyBindingSet Bindings;

	TArray<uint8> ToBytes();
	static UGSTN_KeyBindingsSaveGame* FromBytes(const TArray<uint8>& Bytes, UObject* Outer);
};

/**
 * Bridge to the engine's Enhanced Input user settings (stateless, so it is tested without a player or a world).
 */
struct GSTNINPUTCORE_API FGSTN_KeyBindingsApply
{
	/**
	 * Gives every mapping of the context a player-mappable identity when its asset has none: a transient
	 * UPlayerMappableKeySettings named after the action. Identity only (same for every player, never saved: RF_Transient),
	 * so no Input Action asset has to be edited by hand for its keys to be rebindable. A mapping that ignores settings on
	 * purpose (IgnoreSettings) stays fixed.
	 */
	static void EnsureMappable(const UInputMappingContext* Context);

	/** Mapping name the user settings use for this mapping, NAME_None when it is not rebindable */
	static FName GetMappingName(const struct FEnhancedActionKeyMapping& Mapping);

	/** Row of a mapping that shares its action with another mapping of the same key kind (see EnsureMappable) */
	static FName MakeRowName(FName Action, const FKey& DefaultKey);

	/** Same kind of key for the engine's row query: keyboard and mouse, pad, touch */
	static bool IsSameKeyKind(const FKey& A, const FKey& B);

	/** The user-settings mapping of (Action, DefaultKey): its own row if it has one, else the action's row. Null if none */
	static const struct FPlayerKeyMapping* FindRowMapping(const UEnhancedInputUserSettings& Settings, FName Action, const FKey& DefaultKey);

	/**
	 * Puts the player's active key profile on the defaults, then maps every override whose row exists. The contexts must
	 * already be registered with Settings. Returns how many overrides were applied (the others name rows no context has).
	 */
	static int32 ApplyToUserSettings(UEnhancedInputUserSettings& Settings, const FGSTN_KeyBindingSet& Bindings);

	/** The key the active profile uses for one mapping (its default when not moved), EKeys::Invalid if no such row */
	static FKey GetCurrentKey(const UEnhancedInputUserSettings& Settings, FName Action, const FKey& DefaultKey);
};
