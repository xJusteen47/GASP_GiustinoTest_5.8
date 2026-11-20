# runstart-v5-desktop: run start freeze fix, v4r5 (no planted step), accel ramp (desktop, 2026-11-20)
Branch claude/runstart-v5-desktop (from claude/runstart-accel-1-desktop bdd06da8 + merge claude/runstart-accel-1 03e6eee7). _ue59 lock
taken and freed. Re-check (rule 11): v4r4 capture frames.jsonl read, f109 historyFrame repeated (1135 twice) as stated.

## 1. Frozen frame ("busto che scatta"), code
Cause found (capture chain first, rule 10): engine frame of f109 = `FlushAsyncLoading` + `Compiling System NS_GSTN_RunStartDust` in
Saved/RunStartV4R4/hires/run.log.ue.log (frame [63] = 1063 = OverrideFPS start 954 + 109). The presentation component loaded
ProjectDefaultTable on the first event that fell back on it (the run start dust), and the editor compiled its Niagara systems then.
Fix: UGSTN_PresentationComponent::BeginPlay / SetTableLayer -> WarmTable (load + WaitForCompilationComplete, editor only). Build OK
(Saved/Logs/ci/build_v5.log). Recheck Saved/RunStartV5/hires/run.log.ue.log: compiles at frame 786, before the sequence; no
FlushAsyncLoading between OverrideFPS 60 and 0.
OPEN (gioco, code queue): one zero sim tick remains (3p f100, 1p f99, sinceMove 17; anim time frozen too = world dt ~0 for one
frame), next double tick 1 frame late; no log line; present with 1 player (Saved/RunStartV5/warp1p), so not networking. Count:
historyFrame deltas of frames.jsonl. Next: Unreal Insights trace of that frame. Attempts used: 1 of 2.

## 2. Pose v4r5 (Cascadeur)
RunStart_v4r5.casc = v4r4 with f0-9 cut (runstart_trim_casc_v4r5.py: points; runstart_trim_boxes_casc_v4r5.py: FK/spline boxes; keys
59-68 deleted, size 59). Starts on the loaded crouch after the stomp. FBX export_joints up Z. AS_GSTN_RunStart_v4r5
(runstart_v4_ue.py, RSV3_EXIT_AT 1.0667): contacts exactly -10 f vs v4r4, root 977.9 cm (AS_GSTN_RunStart_v4r5_build.json).
Rows: v4r4 off, v4r5 on (Saved/AnimPipeline/rows/runstart_v4r4off_v5.txt, runstart_v4r5_new.txt).
Curves re-authored as kind `start`: + enable_warping 1 (owner: stride warping should keep feet planted; v4 clips were `pivot` =
warping off). Saved/Routing/curves_author.txt.

## 3. Data: accel ramp
setup_run_start_ramp.py applied (Saved/RunStages/setup_run_start_ramp.txt): stamp 21 -> 0, stage 1 ramp 62 f / 0.15, stage 2 30 f /
0.25. Speed (frames_to_metrics.py + speed_curve.py, REF metrics_accel.json): 10/50/90% at 0.13/0.45/1.32 s (v4r3 0.37/0.52/1.30).
Small dips at ~0.6 s and ~1.4 s (stage ends) worth a hand-tuning look. Planted feet f80-240 (heuristic, metrics_summary.json):
v4r4 L36.6 R73.8, v4r5 L49.5 R43.8 cm.

## Review
Capture 1p Saved/RunStartV5/warp1p: ASSERT: OK f95-170 93% (min 0.56). Evidence docs/notes/reviews/runstart-v4r5/. Fable r6: PASS (3 notes, review_fable_r6.md):
review_fable_r6.md. Owner videos owner/runstart_v4r5.mp4, _025x.mp4.

COSTO: ~14 $ | ~12 min desktop | review ~0.08M token
