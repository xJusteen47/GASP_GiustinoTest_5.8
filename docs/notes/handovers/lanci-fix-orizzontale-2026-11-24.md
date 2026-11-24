# Handover: vertical-only launches and wall rebound (thread "Fix vertical-only launches on other players"), 2026-11-24

ESITO: lanci di nuovo orizzontali su un giocatore (40/80/100/120 % = 6 / 15 / 21 / 28 m all'atterraggio, come il dummy) e rimbalzo a muro al 100 % che si stacca dal muro (anche il secondo rimbalzo). Build e test 268/268 sul fisso; freccia vista sul rimbalzo che esce dall'inquadratura.
SUBAGENTI: nessuno.
COSTO: un thread Opus + ~1 h 45 desktop (Remote Control, worktree GASP_Giustino2_lanci, niente _ue59).

Branch `claude/project-thread-kyph4p` (from `claude/project-thread-5yaz54` 16a357f6). Reports: `docs/notes/reviews/lanci-1a2a/pie-fix-orizzontale-2026-11-24.md`, `wall-rebound-diag-2026-11-24.md` (raw logs), videos `lanci_fix_player_p100*.mp4`, `lanci_wall_rebound_p100*.mp4`, `arrow_ui_v64-v84.jpg`.

## Cause 1 and fix (2e93e149)
Mover's stock mixer copies an OverrideVelocity layered move's velocity into the mode's move but keeps the mode's AdditiveVelocity mix mode.
UGSTN_CombatSimComponent's ProcessGeneratedMovement hook (air momentum, air turn) skips layered-move frames by that mix mode, so on a pawn
with a locomotion profile the knockback's (re)launch frame had its horizontal replaced by the frame's start velocity (0 after hitstop).
The dummy has no profile and skips the hook. Fix: `UGSTN_MovementMixer` (GSTNCombatNP/Movement) sets OverrideVelocity on the combined move;
installed in `BindAirTurn` when the Mover has the stock mixer. Side effect: the air momentum now leaves EVERY velocity-override layered move
alone in the air (dodges, aerial lunges), as its comment always intended: check in playtest.

## Cause 2 and fix (e8af926d + f2cad3d9)
At 100 % the launch is still in its balloon warp (override, 66 frames) when the wall rebound starts; both override at priority 0 and Mover's
mixer keeps the OLDER move, so the rebound's only override frame was lost. A knockback now ends (gives nothing) when a newer knockback is
active, or queued in the start state (Mover activates queued moves while generating: on the newer move's first frame it is still queued).
Tests: GSTNCombat.NP.Hits.Launch.MixerKeepsVelocityOverride, .KnockbackYieldsToNewer.

## Diagnosis logs left in
VeryVerbose only: `LogGSTNKnockbackMove` ([KnockbackGen], every generate with the move clock) and `LogGSTNMoveMix` ([MoveMix], combined
move after the air hooks). Off by default.

## Open
- Floor bounces come out with 0 horizontal (seen at 100 %, before and after these fixes): not diagnosed.
- Distances (21 m at 100 %) come from the balloon rules: owner's call.
- No VFX review (gstn-vfx-reviewer) of puff/arrow yet; the test wall is invisible, so the rebound video happens in mid-air.
- Not merged into claude/integ-*/main: PR from this branch.
