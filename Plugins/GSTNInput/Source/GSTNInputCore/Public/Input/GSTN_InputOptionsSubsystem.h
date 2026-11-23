// Per-player input options (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Input/GSTN_StickThresholds.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GSTN_InputOptionsSubsystem.generated.h"

/** What one player chose, as it is saved */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_InputOptions
{
	GENERATED_BODY()

	/**
	 * The move stick's dead zone (owner 2026-11-16: one dead zone, the player's): under this stick length nothing happens, above it the
	 * length is remapped to 0..1 (radial), so a light tilt past it walks slowly from zero. Applied where the stick is read for the input
	 * command (UGSTN_PlayerInputSource), before anything else reads it: the input mapping context keeps only a tiny hardware floor.
	 * Local only: it shapes what this machine sends, so every player in a split screen has their own and it costs the simulation nothing
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Stick", meta = (ClampMin = "0", ClampMax = "0.5"))
	float MoveStickDeadZone = 0.08f;

	/** The attack zones and flick window: read by the simulation, so they go to the server in the loadout (FGSTN_LoadoutInputFeelExt) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Stick")
	FGSTN_StickThresholds StickThresholds;
};

/** Save slot of one player's options */
UCLASS()
class GSTNINPUTCORE_API UGSTN_InputOptionsSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FGSTN_InputOptions Options;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FGSTN_OnInputOptionsChanged, const FGSTN_InputOptions& /*Options*/);

/**
 * One local player's input options (input-opzioni-per-giocatore IO-1), saved per local player index ("GSTN_InputOptions_<index>"). Never in
 * Project Settings: the defaults are FGSTN_InputOptions' own. Console: gstn.Input.Options (no argument prints, "<field> <value>" sets and
 * saves for local player 0; fields DeadZone, Neutral, Aim, Flick).
 */
UCLASS()
class GSTNINPUTCORE_API UGSTN_InputOptionsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Input|Options")
	const FGSTN_InputOptions& GetOptions() const { return Options; }

	/** Replaces the options, saves them and tells the listeners (the loadout sends the stick thresholds to the server) */
	UFUNCTION(BlueprintCallable, Category = "Input|Options")
	void SetOptions(const FGSTN_InputOptions& NewOptions);

	FGSTN_OnInputOptionsChanged OnChanged;

	/** The stick after a radial dead zone: zero under it, the length remapped from DeadZone..1 to 0..1 above it, direction kept */
	static FVector2D ApplyRadialDeadZone(const FVector2D& Stick, float DeadZone);

	/** The options of a local player (defaults when there is no subsystem, e.g. a server's remote player) */
	static FGSTN_InputOptions GetFor(const class ULocalPlayer* LocalPlayer);

private:
	FString GetSlotName() const;

	FGSTN_InputOptions Options;
};
