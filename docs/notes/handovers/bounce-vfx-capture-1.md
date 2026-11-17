# Bounces missing in the M2 VFX capture (cloud, Opus medium, 2026-11-17). Branch `claude/project-thread-cocsel` from `claude/project-thread-lrogpg` 69ddff1e
## Cause (read from code + r2 events/grids, no PIE here)
1. **Wall / ceiling: branch missing.** lrogpg (laptop M2) never got PR #35 (`claude/project-thread-h2hlp5` ee7fd35b): `git merge-base --is-ancestor` = no.
   Without it a launched victim just stops on a wall/ceiling box (no surface row) and falls: `events_on.txt` wall Tumble 3532->3705 (= hit_large 2918->3093, the box changed nothing), ceiling 4022->4046, no bounce.
2. **Spike (ground_bounce): the game DID bounce, the VFX missed it.** `ground_bounce_side_24-71.png` v45-47: the victim, hit on the ground by the
   -45 spike (Force Bounce), rises at once = forced floor bounce in StepImpact. The presentation read bounces only from velocity reflection
   (down <= -200 then up); the down speed lives one sim frame and the landing's floor projection clears it, so no snapshot sees it.
   Knockdown at the end is correct: it lands after its 177 hitstun frames (5254+177), no second hard bounce outside hitstun.
## Fix (this branch)
- Merged h2hlp5 (clean). Default wall/ceiling bounce + bump now in the M2 line.
- `FGSTN_CombatStateView` + presentation snapshot carry the sim's `GroundBounceCount` / `WallBounceCount` (sync state, rollback safe, presented state on
  every machine). The deriver emits GroundBounce / WallBounce when a count rises (ceiling = rise turned to fall, normal -Up); the velocity reading stays as
  fallback for bounces with no count (surface row / hit-forced wall bounce) and is silent on a counted step (no double event).
- New events `Presentation.Event.Bump.Wall` / `.Ceiling` from the projected `Combat.State.Bump.*` tags (default tag rules); table rows in
  `Tools/GAS/setup_presentation_table.py` (NS_GSTN_Impact at 0.4, size for Fable r3 to judge). Applied choice, da confermare.
- Capture: new sequence `wall_bump` (medium 90 % jab into a wall at 260 cm) for the small bump; wall/ceiling/ground_bounce unchanged.
- Tests: `GSTNPresentation.Core.Derive.CountedBounce` (spike with no down speed, both readings = 1 event, wall normal, ceiling, chain + re-hit, splat, bump tag),
  `GSTNCombat.NP.StateEvents.ViewFromCore` (+counts). NOT COMPILED, NOT RUN (cloud has no UE).
## Exact setup for the capture thread (M2 r3, laptop)
1. Worktree on this branch; build Editor (C++ changed in GSTNPresentationCore + GSTNCombatNP). 2. Tests `GSTNPresentation` + `GSTNCombat.NP.StateEvents`
   + `GSTNCombat.NP.Hits` (wall bounce tests from h2hlp5). 3. Run `Tools/GAS/setup_presentation_table.py` (idempotent) for the Bump rows.
4. Capture `PIE_CAPTURE_SEQUENCES=hit_large,wall,wall_bump,ceiling,ground_bounce`. Expect in the PRES log: GroundBounce at the spike's launch
   (~v45), WallBounce (hard, if the jab at 170 % meets the wall >1200 cm/s inside its 67 f hitstun) or Bump.Wall; Bump.Wall in wall_bump.
   hit_large may now also hard-bounce off the floor (h2hlp5: impact step runs for every entity with CombatRules).
   If wall shows Bump instead of a hard bounce: move the wall nearer (block 420 -> 300) or raise the damage % (170 -> 220); no code change.
## Not done / risks
- No PIE: speeds at wall/ceiling contact are not measured. Known h2hlp5 queue: real bump clip, tumble mesh clips into the wall.
Cost estimate ~6 $ (Opus, ~110k context, no builds).
