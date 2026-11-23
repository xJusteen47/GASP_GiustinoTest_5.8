// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Input/GSTN_InputSlot.h"

/**
 * The spin and drill commands of the default kit as slot specs (spin-drill.md 5, owner answers of 2026-11-01):
 *
 *   Command.Movement.Spin.Clockwise          Spin 360 clockwise in Stick space (the physical stick: facing does not matter), or with the
 *                                            keyboard shortcut the Motion right, down, left (D-S-A), in one AnyOf
 *   Command.Movement.Spin.CounterClockwise   the same counter-clockwise; the shortcut is left, down, right (A-S-D)
 *   Command.Movement.Drill                   Press(Crouch); bound only while Combat.State.Spinning (MakeDrillSlot)
 *
 * Known risk until the shortcut can be gated to digital input (input.md 9, bDigitalDirection, not built): a pad half circle from left
 * through down to right also matches the Motion (spike report D0 (c)); a kit that does not want it builds without the shortcut.
 *
 * Kit content, not framework: a kit puts these in its bindings' slot overrides or its command set, and changing how a spin is entered is
 * a data edit.
 */
struct GSTNSPINDRILLCORE_API FGSTN_SpinSlots
{
	/** The two spin slots; without bKeyboardShortcut only the stick rotations */
	static TArray<FGSTN_InputSlotSpec> Build(bool bKeyboardShortcut = true);

	/** One slot of Build by its command, false when Build has none for it */
	static bool Find(const FGameplayTag& Command, FGSTN_InputSlotSpec& OutSpec, bool bKeyboardShortcut = true);

	/** The drill slot: Press(Crouch) on Command.Movement.Drill (D8) */
	static FGSTN_InputSlotSpec MakeDrillSlot();
};
