# runstart-entry-desktop: run start ENTRY fix (desktop, 2026-11-21) - entry PASS on v4r7h, a/b/c left with clip-data pops
Branch claude/runstart-entry-desktop (from claude/runstart-phase-desktop 7902f7c8). _ue59 lock taken 01:05Z, freed at the end; owner
editor already closed (not touched). Re-check (rule 11): runstart-phase v4r7h ASSERT OK confirmed (Saved/RunStartPhase/v4r7h/assert.txt).
Also committed the previous thread's chooser + 4 `<v>h` clips, saved but never committed (8f266ace).
## Diagnosis (capture with ABP vars: PIE_CAPTURE_ABPVARS regex, new in pie_capture_move.py; Saved/RunStartEntry/diag_v4r7h)
- Gait lags the move by ONE frame: f82 MovementState Moving + Gait Walk -> stock walk start picked (weight <= 0.01), f83 Gait Run ->
  stock Re-Enter (Gait != Gait_LastFrame) -> our start at f84 from frame 0. No later re-pick: same state, Gait, continuous clip time
  (runstart_entry_metrics.py on frames.jsonl; Saved/RunStartEntry/diag_v4r7h/entry_metrics.txt).
- The logged start weight sag 0.82 -> 0.50 (f90) -> 1.0 (f99) is max(w, 1-w) of the stock lean layer ramp (BS_Relaxed_*_Leans weight w
  0 -> 1 over ~0.27 s): most likely one cached Blend Stack pose logged at its larger branch weight. Not verified in the graph.
- Unreal Assistant (Saved/Assistant/replies_runstart_entry.json): stock Gait = stick intent (walk-first should not happen; fix = gait
  from intent + latch, in the character); leans react to LATERAL acceleration only; short blend: keep FastFeet_FastRoot, 0.1 s ok.
## Done
- Entry blend 0.1 s Linear (was 0.05 CircularOut) on the 8 hand-off rows: runstart_handoff_ue.py defaults, Save-Assets
  (Saved/AnimPipeline/rows/runstart_handoff.txt: "ESITO: OK handoff rows 8 mismatch 0"). Tunable: RSH_BLEND / RSH_CURVE + rerun.
  Divergence row 19 updated.
- Captures (runstart_handoff_captures.ps1 -SavedDir RunStartEntry -ReviewPrefix runstart-entry-): 4/4 ASSERT OK. Max one-frame
  stance-foot slide at entry (runstart_entry_metrics.py, prev = Saved/RunStartPhase): v4r7 12.0 -> 5.5 cm, a 12.0 -> 5.7, c 7.3 -> 4.3,
  b 1.6 -> 4.4. Dense entry grids v80-v103 in docs/notes/reviews/runstart-entry-<v>h/entry/.
- Reviews (Fable, brief docs/notes/reviews/runstart-entry-brief.md + 3 grids each, 88-99k tokens each): entry pop FIXED in all 4.
  v4r7h PASS. a FAIL v89 one-frame leg twitch; b FAIL v91 one-frame upright torso; c FAIL v93 (dati montage, in the clip).
  Reviewers labelled a/b "gioco"; I relabel them dati montage: clip time steps evenly (dt 0.016-0.017), the entry blend is over by
  f90, same frames in both rounds with different blends, and v4r7h (not time-warped) has no pop > 5.6 cm (runstart_pop_check.py,
  Saved/RunStartEntry/<v>h/pops.txt: a f89 foot_l 10.9 cm, c f93 foot_r 9.0 cm). b's R landing skim v113-118 [posa] carried over.
## Not done / next
- Gait 1-frame lag: left as is (1 frame, weight <= 0.01; fix lives in the shared character Blueprint -> owner/editor thread).
- a/b/c pops: pose/clip threads on the variants (with the owner's 0.47 s first-block timing target), not code.
- v4r7h hand-off not re-reviewed this round (grids end v144; unchanged since its PASS last round).
COSTO: ~11 $ | ~15 min desktop | ~368k token review (4 x 88-99k)
