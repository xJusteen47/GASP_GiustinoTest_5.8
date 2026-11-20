# Rendering profiles: forward-rendering groundwork (2026-11-20)

Owner ask (11-20): drop Lumen for forward rendering, or at least lay the groundwork; also a light flicker seen in the
editor viewport before PIE. Nothing here changes the project defaults (deferred + Lumen in `Config/DefaultEngine.ini`).
Flipping the default is an owner decision (see "Decisions" below).

## What exists now

| Tool | Restart? | What it does |
|---|---|---|
| `gstn.Render.Profile 0..3` (console, editor and PIE) | no | 0 default. 1 Lumen + frozen auto exposure. 2 no Lumen, still deferred (no dynamic GI, SSR, DFAO). 3 = 2 + low-end savings (67 % screen, 2 cascades, 1024 shadows, no volumetric fog, SSAO). Back to 0 restores every value exactly. |
| `gstn.Render.Info` | no | Prints forward/deferred, GI, reflections, AA, MSAA, exposure, VSM, screen % |
| `Tools/Rendering/Apri editor FORWARD.bat` (`Open-Editor-Forward.ps1`, `-Msaa`) | yes | Opens the editor with `r.ForwardShading=1` via `-ini:` command-line overrides. Nothing written to config; the normal launcher brings deferred + Lumen back. |

Code: `Source/GASP_Giustino2/Private/Rendering/GSTN_RenderProfile.cpp` (no header, no UObjects, no assets).

## The flicker: does forward help?

Seen in the editor viewport before PIE, and after a PC restart the Unreal Assistant window no longer flickers, so the
scene flicker is a real rendering issue, not the streaming host. Likely causes, cheapest test first:
1. **Auto exposure hunting** (`ExtendDefaultLuminanceRange` on, GASP level post process): `gstn.Render.Profile 1`. If it stops, the fix is a fixed/narrow exposure range in the post process, not a renderer change.
2. **Lumen temporal noise** (screen probes, `TraceMeshSDFs` on, no Nanite, VSM off): `gstn.Render.Profile 2`. If it stops, Lumen is the cause, and forward would also fix it (forward has no Lumen), but so does deferred-without-Lumen, with no restart.
3. Neither stops it: shadow cascades swimming or TSR; test `r.AntiAliasingMethod 2` (TAA) / `0` and `r.Shadow.CSM.MaxCascades 1`.

So forward is not needed to fix the flicker; profile 2 answers whether removing Lumen does.

## What forward shading changes (r.ForwardShading=1)

- **Gains**: cheaper base pass on simple scenes and on Switch 2-class GPUs, MSAA available (sharp, no TSR ghosting on fast moves), better for VR-like clarity. Lumen is impossible, so lighting is fully stable.
- **Losses / differences**: no Lumen GI or reflections (sky light + DFAO/AO + reflection captures instead); screen-space effects that read the GBuffer are limited (SSR, SSGI, some AO); deferred decals become DBuffer decals; subsurface profile shading is simplified; at most 4 overlapping shadow-casting stationary lights; some post effects that read GBuffer channels (custom depth outlines are fine) may need material changes. With MSAA: no temporal anti-aliasing, so dithered fades and thin hair alias.
- **Cost**: a restart and a full shader recompile per mode (first forward launch: tens of minutes; DDC then keeps both). A permanent switch = one line in `DefaultEngine.ini` + rebuilding lighting look (sky light, reflection captures, exposure) on L_Test_Hub and the GASP base level.
- **Switch 2**: the Switch 2 runs the desktop renderer; deferred without Lumen is also a valid target. Forward vs deferred-no-Lumen there should be measured, not assumed (NDA SDK, not checked here).

## Decisions (owner)
1. Default renderer: A keep deferred, Lumen off by default (profile 2 becomes the ini default) / B forward by default / C keep Lumen until a perf pass. Recommended: A after profile tests, B once the art look is rebuilt.
2. Anti-aliasing with forward: A TSR / B MSAA x8. Recommended A for now (B ghosting-free but noisy on dither).
