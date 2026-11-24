# Handover: launches (thread "Find gameplay gaps and fix launch feel"), 2026-11-24 17:05 Rome

ESITO: 1A/2A/3A + launch threshold 750 + balloon ON are on the branch, built and unit-tested on the desktop. In-game check found a BLOCKING
pre-existing bug: on a client-controlled victim every launch flies straight up (0 horizontal). Owner decision A: fix it in the follow-up, then
re-measure walls/arrow/zoom and make the final video.

## Branch and commits
`claude/project-thread-5yaz54` (worktree on the desktop: `C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_lanci`; never _ue59 without
the IN-USO.txt lock). Key commits: 1A flight (knocked gravity x4, launch fade 2300), 2A setup_overlord_hop0.py, be672ecc 3A readability
(zoom/puff/arrow, cvars gstn.Launch.Debug.Zoom/.Effects/.Arrow), 1132492b test compile fixes, 0a21146c + f9eb2c0d launch threshold 1080 -> 750
(the Smash data and the owner's 11-16 decision assume 750), a64bab1c + af47b80f balloon ON (`setup_knockback_smash.py --rules-only --balloon on`),
8174baf8 desktop report + video. Docs: docs/notes/handovers/lanci-diagnosi-2026-11-24.md (causes 1-7), docs/notes/reviews/lanci-1a2a/
(pie-desktop-2026-11-24.md, knockback_move_verbose.log, lanci_1a2a_desktop.mp4; copies in project files reviews/lanci-1a2a/).

## Tests
Targeted 283/283; Reaction + NP.Hits 118/118 after 750 and balloon. Full GSTN suite 1880/1882: GSTNCombat.GAS.Loadout.Screen and
.ScreenSupportAndSuper fail (GSTNCombatGAS, untouched here, likely already on main).

## Measured (desktop PIE, strong hit GA_Playtest_Strong, all vertical because of the bug)
Rise 0.87 m @40 %, 2.01 @80, 2.74 @100 (2 floor bounces), 3.59 @120. Hitstun with balloon 85/107/112 frames @40/80/120 (111 vs 126 @100).
Air hits at 0 %: dropkick +645 cm/s up (0.43 m), dair +916 (0.75 m). Zoom works (678 -> 1172 cm), Effects works, Arrow not verifiable,
wall: no contact. Hub pawn lacks jump gravity attributes (jumps ~9.3 m).

## THE BUG (next step)
Verbose log: knockback move wants (2868,0,1656); next frame Falling has (0,0,1593) (vertical minus one x4 gravity step, horizontal zeroed).
Same with old flight and balloon off: pre-existing. Hypotheses (unproven):
1. Under hitstun FGSTN_StatsMovementModifier sets MaxSpeed = 0; Mover UMovementUtils::ComputeVelocity with MoveInputType == Velocity does
   Velocity = dir * min(MaxSpeed, |input|) -> horizontal 0 in one frame (DirectionalIntent would only brake). GASP's BP_MovementMode_Falling
   is the falling mode: check what it computes. Log the victim's input type, MaxSpeed, Factor.
2. Air momentum (GSTN_RunStageSyncState.cpp ApplyAirMomentum/PostMovement): on a ground launch frame bGrounded may still be true, so
   ExternalVelocity stays 0 and the push becomes "player channel" braked at 4000 cm/s2 (not 0 in one frame alone, but check).
3. Dummy without locomotion profile vs a real player victim (client 2 hit by client 1): compare.
Fix rule: deterministic, no IsServer branches; a victim in hitstun must not steer but must keep the launch velocity.
Then: build, tests (Core.Reaction, NP.Hits, Core.AirMomentum, GSTNInput), PIE table 0/40/80/100/120 % WITH horizontal distance,
pie_wall_bounce, the arrow (a player leaving the frame), new mp4, push, report to owner, VFX review (gstn-vfx-reviewer) before showing.

## Owner side questions answered in thread
Wall/ceiling/floor bounce exist (11-17..21) with 3 open issues (bump clip gone after 1 frame, tumble pose into wall, placeholder clip); tech is in
the Defence plugin (off). Smash scale: 1 unit = 12.5 cm makes speed, gravity x4, decay 2300 Ultimate-consistent. Get-up momentum 2 is on
claude/project-thread-qjfnc0 / claude/integ-2026-11-24, not on main. Content subfolders question not handled here (coordinator).

SUBAGENTI: none. COSTO: one long Opus thread (context summarized once) + ~3 h desktop Remote Control.
