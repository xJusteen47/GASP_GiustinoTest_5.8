// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_ButtonMap.h"
#include "InputCoreTypes.h"
#include "GSTN_InputConfig.generated.h"

class UInputAction;
class UInputMappingContext;
class IEnhancedInputSubsystemInterface;

/** One logical button: its identity in data (the tag) and the Enhanced Input action that drives it. */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_InputButtonDef
{
	GENERATED_BODY()

	/** Identity of the button in conditions and slot data, e.g. Input.Button.Attack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button", meta = (Categories = "Input.Button"))
	FGameplayTag Tag;

	/**
	 * The action whose Started/held state is the button. Empty for a button only AI or scripts press. Mapped in any
	 * mapping context the player has: the fork's ability input mapping is not involved. A soft reference, so the config does not
	 * pull the input actions of a project into every map that mentions it: UGSTN_InputConfig::ResolveActions loads them once
	 * at setup and the input source reads the resolved ones
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button")
	TSoftObjectPtr<UInputAction> Action;
};

/** A mapping context the player gets while this config drives their pawn, and its priority */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_InputContextEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	TSoftObjectPtr<UInputMappingContext> Context;

	/** Higher wins a key mapped by both (with a consuming action): the gameplay contexts sit low, menus far above */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context")
	int32 Priority = 0;
};

/**
 * The buttons of a character's input. The array index is the bit of the button in every input mask (command,
 * history, sync state), so the same config must be used by every machine and its order must not change while a match is
 * running. Conditions refer to buttons by tag and are bound to bits through this config, which keeps slot sets reusable
 * across configs.
 */
UCLASS(BlueprintType)
class GSTNINPUTCORE_API UGSTN_InputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Bits available in the input masks */
	static constexpr int32 MaxButtons = 32;

	/** Index (= bit) of a button, INDEX_NONE if the config does not have it */
	UFUNCTION(BlueprintPure, Category = "Input")
	int32 FindButtonIndex(const FGameplayTag& ButtonTag) const;

	/** Bit mask of a button, 0 if the config does not have it */
	uint32 GetButtonMask(const FGameplayTag& ButtonTag) const;

	/** The tag -> bit lookup the pattern matcher binds slot data to this config with */
	FGSTN_ButtonMap MakeButtonMap() const;

	/** The buttons, in bit order */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TArray<FGSTN_InputButtonDef> Buttons;

	/**
	 * The 2D move action, read as the stick on the controller (+Y up) by the player input source. Unset: the stick is
	 * always neutral. It sits next to the buttons so one asset describes everything the player drives the simulation with
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> MoveAction;

	/**
	 * Whether the player input source also sends the control rotation's yaw and pitch at 65536 steps per turn
	 * (FGSTN_InputCmd::AimYaw/AimPitch), which an ability reads as its aim (a ranged attack, a hold-to-aim stance). Off: the command
	 * carries one bit more than before and nothing else. The coarse camera yaw byte the direction classifier uses is always sent
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	bool bSendCameraAim = false;

	/**
	 * The 2D aim action (the right stick, +Y up), sent as FGSTN_InputCmd::AimStickX/Y: it steers a move during its startup
	 * (plan precisione-weakpoint 5). A gyro device can feed the same action; the simulation does not know where it comes from.
	 * Unset: the aim stick is always centred and nothing steers
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> AimStickAction;

	/**
	 * The precision reticle's on/off button (the right stick click, plan precisione-weakpoint 4). Local presentation only: it never enters the
	 * command, so it costs nothing on the wire and the simulation does not know the reticle exists. Read by UGSTN_PrecisionAimComponent
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> ReticleToggleAction;

	/**
	 * Designers set nothing for the right stick: while AimStickAction or ReticleToggleAction is unset, ResolveActions makes a transient action for
	 * it, bound to AutoAimStickKey / AutoReticleToggleKey in a transient mapping context (GetAutoMappingContext) that the player input source
	 * adds for the local player. The actions do not consume their keys, so the camera keeps the right stick. Off: an unset action stays unset
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Auto")
	bool bAutoAimActions = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Auto", meta = (EditCondition = "bAutoAimActions"))
	FKey AutoAimStickKey = EKeys::Gamepad_Right2D;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Auto", meta = (EditCondition = "bAutoAimActions"))
	FKey AutoReticleToggleKey = EKeys::Gamepad_RightThumbstick;

	/** Priority of the automatic mapping context: above the game's own, harmless because its actions let the keys through */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Auto", meta = (EditCondition = "bAutoAimActions"))
	int32 AutoMappingPriority = 100;

	/**
	 * The mapping contexts that give the player their keys while this config drives the pawn (the keys of the buttons and of MoveAction, the
	 * camera's look). Added to the pawn's local player by the player input source and kept there: the keys are data, per config, never code
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Contexts")
	TArray<FGSTN_InputContextEntry> MappingContexts;

	/**
	 * Contexts of the base game that this config's own replace, removed from the local player whenever they show up (the Game Animation
	 * Sample's IMC_Sandbox, which its character Blueprint adds at BeginPlay and which maps the pad to GASP's own actions: next pawn, ragdoll,
	 * walk, strafe...). Removing the whole context, not single keys, keeps the base game's Blueprints untouched
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Contexts")
	TArray<TSoftObjectPtr<UInputMappingContext>> ReplacedMappingContexts;

	/**
	 * Loads the input actions (synchronously, if they are not loaded yet) and keeps them for the getters below. Call it once
	 * when the host sets up (BeginPlay, a loadout change), never from a tick: the input source reads the resolved actions every
	 * frame and does not resolve anything itself. Calling it again after the asset changed refreshes them.
	 */
	void ResolveActions();

	/** The resolved action of a button, null if it has none or ResolveActions was not called */
	UInputAction* GetButtonAction(int32 ButtonIndex) const;

	/** The resolved move action, null if unset or ResolveActions was not called */
	UInputAction* GetMoveAction() const { return ResolvedMoveAction; }

	/** The resolved aim stick action, null if unset or ResolveActions was not called */
	UInputAction* GetAimStickAction() const { return ResolvedAimStickAction; }

	/** The resolved reticle toggle action, null if unset (and not made automatically) or ResolveActions was not called */
	UInputAction* GetReticleToggleAction() const { return ResolvedReticleToggleAction; }

	/** The transient mapping context of the automatic actions, null when none was made. The local player adds it once (UGSTN_PlayerInputSource) */
	UInputMappingContext* GetAutoMappingContext() const { return AutoMappingContext; }

	/**
	 * Puts the resolved MappingContexts on a local player's input and takes the ReplacedMappingContexts off it, when they are not already so.
	 * Cheap when nothing changed (lookups only), so the player input source calls it every frame: a Blueprint that adds its context later
	 * (BeginPlay, a possess) loses it again within a frame. Local presentation, never simulation state
	 */
	void ApplyMappingContexts(IEnhancedInputSubsystemInterface& Input) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** Strong references to the loaded actions, parallel to Buttons: a soft pointer alone does not keep an asset alive */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> ResolvedButtonActions;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ResolvedMoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ResolvedAimStickAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ResolvedReticleToggleAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> AutoMappingContext;

	/** MappingContexts, loaded by ResolveActions, with their priorities in the same order */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputMappingContext>> ResolvedContexts;

	TArray<int32> ResolvedContextPriorities;

	/** ReplacedMappingContexts, loaded by ResolveActions */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputMappingContext>> ResolvedReplacedContexts;
};
