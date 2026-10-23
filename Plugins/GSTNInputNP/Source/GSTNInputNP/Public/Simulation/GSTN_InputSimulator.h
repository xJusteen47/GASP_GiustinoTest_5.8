// The per-frame input simulation a host model embeds (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Bindings/GSTN_CommandBindingSet.h"
#include "Events/GSTN_SimEvent.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_BufferResetRules.h"
#include "Simulation/GSTN_InputSimTypes.h"
#include "StructUtils/InstancedStruct.h"

class UGSTN_InputSettings;
struct FGSTN_ButtonMap;

/**
 * What the host knows and decides during the input simulation of a frame. Plain C++ (not a UInterface): the host model
 * (the combat component, the standalone input component) implements it and passes itself to FGSTN_InputSimulator::Tick.
 *
 * Every query describes the end of the previous frame and is read inside the simulation, so implementations must be
 * deterministic and read only rolled back state: no Mover component queries, no role checks (gas-npp.md rule 1).
 */
class GSTNINPUTNP_API IGSTN_InputSimHost
{
public:
	virtual ~IGSTN_InputSimHost() = default;

	/** Yaw in degrees the owner faces at the end of the previous frame */
	virtual float GetFacingYaw() const = 0;

	/** The owner's gameplay tags at the end of the previous frame (the ability system's, which the ASC has already rolled back) */
	virtual void GetOwnerTags(FGameplayTagContainer& OutTags) const = 0;

	/** Context tags on top of the owner's, for the bindings' requirements (Input.Context.*, Mover tags...) */
	virtual void AppendContextTags(FGameplayTagContainer& OutTags) const {}

	/** Summary of the movement context stored in the sync state, see UGSTN_InputContextProvider::GetContextFingerprint */
	virtual uint8 GetContextFingerprint() const { return 0; }

	/** Frame events beyond the simulator's own (landing, hits received...) */
	virtual void AppendFrameEvents(uint8 PreviousFingerprint, uint8 CurrentFingerprint, FGSTN_BufferFrameEvents& Events) const {}

	/** The host's own gating of a command (a cancel window, hitstun...). Command locks (Input.Lock.Command*) are the simulator's, not the host's. Default: allowed. A command refused here reserves its press like an unavailable one */
	virtual bool IsCommandAllowed(int32 CommandIndex) const { return true; }

	/**
	 * Whether the binding's ability could start now (the ability system's CanActivateAbility, a const query on the state of
	 * the end of the previous frame). The ability system checks again when it executes the request, so a disagreement between
	 * the two checks costs a failed activation. But the answer decides whether the press is consumed, and that is part of the
	 * rolled back input state: if it differs between the client and the server the states diverge and a reconcile follows.
	 * So the answer must be a function of rolled back state only: CanActivateAbility overrides used by bindings must not
	 * read the role, local control or anything else that differs per machine, and abilities that are ServerOnly or
	 * ServerOnlyExecution (their CanActivateAbility is role dependent) cannot be bound: the binding set rejects them.
	 */
	virtual bool CanExecuteBinding(int32 BindingIndex) const = 0;
};

/** Everything one frame of the input simulation reads and writes. The pointers must outlive the call. */
struct FGSTN_InputTickArgs
{
	/** The Network Prediction frame being simulated (the same number on every machine) */
	int32 Frame = INDEX_NONE;

	const FGSTN_InputCmd* Cmd = nullptr;

	/** The input state of the previous frame, and the one this frame writes (the caller starts it as a copy of In) */
	const FGSTN_InputSimState* In = nullptr;
	FGSTN_InputSimState* Out = nullptr;

	const FGSTN_CommandTable* Table = nullptr;
	const FGSTN_ButtonMap* Buttons = nullptr;
	const UGSTN_InputSettings* Settings = nullptr;

	/** The host's reset rules, applied before the table's own */
	TConstArrayView<TInstancedStruct<FGSTN_BufferResetRule>> GlobalResetRules;

	/** Frames in [PausedStart, PausedEnd) do not age presses (hitstop). INDEX_NONE = no such window */
	int32 PausedStart = INDEX_NONE;
	int32 PausedEnd = INDEX_NONE;

	/**
	 * The sources whose bindings apply this frame (a bit mask the host and the table agree on; this module never reads a bit's
	 * meaning). A binding applies when its SourceMask is 0 or intersects it. All bits = every binding, the default.
	 */
	uint32 ActiveSourceMask = ~0u;

	/**
	 * Where the simulation appends Input.Event.CommandExecuted for each command it requests, null for none. The simulation
	 * writes it unconditionally; the host publishes it after the Network Prediction tick, on the authority only
	 * (FGSTN_SimEventOutbox::Flush)
	 */
	FGSTN_SimEventOutbox* Outbox = nullptr;
};

/** What the frame did, for the host's own logic after the tick. */
struct FGSTN_InputTickResult
{
	/** Buttons pressed this frame (taps included), as sampled */
	uint32 Pressed = 0;

	/** Command and binding requested this frame, INDEX_NONE if none */
	int32 CommandIndex = INDEX_NONE;
	int32 BindingIndex = INDEX_NONE;

	bool HasRequest() const { return BindingIndex != INDEX_NONE; }
};

/**
 * The input part of a host model's fixed tick (M2 section 5), stateless: all state is the embedded FGSTN_InputSimState.
 *
 * Steps, in order: sample the command (held, pressed, released, the stick classified against the facing), append the
 * sample to the history, detect watched tags added since the previous frame, apply the reset rules (the simulator's
 * frame events, the host's, the tags added), resolve which command comes out (a forced one, or the slot resolver over the
 * commands that have a binding), and request the winning binding. The request is only written into the state, stamped with
 * the frame; the host executes it from the ability system's pre-tick, so a re-simulation executes it exactly once.
 *
 * Locks: if the owner's end-of-previous-frame tags hold `Input.Lock.Command`, no command starts this frame (presses stay
 * buffered, and a forced command is refused); `Input.Lock.Command.<root>` locks the commands under that root, which then
 * reserve their press like an unavailable command (UGSTN_CommandLocks). Commands without a binding are not candidates for the
 * resolver, so they never reserve a press either. Each request appends a CommandExecuted event to Args.Outbox.
 *
 * Everything it reads is the command, the previous state, the settings, the table and the host's end-of-previous-frame
 * view; it has no role or re-simulation branches, so the client prediction, the server and every re-simulation compute the
 * same state from the same inputs.
 *
 * Consumption happens on request, not on confirmed activation: the confirmation arrives a frame later and the press must
 * not fire twice. The ability system re-checks CanActivateAbility at execution, and a failed activation is not retried.
 * A command that ends in a stick pattern (a motion, a spin) consumes its terminal stick anchor in the history, which is
 * what keeps it from firing on every frame of its time buffer.
 *
 * TODO: the CanActivateAbility query still goes through the engine ability, which checks the role (ShouldActivateAbility). A
 * role free check in the fork would lift the restriction on bindings to server only abilities.
 */
struct GSTNINPUTNP_API FGSTN_InputSimulator
{
	static FGSTN_InputTickResult Tick(const FGSTN_InputTickArgs& Args, const IGSTN_InputSimHost& Host);
};
