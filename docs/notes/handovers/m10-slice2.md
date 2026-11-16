# M10 slice 2: the hold as a link, default buttons (desktop, 2026-11-10)
Branch `claude/desktop-m10-2` from `claude/desktop-m10-1` (1bbfa0c8). Brief: `m10-slice1.md`. Doc: `Plugins/GSTNHeld/README.md` (steps 6-9).

**Done**
- Link: the host posts every hold start/end to the holder's combat inbox (`Held.Command`, kind 48); the holder's `FGSTN_HeldFragment` (feature id 20, added
  to the core id list) is sync state and `Combat.Held.Carrying` is projected from it every frame, so "holding" is predicted and rolled back. Breaks (host,
  after each frame): holder tags `Combat.State.Hitstun/Tumble`, `Combat.Link.Role.Grabbed` (by name, no GSTNLink dependency), event `Combat.Event.KO`, holder
  gone; drop `DropForwardCm` in front. Per object override `bOverrideBreakRules`. Log `HeldBreak object= entity= reason=`.
- Owner's buttons as defaults (`Config/DefaultGSTNHeld.ini`, all TEST VALUES): `UGSTN_HeldSlots` B = `Command.Held.Pickup` (one below the grab) / `Drop`
  (requires Carrying); RT = `Command.Held.Throw` (requires Carrying, above strong attacks). Throw fires on release: tap < `AimHoldFrames` 12 = left stick
  (new `EGSTN_ProjectileAim::Stick`), held = reticle (`SetHoldAimShown`) + camera aim; reticle toggled on = camera. Max 300 frames.
- Neutral hooks: `UGSTN_GA_FireProjectile::DeferFire`, `GetShotAim`, `StickAimDeadzone`; `UGSTN_CombatSimComponent::GetFrameStick`, `IsPrecisionAimFrame`.
- Content hooks, unused: `UGSTN_HeldObjectDefinition::CommandActions` (other attack buttons per object), `RangedAction`. `UGSTN_GA_HeldDrop(+Server)`, `gstn.Held.Drop`.
- Tests `GSTNHeld.Core.BreakRules`, `ThrowAim`, `DefaultSlots`, `GSTNHeld.Runtime.HandFragment`; harness `held_drop_and_break` in `SUITE_HELD`.

**Choices (owner can overturn)**: GSTNLink's Hand channel links only two fighters today (watchdog, commands, tags are Fighter/Body only), so the hand lives in
GSTNHeld's own fragment; folding it into a Hand link needs M6 neutral hooks (object partner kind). Command tags native `Command.Held.*` (like `Command.Link.*`),
not the plan's `Command.Link.Pickup/Drop`. A cancelled aim (hit) throws nothing. The pickup is still host-decided (client sees Carrying one RTT later).

**Not done**: binding in the owner's kits (content: `UGSTN_HeldSlots::AddToSlotSet` on the kit slot set + bind `UGSTN_GA_HeldPickup/Drop/Throw`), PIE by eye.

**Design for later (M11 kits, owner 2026-11-10, no decision taken)**: grabbed enemies that can be lifted behave like held objects (B drops, RT throws);
kit throws smash-style on special + crouch + stick combos; enemies that can be grabbed but not lifted only take the smash-style throws. All subject to playtest.

**Gate**: build b1 52 s first try (b2 up to date). Headless `Saved/M10b/headless-h1.log` 1481/0/0 ensures (+4). Harness `Saved/M10b/r1.log`, reports
`Saved/Logs/m10b/`: listen+2 at 2 % and dedicated+2 at 0 %, `held_pickup_throw_hit` 6/6 and `held_drop_and_break` 3/3 PASS, 0 ensures. Modules check 21 = before.

Cost: ~200k context, Opus, estimated ~$13-15. Builds 2, headless 1, harness 1 (~3.5 min).
