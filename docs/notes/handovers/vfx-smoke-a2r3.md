# Handover smoke A2 r3 (2026-11-19, laptop, Opus high) - Fable PASS (narrow), to the owner
Branch `claude/project-thread-inmtkm` from fd3c17db. Round 3 of 3. Owner decision 1A (darker warmer smudge on the light floor) applied.
## Changes (scripts; uassets rebuilt by the capture -Pre)
- `setup_presentation_vfx_puff.py`: new material params `LitColorLight`, `ShadeColorLight`, `EdgeTintDark`, blended with the existing ones by
  floor brightness (same defaults = no change for other looks). SmokeReal/Haze: DarkSnow 0.26/0.22, DarkBrightness 1.0, bluer SnowColor/
  LitColor/ShadeColor, light floor warm LitColorLight (1,0.9,0.76), ShadeColorLight (0.44,0.36,0.28), EdgeTint (0.6,0.5,0.4), EdgeWidth 0.6,
  Brightness 0.58, OccStrength 0.65.
- `setup_presentation_vfx.py` land_ring: Skirt max 65 -> 50 cm, ring 30 -> 36 cm, wisps 65 -> 50 cm.
- Speed-up: Run-Timed.ps1 from claude/editor-speed-up (-unattended, no throttle); `vfx_v3_capture.ps1 -Clients` (default 0 = 1 PIE player),
  `pie_capture_vfx.py` reads PIE_CLIENTS (it waited for 3 worlds before). A capture now takes ~4 min (Run-Timed esito durata=258s).
- `Tools/GASNPPTest/vfx_dust_measure_bodyfree.py`: on/off diff with the OFF body masked and dilated (reads lower than Fable's own method).
## Attempts
Dark: 1 pass (Saved/VfxV3/a2r3/dark2). Light: pass 1 stayed blue-grey (the lit side, most of the puff, still had the cool LitColor) ->
pass 2 with LitColorLight (Saved/VfxV3/a2r3/light2). First dark run aborted: "pawns not ready" with 1 player (fixed).
## Reviews (docs/notes/reviews/vfx-v3/a2/r3/)
- CRITIC-A2r3.md (blind Opus): NOT YET both floors (light = soot, which is owner 1A; dark = separate blue puffs, faint).
- REVIEW-A2r3.md (Fable high): PASS narrow. Gap 1 dark closed (median 1.46-1.59x floor vs 2.4-2.8x, B/R 1.61-1.64), gap 2 light read closed
  (0.88-0.89x, area 4100-4600 px vs 2400-2900), gap 3 height closed vs A1b early frame (side top 147-150 px vs 138-149). Numbers: REVIEW file.
- Remaining non-blocking: dark dust bluer than floor (take back ~40 % of the blue shift), dark creases/pellets (OccStrength 0.5), one darker
  light-floor puff, dust spawns 2 frames before contact (game side, inherited).
## Owner (mp4 in video/, copied to project files)
Decisions listed in REVIEW-A2r3.md: smudge strength on light, blue tint on dark, separate puffs vs one merged skirt.
Unreal Assistant: `Tools/UE59/ask_unreal_assistant.py` pulled from claude/ue59-adoption-1 (not used this round; needs an open editor).
Cost estimate ~10 $.
