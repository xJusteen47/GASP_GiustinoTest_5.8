# turnarounds-2 + capture speed (desktop, 2026-11-21): captures 4x faster per variant, Cascadeur ~25x, turnarounds v26 ASSERT OK x4
Branch claude/runstart-generic-desktop (continues 8cb52ea7). Lock _ue59 20:43-21:15Z; the owner closed his editor himself ("chiuso").
DA_Locomotion_Default (owner's uncommitted change) untouched. Re-check (rule 11): v25_h1 ASSERT OK (Saved/RunStartV23/v25_h1/assert.txt).
## Capture speed (owner: "velocizza le catture")
- Was already fixed-step (t.OverrideFPS 60). The cost was per variant: ~139 s (Run-Timed durata, Saved/RunStartV23/v25_h1/run.log.esito),
  ~100 s of it editor boot + Play + quit. pie_capture_move.py: PIE_CAPTURE_RUNS=<runs.json> = N variants in ONE Play session (cvars
  per run, camera yaw restored + stick nudge so the pawn faces the start yaw, spawn reset; every run starts at the same spot and yaw,
  log "facing off by"), per-run assert.txt; PIE_CAPTURE_SHOOT='70,210' shoots only the review window (PNG export = 85% of a clip:
  31.3 of 37.5 s, "timing" lines in capture.log); 1-player captures no longer wait 15 s for 3 players.
- Numbers: 1 variant 107 s (Saved/CaptureSpeed/prof1/console.txt); 8 turn variants 267 s = ~33 s each (Saved/Turn2/t2/timing.txt);
  before 8 x 139 = ~1110 s. Wrapper: Tools/AnimPipeline/cards/runstart/runstart_batch_captures.ps1 -Runs 'name|variant|clip|seq|win|cvars'.
- Laptop / other moves: same shared script, pull the branch. Move captures (dropkick, dair...) keep working unchanged (all opt-in env
  vars); they gain from PIE_CAPTURE_SHOOT (frames to keep), PIE_CAPTURE_CLIENTS=0 when one character is enough, and PIE_CAPTURE_RUNS
  for several variants/kits' cvars (the kit itself is still one per session).
- Cascadeur: keep it open, tabs per clip, one batch call (switch, save-as, write, export, save): 4 clips write+export 31 s (was
  ~3-4 min/clip). UE: one Save-Assets run for 4 clips + all rows, 22 s (runstart_turn_ue.py). Build 31 s. A/B videos 4 dirs 75 s.
## Turnarounds (variant 26/36, default 21 unchanged)
- Composer knobs (default off, old w2t* rebuild bit-exact, checked max diff 0.0): ANKLE_MIN=11 (lateral foot gap f0-14, was 0-1 cm at
  f8-13 in 180), ARM_IK=1 + ARM_SOFT=0.85. Arm jitter cause found: the rev scaled arm points to upper arm 26-50 cm (rig 26.65) and
  Cascadeur missed elbow targets by up to 43 cm and zigzagged (180L R elbow alternating f3-6, f11-12). After: solve error 18 cm (pole
  helpers), elbow zigzag only f11-12 (180L) / none (090), hand swing amplitude 96-101 % (rule 10 limit 85 %). Recipe:
  python runstart_turn_compose_all.py f FOOT_RIGID=1 ANKLE_MIN=11 ARM_IK=1 ARM_SOFT=0.85. Divergence row 21f. Debug drop-down + cvar help list 26.
- Re-selection lock (Assistant): NOT applied. Captures show one clip for the whole turn (Walk_Reface for 2 frames at weight 0.00, then ours).
- Captures Saved/Turn2/t2, ASSERT OK 8/8 (21 and 26 for 180a/b, 090a/b). A/B: docs/notes/reviews/turn2-v26/<dir>/ab_21_vs_26_{az180,az90,game}_{real,slow4x}.mp4 + grids.
## Open
- Rule 10 cloud frame review not done; owner verdict on 26 (and 23/24/25) pending. Same arm issue exists in the forward starts
  (w2/w2f: same ARM scaling): ARM_IK could be applied there as a variant if the owner confirms the fix on the turns.
COSTO: ~13 $ | ~25 min desktop | 0 token review
## Owner 21:19Z on v26 poses: "parecchio strane" -> NEW DESIGN for the turnarounds (brief for the next thread)
Owner screenshot from behind: hunched forward, twisted, arms hanging, feet close. His principle (scope: turnarounds 90/180 L/R):
the first step is taken with the foot on the side you turn toward, placed so the body rotates on that step, then straight into
the frantic run start. Cause in the current clips: the composer plays the w2 forward rev from frame 0 in the root frame and only
yaws the root (stock reface yaw 180/90 -> 0 by f24), so legs run forward while the body is still turning.
Brief: new variant 27/37 (keep 21 and 26). Identity traits: 1 pivot step: turn-side foot steps out toward the new heading and
plants turned (~90 deg in the 180, ~45-60 in the 90), hips lead then chest, other foot pushes off; 2 body upright-ish during the
pivot (no hunch), arms counter-swing, not hanging; 3 from the second contact the w2 frantic rev (same rev as 24, ends on the
stock loop). Pipeline: gstn-move-animation skill + Cascadeur keyed poses (owner rule, clips via Cascadeur), w2 rev appended from
the plant (composer can supply f>=plant; ARM_IK=1, ANKLE_MIN=11 kept). Root: stock reface root yaw/path or re-timed to the pivot
(rule 14: ask the Unreal Assistant how stock reface/pivot clips place the first plant). Captures: runstart_batch_captures.ps1, 21/26/27 x 4.
Suggested: Opus high, desktop, ~12 $, owner verdict on the first 180 L before doing the other three.
