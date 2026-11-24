# GSTNSpinScrew

A screw on the floor that the spin and the drill dive unscrew (spin-drill.md 13d, owner 2026-11-24). Bridge between
[GSTNSpinDrill](../GSTNSpinDrill/README.md) (the `GSTN Spin Turnable` interface) and [GSTNHeld](../GSTNHeld/README.md) (the unscrewed
screw is a held object): neither of them names the other.

Status: **experimental (0.1)**, `EnabledByDefault: false`, enabled in `GASP_Giustino2.uproject`.

## Module

`GSTNSpinScrew` (Runtime). Public: Core, CoreUObject, Engine, GSTNSpinDrillCore, GSTNHeldRuntime. Private: NetCore. Never the project
module, GSTNCombat* directly or Network Prediction: the screw is ordinary replicated state, never simulated.

## Classes

- `AGSTN_SpinScrew` ("GSTN Spin Screw"): place it on the floor (actor origin = floor level). Greybox: a flat cylinder head 60 cm wide
  that blocks like the floor, a shaft under it, a dark slot on top to read the turn. Implements `IGSTN_SpinTurnable`: every spin within
  reach turns it `SpinSteps`, every drill dive landing on it `DrillSteps`. The `LoosenSense` (-1 = counter-clockwise) unscrews: the head
  turns `DegreesPerStep` and rises `RisePerStepCm` per step; the other sense screws it back (`bOppositeTightens`). At `StepsToRelease`
  (6) it fires `OnReleased` / `On Screw Released` and `ReleaseScrew`:
  - `ToHandOrFloor` (default): spawns `PickupClass` (`AGSTN_SpinScrewPickup`) where the head was; when the turner holds nothing and lifts
    no enemy, the host asks the ordinary pickup for it, so it ends in the hand; otherwise it stays on the floor to pick up later.
  - `PopAndVanish`: it just goes.
  `TurnBy(Turner, Sense, Steps)` turns it from a script. `Progress` and `bReleased` replicate; the look follows smoothly on every machine.
- `AGSTN_SpinScrewPickup`: an `AGSTN_HeldObjectActor` with a cylinder look.

## Extending

A Blueprint child changes the mesh, the numbers and reacts with `On Screw Turned` / `On Screw Released` (doors, platforms). Other spin
reactive objects implement `GSTN Spin Turnable` themselves (Class Settings > Interfaces) and need nothing from this plugin. Future kinds
(rising platform screw that locks at the top, spring-back screw): spin-drill.md 13d.
