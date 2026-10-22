// Producers of the input command (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Simulation/GSTN_InputSimTypes.h"
#include "UObject/Interface.h"
#include "GSTN_InputSource.generated.h"

class AActor;
class UEnhancedInputComponent;
class UGSTN_InputConfig;
struct FGSTN_CommandTable;
struct FInputActionInstance;

/** What a source may look at when it produces a command. Valid for the call only. */
struct FGSTN_InputSourceContext
{
	/** The pawn the command is for */
	AActor* Owner = nullptr;

	/** The buttons (bit i = Buttons[i]) and the move action */
	const UGSTN_InputConfig* Config = nullptr;

	/** The compiled commands, to turn a command tag into the index ForcedCommand carries */
	const FGSTN_CommandTable* Table = nullptr;
};

UINTERFACE(MinimalAPI)
class UGSTN_InputSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * Fills the input command of one fixed tick. Called by the host's ProduceInput on the machine that controls the pawn
 * (the owning client, or the server for AI), never during a re-simulation: the command it makes is what Network Prediction
 * stores, sends and replays. Writes every field it owns of Out (the host resets it first); anything a source cannot know
 * stays 0.
 */
class GSTNINPUTNP_API IGSTN_InputSource
{
	GENERATED_BODY()

public:
	virtual void ProduceInput(const FGSTN_InputSourceContext& Context, int32 DeltaTimeMS, FGSTN_InputCmd& Out) = 0;
};

/**
 * The local player: held buttons and the stick from Enhanced Input, taps from the buttons' Started events.
 *
 * Held state and the stick are read from the player input at production time with GetActionValue, which also sees injected
 * input (the network test harness presses buttons that way). A tap shorter than a tick would be lost by that read alone, so
 * the Started event of every button action is bound on the controller's input component once there is one, and ORed into
 * the command's pressed mask. The mapping contexts only need to be active for the player (the fork's ability input is not
 * involved). Does nothing on a machine where the pawn is not locally controlled.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class GSTNINPUTNP_API UGSTN_PlayerInputSource : public UObject, public IGSTN_InputSource
{
	GENERATED_BODY()

public:
	virtual void ProduceInput(const FGSTN_InputSourceContext& Context, int32 DeltaTimeMS, FGSTN_InputCmd& Out) override;
	virtual void BeginDestroy() override;

private:
	/** Binds the Started events on the controller's input component, again if the component or the config changed */
	void EnsureBound(UEnhancedInputComponent& InputComponent, const UGSTN_InputConfig& Config);
	void Unbind();
	void HandleButtonStarted(const FInputActionInstance& Instance);

	TWeakObjectPtr<UEnhancedInputComponent> BoundComponent;
	TWeakObjectPtr<const UGSTN_InputConfig> BoundConfig;
	TArray<uint32> BindingHandles;

	/** Bit i = button i started since the last command */
	uint32 PendingTaps = 0;
};

/**
 * Input made by code: a behaviour tree task, a state tree, a script, a test. Holds buttons and a stick until told
 * otherwise, taps buttons for one command, and can ask for a command by name instead of synthesising its input
 * (ForcedCommand). The stick is a vector in the controller's frame sent with camera yaw 0, so with the settings' default
 * world reference "up" is the world's reference direction, not a camera-relative one.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class GSTNINPUTNP_API UGSTN_AIInputSource : public UObject, public IGSTN_InputSource
{
	GENERATED_BODY()

public:
	virtual void ProduceInput(const FGSTN_InputSourceContext& Context, int32 DeltaTimeMS, FGSTN_InputCmd& Out) override;

	/** Buttons held from now until changed (bit i = button i of the input config) */
	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetHeld(int32 Mask);

	/** Presses the buttons for the next command only, even if they were never seen held (a tap shorter than a tick) */
	UFUNCTION(BlueprintCallable, Category = "Input")
	void Tap(int32 Mask);

	/** The stick from now until changed, clamped to the unit circle (+Y up) */
	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetStick(FVector2D WorldPlane);

	/** Asks for a command by tag in the next command only. It still needs a binding that holds in the current context */
	UFUNCTION(BlueprintCallable, Category = "Input")
	void RequestCommand(FGameplayTag Command);

private:
	uint32 Held = 0;
	uint32 Taps = 0;
	FVector2D Stick = FVector2D::ZeroVector;
	FGameplayTag ForcedCommand;
};
