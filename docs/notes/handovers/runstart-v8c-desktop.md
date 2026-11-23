# runstart-v8c-desktop: run start variants c, a, b built, switchable, captured (ASSERT OK); reviews pending (desktop, 2026-11-21)
Branch claude/runstart-v8c-desktop (from claude/runstart-v8-desktop d7776cb5). Lock _ue59 taken 23:40Z, freed at the end; owner's
editor reopened. Re-check (rule 11): handover v8 "probe A N_NEW 39" = 39 frames exported here (AS_GSTN_RunStart_v4r8a_build.json).
## Done
- Unreal Assistant (Saved/UE59/assistant_runstart_v8c.json): stock switch = chooser filter column on an ABP variable filled each
  frame from a cvar; for a sped-up loop start it recommends a BAKED clip with root on the capsule ramp and feet locked (runtime
  play rate 3-4x breaks pose history and the stride-warping clamp). Applied (rule 6).
- v4r8c (owner's Mario observation): retime script with new TAIL_START: v4r7 loop cycle f28-46 warped into 8 f (rate 3.5 -> 1.0,
  0.27 s), tail f28-58, root on the DA ramp, strides scaled offline to 0.04. Phase f46 vs f28: 2.7 cm mean point error.
- v4r8a: probe A trimmed/exported. v4r8b: attempt-2 scene, rev-up feet lifted to ball >= 12 cm on f1-f7 (scripted, round 1).
- Switch: cvar gstn.RunStart.Variant (0 v4r7 default, 1 c, 2 a, 3 b; base 1 only) -> UGSTN_LocoBaseSubsystem -> ABP_GSTN_StockMover
  float GSTN_RunStartVariant -> FloatRange column 0 of Run F Loco Transitions (runstart_variant_rows_ue.py, rows report
  Saved/AnimPipeline/rows/runstart_variants.txt). v4r7 rows untouched (enabled). Divergence row 18.
- Captures (runstart_v4r8x_captures.ps1, same run, base v4r7 Saved/RunStartV8c/v4r7): ASSERT OK c/a/b f97-115 100 % min 0.91
  (Saved/RunStartV8c/<v>/assert.txt). Planted-feet travel f80-240 L/R (evidence metrics_summary.json): v4r7 36.6/33.0,
  c 48.7/48.7, a 42.6/45.2, b 44.1/47.0 (b includes the intended airborne frames).
- Attempt 1 of captures FAILED: Live Coding patch is not in Binaries, the capture editor ran without the cvar (v4r7 played).
  Fixed with Run-Timed -Preset Build (35 s). Memory updated.
- Owner permission 2026-11-21: Tools/UE59/Close-OwnerEditor.ps1 (refuses on agent editor running, PIE, dirty packages left;
  graceful quit_editor). Not run (classifier denied the dry test while a capture editor was up).
## Open
- Reviews (rule 10, one per move): docs/notes/reviews/runstart-v4r8{c,a,b}/brief_review.md + grids + mp4. Only PASS to owner.
- c skate risk: feet travel +12-15 cm vs v4r7; if FAIL on feet: S_MIN already 0.02, next knob = TARGET_S 0.3 / root ramp.
- Owner: allow line for the close script (steps in the thread message). DA ramp unchanged (30 f / 0.15).
COSTO: ~16 $ | ~45 min desktop | review 0 token
