# Handover smoke A2 r6 (2026-11-19, laptop, Opus) - Fable PASS (narrow) after 1 fix round, to the owner
Branch `claude/project-thread-65g9ca` from 932a15ef. Owner on r5: "still looks fake" + DK clip; study `docs/notes/vfx-studies/dk-clip-2059/`.
Owner 21:02Z: "never shrinks" = smoke/dust only; solid grit may shrink.
## Changes
- C++ `UGSTN_VfxAuthoringLibrary::SetLightRenderer` (one light per particle, radius, exponent, affects translucency, alpha = brightness).
- setup_presentation_vfx.py land_ring: Skirt `land_shockwave_v` (13 x 16-40 cm, Disc Coverage 0.35) + Lumps `land_lumps` (5 x 30-50 cm,
  Spawn Time 0.025) + Wisps; all end 0.42-0.45 s. Grit `land_grit` 12 x 2-5 cm radial ~35 deg up, 450 cm/s, gravity -2000 (MI GritLit).
  Flash `_flash`: Niagara light 300 cm, (54,40,26), 25 cm up, 0.05 s, ScaleColor off, MegaLights off. `GSTN_DUST_LIT=0` = old unlit looks.
- setup_presentation_vfx_puff.py: master `M_GSTN_VFX_DustLit` (Default Lit translucent, Volumetric Directional, flipbook normal with
  NormalScale (1,-1,1), directional intensity 2.5, EmissiveLift); MIs DustLit / DustLitHaze / GritLit (r3 colours, retuned brightness).
- `Tools/GAS/pre_ask_unreal_assistant.py`: ask the Unreal Assistant from a capture editor (`-Pre ...,pre_ask_unreal_assistant`, env
  ASK_FILE/ASK_OUT). Reply on lit dust: `docs/notes/reviews/vfx-v3/a2/r6/ASSISTANT-lit-dust.md` (Q2 on flash units was not answered in time).
## Captures (vfx_v3_capture.ps1 -Seq jump -Wide 1920x1080 -Views side,game, -Pre setup_presentation_vfx_puff,setup_presentation_vfx,hub_gasp_look;
surfaces dark 0.30,0.28,0.26 / light 0.88,0.91,0.97; ESITO OK in each run.log.esito)
b1 dark (flash x1/3, grit white floating), b2 dark+light (reviewed), c dark+light (final). Saved/VfxV3/a2r6/{b1,b2,c}.
## Reviews (docs/notes/reviews/vfx-v3/a2/r6/)
CRITIC-A2r6.md (blind Opus on b2): NOT YET. REVIEW-A2r6.md (Fable high): b2 FAIL 5 items (grit vertical/hanging, late erosion, sun side not
shown, flash weak/late, dark value up); fix round = attempt 1 of 2; "r6c recheck" section: PASS narrow (grit, never-shrink 76 % area at
1.51x, flash 2.9-3.3x dark / 1.10x light, dark median 1.73-1.87x). Owner videos video/A2r6_atterraggio_{dark,light}[_vicino].mp4 (from c).
## Open (non-blocking)
- Lit side not proven: L/R 0.90-1.00, no 180 deg light-flip test; normal sign (NormalScale) unverified. Do the flip test first next time.
- Flash cuts off in 1 frame (want a 3-4 frame tail), legs only show a thigh patch; grit still round dots (want shards with spin);
  dark cloud thinner than r5d (area ~5500-6300 px vs 9900, try DarkBrightness 1.25); depth fade edge on the light floor.
- Critic points not addressed: motion stretch early (DK fix 4), richer flipbook (fix 6), hard early contact disc at f156.
- No unlit r6 comparison pass (GSTN_DUST_LIT=0), no OFF pass, no low preset, no perf numbers. Hub umap restored after captures.
Cost estimate ~11 $ (1 build, 5 captures, critic, Fable review + recheck).
