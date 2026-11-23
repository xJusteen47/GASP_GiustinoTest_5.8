# runstart turnaround (desktop, 2026-11-21): 180/90 L/R turnaround starts of the w2 family, ASSERT OK x4, Fable PASS (round 2)
Branch claude/runstart-generic-desktop (continues 5e585aa8). Lock _ue59 15:00-16:10Z; the owner had closed his editor, I reopened it.
DA_Locomotion_Default (owner's uncommitted change) untouched. Re-check (rule 11): w2 "ASSERT OK" confirmed (w2_h1/assert.txt).
## Done
- Owner 14:57Z "La w2 va bene, nuovo default": tag approved/runstart-w2 (5e585aa8, pushed), anim-approvate.md line, default
  gstn.RunStart.Variant 21 + debug menu label/first entry (878f6233, build OK Saved/Logs/ci/build_turn.log).
- Brief correction (applied, rule 6): stock B/LL/RL starts are STRAFE starts (body keeps facing, root yaw 0). The orient-to-movement
  turnarounds are M_Relaxed_Run_Reface_Start_{090,180}_{L_Lfoot,R_Rfoot} rows in "Run F Loco Transitions": root yaw 180/90 -> 0 by
  clip f24 on the root BONE track, root path straight ahead (dumps Saved/AnimPipeline/runstart/turn/*.meta.json, *.bones.json).
  The Unreal Assistant confirms (docs/notes/research/unreal-assistant-reface-starts.md, rule 14).
- Clips AS_GSTN_RunStart_Generic_w2t{180,090}{L,R} (63 f): composer SRC=<reface meta> (root frame, stock root yaw), TMAP = game timing
  of the first 180 capture (docs/notes/reviews/runstart-t180a_h1/t180_game_tmap.json), TURN_LEAN=chest (chest leads 45 deg, head +20,
  lean along the chest once < 120 deg is left), w2 recipe otherwise; UE import writes the stock yaw on the root bone (RSGG_SRC).
  Recipe line: w2 env + PIN_U=19 TMAP=<t180 map> TURN_LEAN=chest SRC=<reface meta>. 0 backward knees (runstart_turn_check.py).
- Rows (Variant 21 and 31): sibling of each stock reface row; stock reface row variant <= 20.5 + stock copies 21.5-30.5 and >= 31.5
  (runstart_generic_rows_ue.py RSG_STOCK/RSG_SPLIT). Divergence row 21c. Other variants still play stock refaces.
- Captures (Saved/RunStartTurn, sequences mv_turn_{180,090}{a,b}): r2t180a/b, r2t090a/b ASSERT OK min 1.00 f97-140; a = L, b = R.
- Review r1 FAIL trait 1 (backward tip v88-96, lean on the final heading while the chest faced back); r2 PASS 180 and 90
  (docs/notes/reviews/runstart-r2t180a_h1/review_fable.md). Watch: posa 180 v96-104 narrow base (plank at v100), bank sideways ~4 f;
  gioco 90 idle -> full sink in 4 f (check the blend at speed); dati long-stride landing L 38.2 / R 30.4 cm (stock 29.6-35.5, w2 20).
- PIE 3 players (owner): cause = agent captures on this checkout (pie_capture_move setup_editor listen + 2 clients) and the editor
  saves PIE settings at end of Play into Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini. Fix: Run-Timed puts those keys
  back after every editor run (ba09e939); owner ini set to 1 and checked: PIE in his editor = 1 PlayerController.
- Side task: docs/notes/research/unreal-assistant-haptics-audio.md (raw answers). ask_unreal_assistant.py fixed (json result).
## Open / to the owner
- Turnarounds on Variant 21: look in game (stick back / sideways from idle). Next polish if wanted: wider catch step at the 180 fall,
  L long-stride landing slide (TMAP_CUT), 90 entry blend.
COSTO: ~14 $ | ~30 min desktop | ~217k token review (2 reviews, over the 80k target each)
