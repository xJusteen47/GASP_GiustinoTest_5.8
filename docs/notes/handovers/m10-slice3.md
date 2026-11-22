# M10 slice 3: held buttons in the kits, pickup target and prompt (desktop, 2026-11-10)
Branch `claude/desktop-m10-3` from `claude/desktop-m10-2` (880f8c7b). Brief: `m10-slice2.md`. Doc: `Plugins/GSTNHeld/README.md` (steps 8-9).

**Done**
- Buttons in kits by code, no Blueprint: the runtime module registers loadout extension `GSTNHeld`; kits listed in `KitsWithDefaultBindings`
  (`DefaultGSTNHeld.ini`, TEST VALUES `KIT_Test_Hits` = hub default, `KIT_Test_Link`) get pickup/drop/throw with their slots
  (`UGSTN_HeldSlots::AppendKitBindings`); a kit's own binding of a held command wins; `bAddDefaultBindingsToEveryKit` (off) for all kits.
- Bug of slice 2 fixed: `Command.Held` had no tier (strong attacks, 200, took RT from the throw). Now `CommandTiers` Command.Held = 360 (as
  Command.Link) contributed at startup. Offsets: pickup -2 (below grab 0, mount -1), drop/throw 110 (above release 100, dismount 101, carrier drop 102).
- G2 interim (no GSTNLink edit): tick extension projects `Interact.Target.Pickup` (empty hand + object at rest in reach of the snapshot pose,
  `FGSTN_HeldRules::FindPickupTarget` = the host race with one request); pickup binding requires it, so B falls through when nothing is near.
  Reads the host's replicated object state (not stamped records): around a pickup/landing a client may be a round trip early/late.
- Prompt: placeholder text "B" above the local player's target object (`UpdatePrompt`, presentation only). `HeldHandReport` logs `target=`.
- Tests `GSTNHeld.Core.KitBindings`, `PickupTarget`, `SlotPriorities`; DefaultSlots updated. Harness `held_drop_and_break` +2 checks (target free / none while carrying).

**Choices (owner can overturn)**: kit list instead of every kit (every kit broke 4 combat tests that count table rows, and changes kits that
never see objects); interim G2 in GSTNHeld instead of GSTNLink's probe (plan G2 needs a shared meaning table: design, GSTNLink is touched by other threads).

**Gate**: builds b1 15 s, b2 10 s, b3 8 s (all first try). Headless h1 1480/4 (every-kit bindings, fixed), h2 `Saved/M10c/headless-h2.log` 1484/0/0.
Harness r1 FAIL (my extra report pass stalled the game thread and compressed the timeline; one pass per world now), r2 PASS listen+2 2 % and
dedicated+2 0 %, 11/11 each, 0 ensures (`Saved/Logs/m10c/r2-*`). Modules check 21 = before. PIE by eye not done (editor closed).

**M10 is not complete. Left (plan order)**: G2 proper (stamped records in GSTNLink's probe, one `Interact.Target.*`) + G3 detached credit /
`Combat.Source.Thrown`; G6 carry load and size classes, G7/G8 rest (aim policy, Break/Bounce/Stick, object ghost); G9 thrown enemies, G12-G13 debug/editor/sweep.
M11 notes kept (no work): grabbed enemies as held objects, smash-style kit throws, non-liftable enemies.

Cost: ~160k context, Opus, estimated ~$12-14. Builds 3, headless 2, harness 2 (~3.5 min each).
