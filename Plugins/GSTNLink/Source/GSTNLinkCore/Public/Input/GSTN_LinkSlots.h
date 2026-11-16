// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Input/GSTN_CommandBindings.h"
#include "Input/GSTN_InputSlot.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_LinkSlots.generated.h"

class UGSTN_InputSlotSet;
enum class EGSTN_LinkThrowInputMode : uint8;
struct FGSTN_BindingInput;
struct FGSTN_LinkThrowSet;

/** What UGSTN_LinkSlots::ApplyKitThrows needs (M11 K2): the project's settings, the kit's own set and the blockers other plugins registered */
struct GSTNLINKCORE_API FGSTN_KitThrowFill
{
	EGSTN_LinkThrowInputMode Mode;
	/** Add a second grab row: special + crouch held */
	bool bSpecialCrouchGrab = false;
	/** The kit's own set (its link extension), or null */
	const FGSTN_LinkThrowSet* KitThrows = nullptr;
	/** The project's common set, or null */
	const FGSTN_LinkThrowSet* Fallback = nullptr;
	/** Slots from the settings that replace the mode's shape for their command (a throw or the special grab), or null */
	const TArray<FGSTN_InputSlotSpec>* SlotOverrides = nullptr;
	/** Tags that switch every kit throw off */
	FGameplayTagContainer BlockAll;
	/** Tags that switch off only the throws the stick alone triggers (forward and back in Stick mode) */
	FGameplayTagContainer BlockStickOnly;

	explicit FGSTN_KitThrowFill(EGSTN_LinkThrowInputMode InMode) : Mode(InMode) {}
};

/** The requirement a kit's binding of one link command carries (the slot is the input, the requirement the context it means something in) */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkSlotBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link", meta = (Categories = "Command.Link"))
	FGameplayTag Command;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link")
	FGSTN_TagRequirements Requirements;
};

/**
 * The link commands of the standard command set (m6 spec 8) as slot specs, beside the attack set (UGSTN_StandardSlots) and the movement set
 * (UGSTN_MovementSlots): every row is data, so a kit rebinds them without code.
 *
 *   Command.Link.Grab           Press(Interact)                                  binding requires Interact.Target.Grab
 *   Command.Link.Release        Press(Interact)                                  bindings require Role.Grabber (the grabber lets go) or Role.Latcher,
 *                                                                                each ignoring the other link roles (exclusive for the loadout validator)
 *   Command.Link.Pummel         Press(Attack), stick neutral or lightly pushed   requires Role.Grabber
 *   Command.Link.Throw.Forward  Press(Attack or Strong) + stick tilted forward    requires Role.Grabber
 *   Command.Link.Throw.Back     Press(Attack or Strong) + stick tilted back       requires Role.Grabber
 *   Command.Link.Throw.Up       Press(Strong), stick neutral                     requires Role.Grabber
 *   Command.Link.Throw.Down     Press(Attack) + Crouch held                      requires Role.Grabber (one above the pummel: the crouch shape wins)
 *   Command.Link.Latch.Attack   Press(Attack)                                    requires Role.Latcher
 *
 * Every link slot falls through when unavailable (bFallThroughWhenUnavailable): outside its context it must not reserve the press it shares with
 * a slot below it (the slot resolver otherwise keeps a matched but unavailable slot's press from the lower ones).
 *
 * Grab and Release share the press: the requirements make them exclusive (a grabber has no grab target, the probe projects none while linked),
 * so the same tap grabs and lets go. The role-gated slots carry a priority offset above the attack set's rank, so with the role held the link
 * command wins over Command.Attack.* on the same press; without the role their bindings are not candidates at all. The tap fires on the press
 * (spec 8: the grab must be responsive); the movement set's release-anchored Command.Interact.Grab stays a kit's alternative.
 */
UCLASS()
class GSTNLINKCORE_API UGSTN_LinkSlots : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Added to the role-gated slots' priority so they outrank the standard attack slots of the same press */
	static constexpr int32 RoleSlotPriorityOffset = 100;

	/** The slots listed above, in that order */
	static TArray<FGSTN_InputSlotSpec> Build();

	/** The binding requirement of each row above (Release twice: grabber and latcher), in that order */
	static TArray<FGSTN_LinkSlotBinding> DefaultBindings();

	/** The special grab's offset (M11 K2): above the down throw, so special + crouch never reads as anything else while a target is framed */
	static constexpr int32 SpecialGrabPriorityOffset = RoleSlotPriorityOffset + 2;

	/**
	 * The slot of one throw command in an input mode (M11 K2, see EGSTN_LinkThrowInputMode). Buttons = the row of Build. Stick: forward and
	 * back are a one-element Motion (the stick ENTERING the facing sector at tilt: an anchor, consumed when it fires), up a press of jump,
	 * down a press of crouch (one above, as the F12 down throw). StickPlusSpecial: special + tilt, special + neutral stick, special + crouch
	 * held. The buttons are the combat's logical buttons (Combat.Button.*), which the input config maps to physical keys and pad buttons
	 */
	static FGSTN_InputSlotSpec BuildThrowSlot(const FGameplayTag& ThrowCommand, EGSTN_LinkThrowInputMode Mode);

	/** The second grab slot (owner decision 2): special pressed with crouch held, falling through without a grab target */
	static FGSTN_InputSlotSpec BuildSpecialCrouchGrabSlot();

	/** True for the throws the stick alone triggers in this mode (forward and back in Stick mode): those a stick used for aiming would fire */
	static bool IsStickOnlyThrow(const FGameplayTag& ThrowCommand, EGSTN_LinkThrowInputMode Mode);

	/**
	 * Gives a kit that grabs (it binds Command.Link.Grab to an ability) its four throws (M11 K2). Per throw: the kit set's entry replaces the
	 * ability of the kit's own rows; with no row (a row bound to nothing is the kit hiding the throw) the kit set's entry, else the fallback's,
	 * is added with the grabber's requirement; then every bound row gets the blockers as ignore tags and, outside Buttons mode, the mode's slot
	 * (or the settings' override of that command). With bSpecialCrouchGrab a copy of the grab row with the special grab slot is added (once).
	 * A kit that does not grab is left untouched. Pure and deterministic: every machine compiles the same table from the same kit and settings.
	 * Returns the number of throws bound
	 */
	static int32 ApplyKitThrows(const FGSTN_KitThrowFill& Fill, TArray<FGSTN_BindingInput>& Bindings);

	/**
	 * Throw blockers other plugins register at startup, by source name (GSTNHeld: Combat.Held.CarryingEnemy, so a lifted enemy is thrown
	 * with the held throw). The link plugin never names their tags. Game thread; same modules and settings on every machine of a match
	 */
	static void SetThrowBlockers(FName Source, const FGameplayTagContainer& BlockAll, const FGameplayTagContainer& BlockStickOnly);
	static void ClearThrowBlockers(FName Source);
	static void GetThrowBlockers(FGameplayTagContainer& OutBlockAll, FGameplayTagContainer& OutBlockStickOnly);

#if WITH_EDITOR
	/** Adds the slots of Build to a set asset (replacing those of the same command), for setup scripts. Null is ignored */
	UFUNCTION(BlueprintCallable, Category = "Link|Slots")
	static void AddToSlotSet(UGSTN_InputSlotSet* Set);
#endif
};
