# runstart generic w2 + w1 locked + debug menu (desktop, 2026-11-21): w2 built, ASSERT OK, Fable PASS -> owner
Branch claude/runstart-generic-desktop (continues 1c1c4ab0). Lock _ue59 14:37-14:55Z; owner closed the editor, I reopened it.
DA_Locomotion_Default (owner's uncommitted change) untouched. Re-check (rule 11): w1 "ASSERT OK" confirmed (w1_h1/assert.txt).
## Done
- Owner 14:35Z "Buona mi piace. Nuovo standard": tag approved/runstart-w1 (1c1c4ab0), line in anim-approvate.md; defaults
  gstn.RunStart.Variant 20 + Handoff 1 (= what the w1 review saw, rows 30) (631a7ed9).
- Owner 14:41Z debug menu: gstn.Loco.Base default 1; one control per cvar (bool / plain int = check box only; int whose help names a
  value >= 2 = number only; RunStart.Variant = drop-down "20 generic w1 ..." with 20, 21, 0-9) (ea4b03b7). Build OK.
- Owner 14:40Z "PIE 3 personaggi": _ue59 already had PlayNumberOfClients=1; the 5.8 copy GASP_Giustino2 had 2 (listen host + 2 =
  3 players) -> set to 1 in its Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini (backup in my scratchpad). Not in git.
- Slide cause (w1 capture metrics_w1_h1.json): capsule 2-3x ahead of the clip root in the drive (13.2 vs 5.3-5.6 cm per clip frame
  at f11), dynamic play rate 1.0 -> 1.25x. Composer knobs (ea4b03b7, w1 reproduced bit-exact without them): TMAP (json uf/dist from a
  capture: resample on the game timing, root on the capsule until TMAP_CUT = N_REV, then loop speed), PIN_U (drive contacts pinned,
  knees by leg_ik). Map: docs/notes/reviews/runstart-w1_h1/w1_game_tmap.json.
- w2 = Variant 21/31: w1 recipe + LEAN_OUT=22 DROP_HOLD=13 DROP_OUT=22 PIN_U=19 TMAP=<map>. 62 f, 0 backward knees (min 6.3 cm).
  First try followed the capsule to the end: hand-off contacts got worse (L 37.3, R 35.0) -> TMAP_CUT. Divergence row 21b updated.
- Capture: ASSERT OK min 1.00 f97-140 (Saved/RunStartGeneric/w2_h1/assert.txt), travel f0-200 1017.0 = w1. Per-contact (runs z<13,
  f80-240, one-off on metrics_*_h1.json): w2 L 14.6/5.0/28.7/28.6/29.3, R 1.1/2.7/20.0/29.5/24.7; w1 L 14.3/20.5/25.6/28.3/29.3,
  R 1.1/11.3/33.9/28.8/24.7.
## Review (Fable, docs/notes/reviews/runstart-w2_h1/review_fable.md): PASS, 3 of 3 traits, w2 better than w1
Watch: A dati v114-120 R long-stride landing 20 cm (was 33.9); B dati v132-138 L 28.7 (+3, loop class); C posa v108-112 swing
catches up a bit faster after the pin (do not pin longer); gioco: first L 14.6 + fold v86-88 under the idle blend (code queue).
## Open / to the owner
- w2 to become the standard (default 21, lock w1 stays)? Next: turnarounds, brief runstart-turnaround-brief.md (new thread).
COSTO: ~7 $ (w1 lock + menu + w2) | ~9 min desktop | ~90k token review
