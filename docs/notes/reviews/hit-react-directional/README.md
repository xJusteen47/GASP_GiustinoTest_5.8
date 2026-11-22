# Directional hit additive: system check (rule 10, PLACEHOLDER poses, poses NOT judged)
Captures: Tools/CI/Run-Timed.ps1 -Preset Capture with Tools/GASNPPTest/pie_capture_react_ingame.py (new sequences side_hitstand = victim turned 90 deg
to the attacker's right, side_hitstand_l = left; env PIE_CAPTURE_PLACE_SIDE / PIE_CAPTURE_PLACE_BEHIND). Additive ON/OFF = cvar a.GSTN.HitAdditive 1/0
(PIE_CAPTURE_CVARS). Logs Saved/Logs/ci/hitint_cap_*.log (ESITO: OK, 168-230 s). Files: <seq>_<on|off>.mp4 (hit-2 .. hit+60), <seq>_<on|off>_az45/az90.png
(6 frames... every 3rd frame from hit-2 to hit+31), cmp_<seq>_az90_ON-top_OFF-bottom.png.
Result: all four hits land (Hitstun segment in frames.jsonl), no errors in capture.log, ON adds a torso bend on top of the same HitStand base, OFF = base only.
Limits: the selected side is not logged (no readout), only inferred from the geometry; back-hit turn untouched; ON/OFF camera aim differs a little per run.
