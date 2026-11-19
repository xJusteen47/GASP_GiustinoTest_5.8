# L3 dash: Smash-style puff, SPEC ONLY (no assets) - 2026-11-19

Owner via coordinator: "simple Smash-style puff VFX only as a spec". Rules that apply (memory + principi-vfx.md, owner 11-19):
real behaviour, style only in palette; gases GROW while fading to alpha 0, never shrink opaque; light dust "like snow";
readable on light and dark ground; ALL kits share it (DA_Locomotion_Default). Reuse the run stop dust system when it fits
(NS_GSTN_RunStopDust, locomotion-finish-3) as the base emitter instead of a new one.

## When (clip frames from 1, 60 fps; notify names are proposals)
| Notify | Frame | Where | What |
|---|---|---|---|
| GSTN_DashPuff_Burst | 4 | between the feet, on the ground | the load burst: 4-6 round puffs, low, spreading sideways and back |
| GSTN_DashPuff_Push | 7 | right ball (push foot), ground | 2-3 puffs kicked BACK (opposite the dash), slightly up |
| GSTN_DashPuff_Trail | 8-14 | where the push foot left the ground | the trail: 1 puff every 2 frames, left in the WORLD (not attached), so the dash pulls away from it |
| GSTN_DashPuff_Catch | 16 | left heel (lead foot contact) | 1-2 small puffs, half the size of the burst |

## Shape, size, life (relative to the body; height ~1.8 m)
- Puff = soft round volume, not a flat card: at least 2 layers (dense core, thin rim), random rotation, slight noise.
- Burst puff: born at 0.08 body heights wide, grows to ~0.25 while fading; life 0.35-0.45 s; peak opacity at 2-3 frames, then fades to alpha 0
  while still growing. Speed at birth 2-3 m/s outward, strong drag (stops within ~0.15 s), small rise.
- Trail puffs: born smaller (0.05), grow to ~0.18, life 0.3 s, almost no own speed (they stay where the foot was).
- Catch: like the burst at half size, life 0.25 s.
- Total on screen at the peak: ~10 puffs. No sparks, no ring, no light emission (it is dust, lit by the scene).

## Colour
- Dust tinted by the ground (light warm grey on our test floor), core slightly darker than the rim; never pure white on a light floor.
  Check on light and dark ground (owner captures always both).

## What not to do
- No opaque shrinking blobs; no PNG-flat billboards that face the camera with a hard edge (VFX v3 critique "big flat PNGs").
- No puff on frames 1-3 (the load must read first).

## Hook
- The clip's notifies are added by the desktop thread after import (AnimationLibrary.add_animation_notify_event on a track
  "GSTN_VFX"); the Niagara system is a separate VFX thread (Fable VFX review vs this spec and the SSBU grid, dossier dash-ganon.md).
