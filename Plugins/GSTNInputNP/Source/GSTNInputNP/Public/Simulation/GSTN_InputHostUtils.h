// What every host of the input simulation does the same way (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Events/GSTN_SimEvent.h"

class AActor;
class UAbilitySystemComponent;
struct FGSTN_CommandTable;
struct FGSTN_InputSimState;

/**
 * The parts of a host model (the combat component, the standalone input component) that do not depend on which model it is:
 * asking the ability system whether a binding could start, executing the request from the ability system's pre-tick, reading
 * the held buttons of the frame, and publishing the event outbox. Stateless, so the hosts keep their own state and share the
 * rules; the ones that read only the sync state are testable without a world.
 */
struct GSTNINPUTNP_API FGSTN_InputHostUtils
{
	/**
	 * The ability system's CanActivateAbility for the binding's ability class, a const query on the state of the end of the
	 * previous frame (the host's CanExecuteBinding). False if the ability system has no spec of that exact class.
	 */
	static bool CanExecuteBinding(const UAbilitySystemComponent* AbilitySystem, const FGSTN_CommandTable& Table, int32 BindingIndex);

	/**
	 * Runs from the ability system's pre-tick: if State holds a request made on Frame, activates the binding's ability. A
	 * request of an older frame (a restored state, a frame the simulation did not repeat) is ignored, so a re-simulation
	 * executes each request exactly once. The ability is found by its exact class, so bindings must name the granted class.
	 * A failed activation is not retried: the press was consumed with the request.
	 * @return whether a request was made on Frame (it may still have failed to activate)
	 */
	static bool ExecuteRequest(UAbilitySystemComponent* AbilitySystem, const FGSTN_CommandTable& Table, const FGSTN_InputSimState& State, int32 Frame);

	/** Whether the button (index in the input config) is held on the latest frame of State's history; false for an index outside the mask */
	static bool IsButtonHeld(const FGSTN_InputSimState& State, int32 Button);

	/**
	 * Publishes the outbox to the world's event subsystem on the authority, with Owner as the instigator, and empties it
	 * either way. Authority means Owner->HasAuthority() and nothing else: the pawn of a listen-server host is the authority
	 * and is also locally controlled, and its events must be published like any other pawn's. Call it outside the
	 * simulation, after the Network Prediction tick (FinalizeFrame).
	 * @return the number of events published
	 */
	static int32 PublishOutbox(FGSTN_SimEventOutbox& Outbox, AActor* Owner);
};
