// What an ability may ask the component that started it (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "GSTN_CommandSourceInterface.generated.h"

class AActor;
struct FGSTN_CommandTable;
struct FGSTN_InputSimState;

/** Where the activation of an ability came from */
UENUM(BlueprintType)
enum class EGSTN_ActivationSource : uint8
{
	/** The player's (or an AI's) input matched the command's pattern */
	Command,

	/** A command requested by name (AI, script): no input behind it */
	Forced,

	/** Taken over from another timeline (combat milestone 3) */
	Adopted,

	/** Not started by a command at all */
	Other
};

/**
 * Why an ability is running, as the input simulation knew it on the frame it requested it (M2 section 7a). An ability reads
 * it once in ActivateAbility and keeps what it needs in its own sync variables: the host only holds the request of the frame
 * being simulated, so a later call returns an invalid context.
 */
USTRUCT(BlueprintType)
struct GSTNINPUTNP_API FGSTN_ActivationContext
{
	GENERATED_BODY()

	/** The command that started the ability */
	UPROPERTY(BlueprintReadOnly, Category = "Activation")
	FGameplayTag Command;

	/** Frame of the press that completed the command (the request's frame for a forced command), INDEX_NONE when invalid */
	UPROPERTY(BlueprintReadOnly, Category = "Activation")
	int32 PressFrame = INDEX_NONE;

	/** Index of the button whose press completed the command in the input config, -1 for none (forced, or a pattern without a press). An int32, not the byte of the wire format, so Blueprint can hold it */
	UPROPERTY(BlueprintReadOnly, Category = "Activation")
	int32 Button = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Activation")
	EGSTN_ActivationSource Source = EGSTN_ActivationSource::Other;

	/** Whether a command started the ability this frame */
	bool IsValid() const { return Command.IsValid(); }

	/**
	 * The context of the request State holds, if it was made on Frame (the frame being simulated: the ability system ticks
	 * right after the input simulation, in the same frame); an invalid one otherwise. Pure: the table gives the command, the
	 * state the button, and the history the frame of the press (the newest press of that button at or before the request,
	 * which is the terminal one unless the same button was pressed again within the buffer).
	 */
	static FGSTN_ActivationContext FromRequest(const FGSTN_InputSimState& State, const FGSTN_CommandTable& Table, int32 Frame);
};

UINTERFACE(MinimalAPI)
class UGSTN_CommandSourceInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by every component that runs the input simulation (the standalone input component, the combat component),
 * so an ability finds what it needs on its avatar without knowing which one it is.
 *
 * Both queries describe the frame being simulated: call them from inside the simulation (ActivateAbility, an ability's
 * tick), where the ability system and the input simulation are on the same frame. Reading them from presentation code
 * returns the latest simulated state instead, which is fine for UI but not for anything that must stay deterministic.
 */
class GSTNINPUTNP_API IGSTN_CommandSourceInterface
{
	GENERATED_BODY()

public:
	/** Why the ability being activated now started; invalid if no command requested an ability this frame */
	virtual FGSTN_ActivationContext GetActivationContext() const = 0;

	/** Whether the button (index in the input config) is held on this frame, from the input history. Lets an ability end on release (a stance, hold to aim) without ActivationInputs */
	virtual bool IsCommandButtonHeld(int32 Button) const = 0;

	/** The first component of Actor that implements the interface, null if none */
	static IGSTN_CommandSourceInterface* Find(const AActor* Actor);
};
