# GSTNCapture

Fast in-game review capture (owner 2026-11-24, decisions 1C + 2A). Tooling only: one Runtime module, denied in Shipping, nothing in the
game depends on it. Driven from Python by `Tools/GASNPPTest/pie_capture_move.py` with `PIE_FASTCAP=1` (set by
`Tools/AnimPipeline/gstn_anim_mcp/capture_move.py`, `fast=True` by default).

## Why

The old path (`_shoot()` in `pie_capture_move.py`) did, per frame and per view, in a row: `capture_scene()`, then
`export_render_target()`, which stops the engine until the GPU has finished, reads the picture back and compresses a PNG on the game
thread. CPU and GPU waited on each other: 4.4 fps with 7 views (`docs/notes/reviews/cp5-data/dair_base/capture.log`), ~9 min of
capture per animation round. Analysis: `handoffs/cattura-rapida-2026-11-23.md`, `handoffs/tempi-import-cattura-2026-11-23.md`.

## What it does

`UGSTN_FastCapture` (header `GSTN_FastCapture.h`, read it first):
- `Shoot(frame)` enqueues every view's scene capture and a GPU->CPU copy (`FRHIGPUTextureReadback`); nothing waits;
- `Pump()` (inside `Shoot`) collects finished copies on the render thread; PNGs are written by worker tasks to the same
  `<seq>/<view>/f<NNN>.png` files as before, so `capture_assert`, `pie_capture_grid`, `review_pack` are unchanged;
- pre-roll ring: frames before `Commit(press)` stay in memory; only `press - PreRollFrames` on reach the disk;
- `AddView(..., bReviewLighting)`: simple light on review views (no shadows, Lumen GI/reflections, AO, motion blur). Not unlit: the fill
  lights still shade arms and elbows. The game view keeps the full light.

The driver adds: window `PIE_FASTCAP_WINDOW=pre,post` around the press (+ the hitstop frames of a hit run), early end of a sequence once
its window and assert windows are logged, `t.MaxFPS 0` and no background CPU throttle, cameras that follow the mesh (not the actor: the
actor snaps, the mesh is smoothed; the K7/K8 slow motion "body snap" at v156-v158 was the camera). The simulation still steps 1/60 s per
rendered frame, so base and variant frames match (project rule 10). `capture_move.camera_jumps` warns about camera jumps after a capture.

A new capture chain means new pictures: the cache key carries `CAPTURE_CHAIN`, the base (e.g. a locked approved clip) is captured once
with it and then reused every round.

## Status

0.1, written in the cloud, **not compiled yet**. An editor without it falls back to the PNG export path (`FASTCAP: unavailable` in
`capture.log`). Not a GASP stock deviation (no locomotion or ABP change), so no `gasp-divergenze.md` row.
