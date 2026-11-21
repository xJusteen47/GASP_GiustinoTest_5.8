# runstart v23 (desktop, 2026-11-21): Variant 23 = 22 + agile "vita", ASSERT OK, A/B 22 vs 23 to the owner
Branch claude/runstart-generic-desktop (continues e44b6692). Lock _ue59 19:20-19:35Z, owner's OK "ok chiudi" in-thread 19:20Z; editor reopened on monitor 2.
Order change by the owner 19:11Z: Variant 23 BEFORE the turnarounds. Numbers = Gemini recalibrated for the agile base character
(owner paste 19:14Z, docs/notes/reviews/runstart-w2s/gemini-agile.md); the heavy set (gemini.md, 5-7 cm) is kept for the big character.
## Done
- Cascadeur: RunStart_Generic_w2v.casc = copy of w2 (w2 untouched, locked) + runstart_vita_casc_w2v.py (points only, rig solves):
  hip sway 2 cm to the stance foot, shoulder twist +8 deg (amplifies the existing counter-rotation) with roll 2.5, rear elbow +12 deg
  back in the sagittal plane (elbows tight, no abduction), upper body +5 deg forward peak f12 (= v92) with head drag 1.5 f; all 0 at f0
  and from f29 (loop hand-off). Feet moved 0.001 cm, rig solve error max 1.26 cm. Tunables: SWAY PITCH YAW ROLL ELBOW DRAG PEAK OUT.
- UE (Save-Assets): AS_GSTN_RunStart_Generic_w2v (62 f, travel 728 cm like w2), copy w2vs with GSTN_StanceWidth curves by contact
  (new SW_SCHEDULE in runstart_stancewidth_ue.py: 0.8,0.8,0.8,0.4,0 x 5 cm = 4/4/4/2/0 cm, changes only in swings), rows 23/33,
  ESITO OK 4 rows (Saved/AnimPipeline/stancewidth/setup.txt). Drop-down + cvar help list 23. Build Run-Timed OK 52 s (Saved/Logs/ci/build_v23.log).
- Owner request: editor/capture windows on his SECOND monitor: vd2_mover.py WINDOW_PLACE=monitor2 (new default; vd2 = old rule).
  Verified: Saved/RunStartV23/window.log "place=monitor2".
- Captures mv_run_start_v5, views az180/az90/az0/game, 1 player: v22 and v23 ASSERT OK (Saved/RunStartV23/assert_*.json, copies in
  docs/notes/reviews/runstart-v23_h1). x200 1017.0 both; foot metric L/R 84.8/65.6 (22) vs 84.1/70.3 (23) (Saved/RunStartV23/*.evidence.txt).
  A/B: docs/notes/reviews/runstart-v23_h1/ab_22_vs_23_{az180,az90,game}_{real,slow4x}.mp4 + grid_ab_*.jpg (runstart_ab_video.py).
  Note: the two runs spawn at different hub spots, so backgrounds/camera distance differ; frames are aligned (v77-v197).
## Open
- Rule 10: no cloud frame-by-frame PASS/FAIL yet; owner asked to see it directly. My look at the grids: 23 leans more and pumps the
  rear arm more at v92-v122, no pop at the hand-off, no foot slide; the change is subtle at game distance (may want PITCH 6 / YAW 10).
- After the owner's verdict: turnarounds (ankle clamp 10-12 cm agile, pivot blend 0.14-0.18 s + lock 5-8 f = rule 14 first).
## Round 2 (19:50-20:20Z): toes + torso additive -> variants 24/25
- Owner: "i piedi hanno la punta che si piega". Cause (clip, not game): the composer's stride scaling scaled every foot point's offset
  from the hip, so foot->ball shrank 15 -> 2.6 cm and the toe angle went 5-119 deg in f0-12 (stock loop 25-44); numbers: points of
  Saved/AnimPipeline/runstart/generic_w2_pose.npz vs runloop/loop_relaxed_dump.npz. Fix in runstart_generic_compose.py FOOT_RIGID=1
  (default; 0 rebuilds w2 bit-exact, max diff 0.0): toe 23-41 deg, foot points unchanged. Turnaround clips w2t* still have the old toe.
- w2f (rebuild) / w2vf (w2f + vita) in Cascadeur -> variants 24 (= 22 fixed, w2fs) and 25 (= 23 fixed, w2vfs). Note: the on-disk
  RunStart_Generic_w2.casc is stale (frames 55-60 held); the exported w2 FBX and the w2v tab were fine.
- Torso additive (owner): gstn.Loco.TorsoLean deg (+ forward/lower, - upright) x curve GSTN_TorsoLean (25 only, f1-6 in, f20-29 out),
  in the GSTN Stance Width node; Assistant asked (docs/notes/research/unreal-assistant-torso-lean-pivot.md, also the pivot answer for
  the turnarounds); divergence 21e. Build OK (Saved/Logs/ci/build_v25.log).
- Captures v24, v25, v25 lean -6, v25 lean +6: ASSERT OK all (docs/notes/reviews/runstart-v25_h1/assert_*.txt), x200 1017.0.
  A/B in docs/notes/reviews/runstart-v25_h1/{toe22,toe23,torso}. Captures are 480 px: the toe is hard to see there, the fix is
  verified on the clip points; owner eye needed.
- Next (owner "poi passa a quelle di lato"): turnarounds; rebuild w2t* with FOOT_RIGID, ankle clamp 10-12 cm, pivot: the Assistant
  says a shorter blend makes jitter MORE visible, lock re-selection instead (Pose Reselect History / Search Throttle Time).
COSTO: ~16 $ | ~45 min desktop | 0 token review
