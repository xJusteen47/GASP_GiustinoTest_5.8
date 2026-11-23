# Rendering profiles: forward-rendering groundwork (2026-11-20)

Owner ask (11-20): drop Lumen for forward rendering, or at least lay the groundwork; also a light flicker seen in the
editor viewport before PIE.

## Default change (owner 11-20: 1A deferred without Lumen, 2A TSR)

`Config/DefaultEngine.ini`: `r.DynamicGlobalIlluminationMethod` 1 -> 0 (none), `r.ReflectionMethod` 1 -> 2 (SSR).
Unchanged: deferred, TSR (`r.AntiAliasingMethod=4`), mesh distance fields (DFAO sky occlusion), VSM off.
Undo: put both back to 1, or at runtime `gstn.Render.Profile 4`.
Watch on desktop: a post process volume with "Global Illumination Method = Lumen" overridden (GASP base level, test hub)
still turns Lumen on in that volume; `gstn.Render.Profile 2` forces it off everywhere. If that happens, untick the
override in the volume (map edit: only L_Test_Hub may be saved). Expect darker interiors/undersides: without GI the
ambient comes only from the sky light, so the sky light intensity / a fill light may need tuning on the test hub.

## What exists now

| Tool | Restart? | What it does |
|---|---|---|
| `gstn.Render.Profile 0..4` (console, editor and PIE) | no | 0 project default (no Lumen). 1 frozen auto exposure. 2 Lumen hard-off even under a volume that asks for it (SSR, DFAO). 3 = 2 + low-end savings (67 % screen, 2 cascades, 1024 shadows, no volumetric fog, SSAO). Back to 0 restores every value exactly. 4 = Lumen back (old default) for A/B. |
| `gstn.Render.Info` | no | Prints forward/deferred, GI, reflections, AA, MSAA, exposure, VSM, screen % |
| `Tools/Rendering/Apri editor FORWARD.bat` (`Open-Editor-Forward.ps1`, `-Msaa`) | yes | Opens the editor with `r.ForwardShading=1` via `-ini:` command-line overrides. Nothing written to config; the normal launcher brings deferred + Lumen back. |

Code: `Source/GASP_Giustino2/Private/Rendering/GSTN_RenderProfile.cpp` (no header, no UObjects, no assets).

## The flicker: does forward help?

Seen in the editor viewport before PIE, and after a PC restart the Unreal Assistant window no longer flickers, so the
scene flicker is a real rendering issue, not the streaming host. Likely causes, cheapest test first:
1. **Auto exposure hunting** (`ExtendDefaultLuminanceRange` on, GASP level post process): `gstn.Render.Profile 1`. If it stops, the fix is a fixed/narrow exposure range in the post process, not a renderer change.
2. **Lumen temporal noise** (screen probes, `TraceMeshSDFs` on, no Nanite, VSM off): with the new default Lumen is off; if it still flickers try `gstn.Render.Profile 2` (in case a volume forces Lumen), and `4` to see the old look. If it stops, Lumen is the cause, and forward would also fix it (forward has no Lumen), but so does deferred-without-Lumen, with no restart.
3. Neither stops it: shadow cascades swimming or TSR; test `r.AntiAliasingMethod 2` (TAA) / `0` and `r.Shadow.CSM.MaxCascades 1`.

So forward is not needed to fix the flicker; profile 2 answers whether removing Lumen does.

## What forward shading changes (r.ForwardShading=1)

- **Gains**: cheaper base pass on simple scenes and on Switch 2-class GPUs, MSAA available (sharp, no TSR ghosting on fast moves), better for VR-like clarity. Lumen is impossible, so lighting is fully stable.
- **Losses / differences**: no Lumen GI or reflections (sky light + DFAO/AO + reflection captures instead); screen-space effects that read the GBuffer are limited (SSR, SSGI, some AO); deferred decals become DBuffer decals; subsurface profile shading is simplified; at most 4 overlapping shadow-casting stationary lights; some post effects that read GBuffer channels (custom depth outlines are fine) may need material changes. With MSAA: no temporal anti-aliasing, so dithered fades and thin hair alias.
- **Cost**: a restart and a full shader recompile per mode (first forward launch: tens of minutes; DDC then keeps both). A permanent switch = one line in `DefaultEngine.ini` + rebuilding lighting look (sky light, reflection captures, exposure) on L_Test_Hub and the GASP base level.
- **Switch 2**: the Switch 2 runs the desktop renderer; deferred without Lumen is also a valid target. Forward vs deferred-no-Lumen there should be measured, not assumed (NDA SDK, not checked here).

## Decisions (owner, 2026-11-20)
1. Default renderer: **A, deferred without Lumen** (applied above). Forward stays available via the launcher; revisit after a perf pass.
2. Anti-aliasing: **A, TSR** (unchanged).
