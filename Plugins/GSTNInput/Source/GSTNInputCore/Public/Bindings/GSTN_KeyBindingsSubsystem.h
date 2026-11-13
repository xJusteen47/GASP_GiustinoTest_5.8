// Per local player key bindings (local only: keys never reach the simulation or the network).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Bindings/GSTN_KeyBindings.h"
#include "GSTN_KeyBindingsSubsystem.generated.h"

class UInputMappingContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSTN_OnKeyBindingsChanged);

/**
 * Each local player's physical key -> Input Action layout, the API a future options menu calls (list, rebind, reset, save,
 * load). Split-screen players each have their own instance, overrides and save slot.
 *
 * Why it cannot touch the game: keys stop at the Input Action. The ability system binds actions, not keys, and only the
 * logical button `Input.Button.*` an action produces goes into the predicted input, so a moved key changes nothing in the
 * simulation, in prediction or on the wire, and the defaults (no override) are exactly today's mapping contexts.
 *
 * How it applies: the engine's Enhanced Input user settings (one per local player, created by the Enhanced Input local
 * player subsystem when the player gets a controller) hold the player-mapped keys and rebuild the player's key mappings.
 * Every mapping context a player gets is picked up on the next rebuild, made rebindable (FGSTN_KeyBindingsApply::
 * EnsureMappable), registered, and the overrides are applied. Nothing to wire: the subsystem exists for every local player.
 *
 * Why its own save: the engine settings save to one slot name for every local player, and the PC save system ignores the
 * user index, so two split-screen players would share a file. This subsystem saves only its overrides, in a slot named
 * after the local player index. The engine settings object is never saved by this code.
 */
UCLASS()
class GSTNINPUTCORE_API UGSTN_KeyBindingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Every rebindable mapping of the contexts this player has now, with its current key */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Input|Bindings")
	TArray<FGSTN_BindableMapping> ListBindableMappings() const;

	/** Moves one mapping to NewKey for this player only and applies it at once. Not saved until Save */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Input|Bindings")
	bool Rebind(FName Action, FKey DefaultKey, FKey NewKey);

	UFUNCTION(BlueprintCallable, Category = "GSTN|Input|Bindings")
	bool ResetBinding(FName Action, FKey DefaultKey);

	UFUNCTION(BlueprintCallable, Category = "GSTN|Input|Bindings")
	void ResetAllToDefault();

	/** The key this player uses for a mapping (override or default) */
	UFUNCTION(BlueprintPure, Category = "GSTN|Input|Bindings")
	FKey GetKey(FName Action, FKey DefaultKey) const { return Bindings.Resolve(Action, DefaultKey); }

	UFUNCTION(BlueprintPure, Category = "GSTN|Input|Bindings")
	const FGSTN_KeyBindingSet& GetBindings() const { return Bindings; }

	/** Replaces every override (a menu's "apply"); applied at once */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Input|Bindings")
	void SetBindings(const FGSTN_KeyBindingSet& NewBindings);

	UFUNCTION(BlueprintCallable, Category = "GSTN|Input|Bindings")
	bool Save();

	/** Reads the player's slot (no slot = defaults) and applies it */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Input|Bindings")
	bool Load();

	/** Slot of this player: one per local player index */
	FString GetSaveSlotName() const;

	UPROPERTY(BlueprintAssignable, Category = "GSTN|Input|Bindings")
	FGSTN_OnKeyBindingsChanged OnBindingsChanged;

protected:
	/** Storage, virtual so tests (and a cloud save) can replace it */
	virtual bool ReadStorage(TArray<uint8>& OutBytes) const;
	virtual bool WriteStorage(const TArray<uint8>& Bytes) const;

	/** Index in the slot name: the local player index (0 for the first player) */
	virtual int32 GetPlayerSlotIndex() const;

	/** Pushes the overrides into the engine user settings, registering new contexts first. No-op without them */
	void ApplyToEnhancedInput();

	UFUNCTION()
	void HandleControlMappingsRebuilt();

	UPROPERTY(Transient)
	FGSTN_KeyBindingSet Bindings;

	/** Contexts already registered with the user settings, so a rebuild only re-applies when a new one shows up */
	TSet<TWeakObjectPtr<const UInputMappingContext>> RegisteredContexts;

	bool bApplying = false;
};
