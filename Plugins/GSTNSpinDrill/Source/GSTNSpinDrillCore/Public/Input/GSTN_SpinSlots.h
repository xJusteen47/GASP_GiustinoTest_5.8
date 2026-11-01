// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Input/GSTN_InputSlot.h"

/**
 * The spin commands of the default kit as slot specs (spin-drill.md 5, owner answers of 2026-11-01):
 *
 *   Command.Movement.Spin.Clockwise                 Spin 360 degrees clockwise, Stick space (the physical stick: facing does not matter)
 *   Command.Movement.Spin.CounterClockwise          Spin 360 degrees counter-clockwise, Stick space
 *   Command.Movement.Spin.Clockwise.Keys            Motion right, down, left in Stick space (D-S-A on a keyboard)
 *   Command.Movement.Spin.CounterClockwise.Keys     Motion left, down, right in Stick space (A-S-D)
 *
 * The keyboard shortcut is its own command (a child leaf) because a command is unique within a slot set and a step has no "any of";
 * the kit binds it to the same abilities, and a cancel window that names the parent takes the child too. Known risk until the slot can be
 * gated to digital input (input.md 9, bDigitalDirection, not built): a pad half circle from left through down to right also matches the
 * Motion (spike report D0 (c)); a kit that does not want it builds without the shortcut.
 *
 * Kit content, not framework: a kit puts these in its bindings' slot overrides or its command set, and changing how a spin is entered is
 * a data edit.
 */
struct GSTNSPINDRILLCORE_API FGSTN_SpinSlots
{
	/** The slots listed above; without bKeyboardShortcut only the two stick rotations */
	static TArray<FGSTN_InputSlotSpec> Build(bool bKeyboardShortcut = true);

	/** One slot of Build by its command, false when Build has none for it */
	static bool Find(const FGameplayTag& Command, FGSTN_InputSlotSpec& OutSpec, bool bKeyboardShortcut = true);
};
