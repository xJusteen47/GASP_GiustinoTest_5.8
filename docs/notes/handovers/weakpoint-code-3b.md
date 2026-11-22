# Weak point precision, code slice 3b (laptop, 2026-11-10)

Branch `claude/laptop-weakpoint-3b` from `claude/desktop-weakpoint-3` (7487fa51). Owner decision (2026-11-10 00:56Z): the right stick turns the
character only while the reticle/crosshair is on, and then it has precedence over the left stick; reticle off = left stick wins, as before.
Docs: `Plugins/GSTNCombatNP/README.md` bullet "Steering needs the reticle", `Plugins/GSTNInputNP/README.md` "Aim stick...".

**Done**
- `FGSTN_InputCmd::bPrecisionAim` (1 bit, predicted): the combat component's `ProduceInput` writes the local `UGSTN_PrecisionAimComponent::IsReticleShown()`.
  `ComputeStartupSteer` takes it and refuses without it (`StepStartupSteer` too), so with the reticle off the right stick is the camera's only.
- Precedence: `UGSTN_CombatSimComponent` is now also an `IMoverInputProducerInterface`. Mover asks the pawn's own producer first, then components,
  so during the startup window with the reticle on it replaces the left stick's `OrientationIntent` with `ResolveSteerHoldYaw` (steered yaw, or the
  move origin yaw with the stick centred / a move with `MaxSteerDegrees` 0). Move input is kept. The SetFacing request stays the sim authority.
- Test `GSTNCombat.NP.Weakpoint.ReticleGatesSteer` (off/on, centred hold, window, intent swap keeps move input, bit on the wire); existing steer tests pass `true`.

**Choices (owner can overturn)**
- Reticle on + stick centred during startup = facing held at the move origin (left stick cannot turn the attack). Reticle off: nothing changes.
- The intent swap reads the latest local combat state (frame + 1) and the last produced aim stick: a hint for the movement mode, not authority.
- A base-relative intent (`bUsingMovementBase`, standing on a moving base) is left alone.

**Future (owner intent, not built):** ranged weapons may make some moves (strong attacks, right trigger) aimable while the reticle is on. Such a move
opts in (per move or per hitbox group) and reads `FGSTN_InputCmd::bPrecisionAim` of the frame, never the camera; `IsPrecisionAimInput()` is the local view.

**Not done:** PIE check by eye (I cannot see the screen), harness scenario with the aim stick (none drives it yet).

**Gate:** build `Saved/WP3b/build-b1.out` first try (130 s, reflection change). Headless `Saved/WP3b/headless-b1.log`: 1475 / 0 fail / 0 ensures (+1).

## Later, before PvP (M14)
- Owner ask (2026-11-10): a pad in the level that opens a GASP-style debug menu (gamepad), entries generated automatically: apply any kit (through the
  host), force ricochet, combat debug draw cvars, reticle flag; works in listen + 2 clients; no map save (spawned by the test game mode). Reuse L_Test_Hub and the kit pads.

Cost: ~60k context added, Opus, estimated ~$4-5. Builds 1, headless 1.
