# Bug pass 1-4: one desktop checklist (owner "bug fix e test su tutto", 2026-11-18)

One thread with editor on the DESKTOP (rule 7: take _ue59/IN-USO.txt). Nothing below was compiled in the cloud. Reports: docs/notes/bugpass-1..4.md.

## 0. Branches to merge into claude/project-thread-98dml5 (in this order, no file overlaps between them)
| CP | Branch | Head | Code changed |
|---|---|---|---|
| 1 run/skid/air | claude/project-thread-k9sj5a | ab2746ac | B1 lean sub-step, B5, B7, B2 label |
| 2 defence/link | claude/project-thread-016vb8 | 305251a4 | docs only (contained in CP3's branch) |
| 3 combat sim | claude/project-thread-i8mmmj | 2031201b | C3 flag clear |
| 4 input/camera | claude/project-thread-3f1vdx | (this branch) | K1, K2, I4 test; also brings pg3fen (tilt/shift split, telephoto) |
CP4 merge: one docs conflict in docs/notes/camera-tiltshift-1.md (from pg3fen): keep both sides. pg3fen's own pending steps (setup script for
M_GSTN_TiltPlane, A/B captures, Fable video review) are in its handover, not repeated here.

## 1. Build and tests (Run-Timed.ps1, one attempt each, rule 5)
1. Build: `Run-Timed.ps1 -Preset Build`.
2. Targeted, 0 Fail expected: `-Preset Test -Tests GSTNCombat.NP.RunStartSkid` (B1), `GSTNCombat.NP.Hits.Victim` (C3),
   `GSTN.Camera` (K1, K2 + pg3fen tests), `GSTNInput.NP.Simulator` (I4).
3. Suites: `-Tests GSTNCombat`, `-Tests GSTNDefence`, `-Tests GSTNLink`, `-Tests GSTNInput`, then full `-Tests GSTN`
   (CP1 counted 1783 before its new test; report the new total with the log file).

## 2. PIE checks (L_Test_Hub, KIT_Test_Playtest; video where it is about feel)
| # | From | Check | If wrong |
|---|---|---|---|
| C1 | CP3, FIRST | Launch the dummy into a wall slowly enough for a bump; after it lands and gets up, walk it (or the player after a bump): normal speed at once, or slow ~4 s? With tumble air recovery: bump, recover, jump: is the air drift dead? | code thread: bump drag ends at the tumble's end / first floor contact + test |
| C4 | CP3, owner 1A | DECIDED (owner 11-18 17:59 "A"): after a character has recovered from tumble, a wall bump just bumps; it does NOT put it back in tumble or lock it. Current code already does this; check the bump does not cancel actions. Goes with the C1 fix. | code thread with C1 |
| D1 | CP2 | DA_DefenceRules with MaxDownedFrames shorter than the ground bounce clip: does the dummy pop up without a get-up? | code thread: first free frame at/after the end + test |
| B1 | CP1 | Run start from rest at `t.MaxFPS 30` and `20`: the extra lean comes in and settles (before: locked bent). 60 fps: unchanged. | report |
| B2/B6 | CP1 | Skid at base run after the MinStage 0 data step; skid toward a ledge with ExitFrames 0. | report |
| B4 | CP1 | Profile swap (skin/kit) while running: anim class stays right. | report |
| K3 | CP4 | Jump on stairs/slopes and on flat ground: camera after landing equally firm? | data threshold in a code thread |
| K4 | CP4 | `gstn.Camera.Anchor.Debug 1`: the green sphere is where the view follows during a stomp (anchor drives GASP's camera, not the inactive GSTN one). | report |
| I1 | CP4 | Rest a finger lightly on RT and LT (no full pull): no strong attack / crouch. | add a dead zone to the IMC trigger mappings |

## 3. Only when the feature is used (skip otherwise)
- B3 (CP1) and D2 (CP2): PIE net harness, dedicated + 2 clients with packet lag: run start/stop/boost and a downed client, count reconciles.
- C2 (CP3): before turning Balloon on: strong launch next to a wall, does the victim bounce or slide?
