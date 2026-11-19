# Handover smoke A2 r1: landing dust sized to the owner's frame (2026-11-19, laptop, Opus high) — Fable FAIL round 1 of 3
Branch `claude/project-thread-ii4go3` from 08840aa8. Brief: coordinator note (owner size target = A1b early frame, `reviews/vfx-v3/a2/owner_size_target.png`).
## Built (scripts only; uassets rebuilt by the capture -Pre)
- `setup_presentation_vfx.py` `land_ring`: Skirt 8 puffs 30-90 cm, ring 26 + Ring Width 0.6, speed 180, drag 4.5, rise 10; Wisps -> 4 haze sprites
  50-80 cm (new MI `SmokeHaze`, OpacityMax 0.3); Clumps 5 x 3-6 cm grit; contact disc 120 cm at MaxOpacity 0.25. `_puffs(ring_width=)`.
- `setup_presentation_vfx_puff.py` `SmokeReal`: StartScale 0.8 (slight growth), FadeStart 0.05, OpacityMax 0.75, DarkSnow 0.65, DarkBrightness 1.3,
  Wrap 0.15, Ramp 0.25-0.6, ShadeColor (0.45,0.47,0.52). Flipbook (`make_vfx_smoke_flipbook.py`): grow 0.9->1.05, noise 0.42, dens 0.55+0.75b,
  blur 2, k 14, occlusion on the underside (not the core).
## Evidence
Captures `Saved/VfxV3/a2/{dark,light}{3,4,6}` (6 = GASP hub light, final; `run.log.esito` per folder; 1 and 5 failed: typo / 5.9 level).
Videos `reviews/vfx-v3/a2/video/A2_atterraggio_A1b_vs_A2.mp4` (old light, r0) and `..._luceGASP.mp4` (r1; A1b panels are old light).
Critic r0 `CRITIC-A2.md`, review r1 `REVIEW-A2.md` (Fable high): FAIL. Size: width 0.93-1.07x target (pixel measures in REVIEW-A2), height ok to +11.
## Open gaps (REVIEW-A2, smallest fixes there)
1. colour-value: under GASP light dust is 3.3x the navy floor on dark (white smoke), vanishes on the near-white floor. Lower DarkSnow/brightness
   to ~1.8-2x floor keeping hue; darker ShadeColor so the underside is 0.80-0.85x floor on light.
2. straight dark edge / horizontal line in the dust base under the feet (f159-f163 dark): cause unknown (contact disc vs floor cut) -> UE assistant question.
3. right lobe reaches knee at +15..+26: size max 90 -> ~65 cm (ring ~30), all alpha 0 by +27.
## Traps
- `L_Test_Hub.umap` at b6e798c5 (claude/locomotion-finish-3) is saved with UE **5.9**: 5.8 shows "Impossibile trovare il mondo..." and hangs
  the capture (ESITO BLOCCATO). Here the look was applied with `-Pre hub_gasp_look` (script at module level; "failed: 'run'" is harmless) on the
  5.8 level, then the umap was restored (not committed). Dark/light surface for captures: `0.30,0.28,0.26` / `0.88,0.91,0.97`.
- UE AI assistant: request_access for UnrealEditor.exe declined -> not consulted. The 5.8 editor opens on a "Target Upgrade Required" dialog.
## Round 2 needs (skill §2, else a PASS is void)
A1b recaptured under GASP light (check out the 08840aa8 scripts + SmokeFB png), side view, VFX-off pass, fresh blind critic. Owner decision:
light test floor is near white (L 218) under GASP light: accept a subtle shaded read there, or lower its albedo?
Cost: ~10 $ Opus + 2 Fable high agents (~300k tokens) — estimate ~15 $ against 12 $ budget; stopped at this stable point.
