# Nausea in PIE: diagnosis (2026-11-24, cloud, Opus) — nothing changed, live tests half done

Branch `claude/project-thread-0gtkfd`. Owner: PIE makes him sick, "something wrong with the frame rate"; not with other games, not with GASP base.

## Owner facts (live, 21:27-21:47 UTC)
- L_Test_Hub, 1 PIE player, editor viewport, 144 Hz monitor, never streamed.
- Whole VIEWPORT (not editor panels) flashes slightly blue, repeatedly, also while just playing; ~2 s after dragging the window;
  gone when the editor loses focus (= throttled, low steady fps). GASP base never did it.
- Did NOT remove it: `gstn.Render.Profile 1` (exposure frozen: worse), `r.SkyLight.RealTimeReflectionCapture 0`, `r.SSR.Quality 0`,
  `r.VolumetricCloud 0`, `r.VolumetricFog 0`, `r.AntiAliasingMethod 2`.
- Not yet tried: `ShowFlag.PostProcessing 0`, `gstn.Render.Profile 4` (Lumen back = GASP look), PIE in /Game/Levels/DefaultLevel,
  `r.VSyncEditor 1` + `t.MaxFPS 72`, G-Sync off in the NVIDIA panel.

## Diff with GASP stock (first commit 13faa071, DefaultEngine.ini)
Only image-relevant changes: Nanite + VSM off (f203f68e, 10-25), Lumen GI/reflections off -> none/SSR (389f87e3, 11-20), startup map
DefaultLevel -> L_Test_Hub (6826fe42, 11-13). Tilt-shift post process exists but `gstn.Camera.TiltShift` defaults to 0 (off).
The flicker was first reported 11-20, before Lumen went off: Lumen-off alone is not the cause; level and Nanite/VSM remain.

## Hypotheses, in test order
1. Frame-rate dependent: focus = uncapped, swinging fps on a 144 Hz VRR monitor (G-Sync brightness flicker) and/or temporal effects
   (TSR, DFAO, CSM) at uneven dt. Also the likeliest nausea cause (1 player measured 72-75 fps, no vsync: frames held 1 or 2 refreshes).
2. Our level's lighting/post process (L_Test_Hub vs DefaultLevel).
3. Nanite/VSM off (needs restart to test: `r.Shadow.Virtual.Enable 1` at runtime first).
Measuring tool: `Tools/GASNPPTest/nausea_check.py` on `gstn.Camera.Trace` csv (pacing, sim beat in view, judder). Recordings: review
cameras followed the 62.5 Hz root (capture artefact); gb_sprint has one 8-frame freeze at 7 s, not in the other clips.

## Proposal (owner decides after the tests; nothing is changed before)
A (if 1): cap the editor/PIE frame rate (t.MaxFPS 72 or 144 + r.VSyncEditor 1) in a project ini; no game code.
B (if 2): fix the L_Test_Hub post process / sky light (map edit, owner OK).
C (if 3): bring VSM back (perf cost to measure).
ESITO: diagnosi parziale, nessuna correzione; prove dal vivo da completare
SUBAGENTI: 0
COSTO: ~3 $ | 0 minuti desktop | 0 token review
