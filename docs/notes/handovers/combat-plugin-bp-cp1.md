# COMBAT-PLUGIN-BP CP1 handover (cloud, 2026-11-14)
Branch `claude/project-thread-7rpp3d` from qfjwsu `d7851d36` (= wkds3r `d27c77f5` + plan). Knockdown/landing code untouched. **NOT BUILT** (no UE in the cloud).
**Done**
- `UGSTN_GA_BaseCombat` BP sim events: On Move Started, On Move Key (Event key crossed, new `KeyTag` on `FGSTN_TimelineTrack_Event`), On Move Hit
  Confirmed (new hit groups; blocked counts, parry/ricochet not), On Move Ended (`EGSTN_MoveEndReason`), On Move Frame only with `bTickMoveInBlueprint`
  (default off). Cursors are FSync vars (`EvaluatedFrame`, `ReportedHitGroups`, `MoveEventsFrame`): once per simulated path, re-raised on resim,
  never while restoring a frame. Rules split into `FGSTN_MoveEventRules` (`Abilities/GSTN_MoveEvents.h`). BP getters `GetMoveTimelineFrame`, `GetMoveHitConfirmGroups`.
- Combat component BP queries by tag (no keys): `WasButtonPressedWithin`, `GetFramesSinceButtonPressed`, `CountButtonPressesWithin`,
  `GetButtonHeldFrames`, `WasCommandEnteredWithin` (whole pattern, unused presses only), `GetMoveStick`, `GetStickDirection`, `GetEvaluatedTimelineFrame`.
  Ages = combat frames (hitstop excluded, same rule as the buffer). Pure math `FGSTN_InputQueries` in GSTNInputCore.
- Editor: Event row label shows "Key X"; validator reports an unregistered KeyTag. README section in `Plugins/GSTNCombatNP/README.md`.
- Tests (9 new): `GSTNInput.Core.Input.Queries.*` (5), `GSTNCombat.NP.MoveEvents.*` (4: keys once through hitstop/jump/rollback and same as the player, hit groups, end reason).
**To do before CP2 (desktop, Haiku low, ~$2)**: build `_ue59` on this branch, run `GSTNInput.Core.Input.Queries` + `GSTNCombat.NP.MoveEvents` + full suite (was 1596/0).
  Max 2 fix attempts on compile errors; likely spots: UPARAM meta on BP pure params, K2 event with const struct ref.
**Findings**
- The cue and gameplay event of an Event key are NOT fired anywhere at runtime today (only the outbox part is). Variant A of the double-press
  special (key → GA_FireProjectile by gameplay event) does not work yet: CP2 must fire them (inside the sim, once per crossing, same cursor).
- Validator for BP sim graphs (non-FSync vars, Delay/timers, Mover reads) from the plan was NOT done: moved to CP2.
**CP2 brief (Opus high, cloud then desktop build, $25-35)**: read the plan + this file only. (1) Fire the Event key's GameplayEventTag (and cue)
  from the ability on the crossing frame using `EvaluatedFrame`; test resim once-only. (2) BP request actions on the ability: Fire Projectile
  from a `ProjectileDefinition` data asset (GSTNProjectile plugin, host spawn + ghost through the existing path), Send Event, Apply Move GE,
  Open Tag Window; never direct state writes. (3) Editor validator for sim graphs. (4) Tests. Stop after CP2; CP3 = double-press A+B examples.
**Cost**: cloud Opus, ~120k context, no subagents, ~$9.
